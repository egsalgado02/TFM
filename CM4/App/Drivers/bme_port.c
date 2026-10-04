/*
 * bme_port.c - driver del BME680 (temperatura, humedad y presión, sin gas).
 *
 * Es la implementación real de bme_port.h. El orden en el que la FSM
 * (bme_fsm.c) llama a estas funciones en cada ciclo es:
 *
 *   SetPowerGPIO(true)     alimenta el sensor (PA15, PWR_BME)
 *   StartBootTimer         espera 5 ms a que arranque
 *   CheckStatusI2C         comprueba el chip ID (0x61) y, la primera vez,
 *                          lee los coeficientes de calibración
 *   TriggerForcedReading   pide una medida en modo forzado
 *   StartMedidaTimer       espera 20 ms a que termine la conversión
 *   IsDataReady            mira el bit "measuring" de EAS_STATUS_0
 *   ReadData               lee los datos crudos y los compensa
 *
 * Las fórmulas de compensación son las enteras del driver oficial de
 * Bosch (datasheet BME680 Rev 1.7, apartados 3.3 a 3.5). El M4 tiene FPU,
 * pero la versión entera es la que Bosch da como referencia y así el
 * resultado sale igual que en su driver.
 *
 * Todos los accesos al bus van con el lock de I2C2, que comparte con el
 * LIS2DW12 y el INA238 interno. La HAL no reparte el bus entre tareas por
 * sí sola: si dos tareas leen a la vez, las transferencias se mezclan.
 */

#include "bme_port.h"
#include "bme_fsm.h"
#include "main.h"                     /* PWR_BME_Pin, PWR_BME_GPIO_Port (labels del .ioc) */
#include "stm32wlxx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "cmsis_os.h"
#include "i2c2_lock.h"                /* lock del bus I2C2 (sensores de la placa) */
#include <string.h>

/* ================================================================
 * Dirección y registros del BME680 (datasheet Bosch Rev 1.7)
 * ================================================================ */
#define BME680_I2C_ADDR       (0x76 << 1)   /* SDO a GND en la placa */

#define REG_CHIP_ID           0xD0   /* debe leer 0x61 */
#define REG_RESET             0xE0   /* escribiendo 0xB6 se resetea */
#define REG_CTRL_HUM          0x72   /* osrs_h<2:0> */
#define REG_CTRL_MEAS         0x74   /* osrs_t<7:5> | osrs_p<4:2> | mode<1:0> */
#define REG_CONFIG            0x75   /* t_sb<7:5> | filter<4:2> | spi3w_en<0> */
#define REG_EAS_STATUS_0      0x1D   /* bit5 measuring | bit4 gas_measuring | ... */
#define REG_PRESS_MSB         0x1F   /* 8 bytes seguidos: P_MSB P_LSB P_XLSB T_MSB T_LSB T_XLSB H_MSB H_LSB */
#define REG_CALIB_PART1       0x89   /* 25 bytes */
#define REG_CALIB_PART2       0xE1   /* 16 bytes */

/* Sobremuestreo x2 en las tres magnitudes. La conversión dura unos 14 ms,
 * de ahí los 20 ms del temporizador de medida.
 * En la Rev.0 presión y humedad salen a cero aunque el chip acepta bien
 * esta configuración: sus elementos sensores están dañados (el puerto del
 * encapsulado se contaminó al limpiar la placa con isopropanol). Solo la
 * temperatura es válida. */
#define CTRL_HUM_OSRS_1       0x02   /* osrs_h = x2 */
#define CTRL_MEAS_OSRS_TP     ((2u<<5) | (2u<<2))  /* osrs_t=x2, osrs_p=x2 */
#define CTRL_MEAS_MODE_FORCED 0x01

/* ================================================================
 * Cosas que vienen de fuera (app_tasks.c y CubeMX)
 * ================================================================ */
extern I2C_HandleTypeDef hi2c2;
extern bme_fsm_t         mi_bme;
extern osThreadId_t      bmeTaskHandle;

/* ================================================================
 * Coeficientes de calibración. Se leen una sola vez, la primera vez que
 * el chip contesta. Los nombres son los del datasheet.
 * ================================================================ */
static struct {
    /* Temperatura */
    uint16_t par_t1;
    int16_t  par_t2;
    int8_t   par_t3;
    /* Presion */
    uint16_t par_p1;
    int16_t  par_p2;
    int8_t   par_p3;
    int16_t  par_p4;
    int16_t  par_p5;
    int8_t   par_p6;
    int8_t   par_p7;
    int16_t  par_p8;
    int16_t  par_p9;
    uint8_t  par_p10;
    /* Humedad */
    uint16_t par_h1;
    uint16_t par_h2;
    int8_t   par_h3;
    int8_t   par_h4;
    int8_t   par_h5;
    uint8_t  par_h6;
    int8_t   par_h7;
} s_cal;

static int32_t s_t_fine = 0;   /* sale del cálculo de T y lo usan P y H */
static bool    s_cal_ok = false;

/* ================================================================
 * Acceso a registros, cogiendo el lock del bus
 * ================================================================ */
static bool i2c_write_reg(uint8_t reg, uint8_t val)
{
    if (!I2C2_Lock_Take()) return false;
    bool ok = HAL_I2C_Mem_Write(&hi2c2, BME680_I2C_ADDR, reg,
                                I2C_MEMADD_SIZE_8BIT, &val, 1, 50) == HAL_OK;
    I2C2_Lock_Give();
    return ok;
}

static bool i2c_read_burst(uint8_t reg, uint8_t* buf, uint16_t len)
{
    if (!I2C2_Lock_Take()) return false;
    bool ok = HAL_I2C_Mem_Read(&hi2c2, BME680_I2C_ADDR, reg,
                               I2C_MEMADD_SIZE_8BIT, buf, len, 100) == HAL_OK;
    I2C2_Lock_Give();
    return ok;
}

/* ================================================================
 * Lectura de los coeficientes de calibración.
 * Los coeficientes están repartidos entre dos bloques de registros y
 * mezclados de una forma poco intuitiva (H1 y H2 comparten un byte, T1
 * está en el segundo bloque). El reparto está copiado del driver oficial
 * de Bosch y del datasheet (apartado 3.4, tabla 6), no deducido a mano.
 * ================================================================ */
static bool leer_calibracion(void)
{
    uint8_t p1[25];
    uint8_t p2[16];
    if (!i2c_read_burst(REG_CALIB_PART1, p1, sizeof(p1))) return false;
    if (!i2c_read_burst(REG_CALIB_PART2, p2, sizeof(p2))) return false;

    s_cal.par_t2  = (int16_t)((p1[2] << 8) | p1[1]);
    s_cal.par_t3  = (int8_t)p1[3];
    s_cal.par_p1  = (uint16_t)((p1[6] << 8) | p1[5]);
    s_cal.par_p2  = (int16_t)((p1[8] << 8) | p1[7]);
    s_cal.par_p3  = (int8_t)p1[9];
    s_cal.par_p4  = (int16_t)((p1[12] << 8) | p1[11]);
    s_cal.par_p5  = (int16_t)((p1[14] << 8) | p1[13]);
    s_cal.par_p7  = (int8_t)p1[15];
    s_cal.par_p6  = (int8_t)p1[16];
    s_cal.par_p8  = (int16_t)((p1[20] << 8) | p1[19]);
    s_cal.par_p9  = (int16_t)((p1[22] << 8) | p1[21]);
    s_cal.par_p10 = p1[23];

    s_cal.par_h2 = (uint16_t)((p2[0] << 4) | (p2[1] >> 4));
    s_cal.par_h1 = (uint16_t)((p2[2] << 4) | (p2[1] & 0x0F));
    s_cal.par_h3 = (int8_t)p2[3];
    s_cal.par_h4 = (int8_t)p2[4];
    s_cal.par_h5 = (int8_t)p2[5];
    s_cal.par_h6 = p2[6];
    s_cal.par_h7 = (int8_t)p2[7];
    s_cal.par_t1 = (uint16_t)((p2[9] << 8) | p2[8]);

    return true;
}

/* ================================================================
 * Compensación de Bosch, en aritmética entera.
 * Salidas: T en centésimas de grado, P en Pa y H en %RH * 1024.
 * ReadData las pasa luego a las décimas que usa la FSM.
 * ================================================================ */
static int32_t compensate_T(uint32_t adc_T)
{
    int32_t var1 = ((int32_t)adc_T >> 3) - ((int32_t)s_cal.par_t1 << 1);
    int32_t var2 = (var1 * (int32_t)s_cal.par_t2) >> 11;
    int32_t var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = ((var3) * ((int32_t)s_cal.par_t3 << 4)) >> 14;
    s_t_fine = var2 + var3;
    return ((s_t_fine * 5 + 128) >> 8);  /* grados * 100 */
}

static uint32_t compensate_P(uint32_t adc_P)
{
    int32_t var1 = (((int32_t)s_t_fine) >> 1) - 64000;
    int32_t var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)s_cal.par_p6) >> 2;
    var2 = var2 + ((var1 * (int32_t)s_cal.par_p5) << 1);
    var2 = (var2 >> 2) + ((int32_t)s_cal.par_p4 << 16);
    var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) * ((int32_t)s_cal.par_p3 << 5)) >> 3)
           + (((int32_t)s_cal.par_p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * (int32_t)s_cal.par_p1) >> 15;
    uint32_t p = (uint32_t)(1048576 - adc_P);
    p = (uint32_t)((p - (var2 >> 12)) * 3125);
    if (p >= (1u << 31)) p = (p / (uint32_t)var1) << 1;
    else                 p = (p << 1) / (uint32_t)var1;
    var1 = ((int32_t)s_cal.par_p9
            * (int32_t)(((p >> 3) * (p >> 3)) >> 13)) >> 12;
    var2 = ((int32_t)(p >> 2) * (int32_t)s_cal.par_p8) >> 13;
    int32_t var3_ = ((int32_t)(p >> 8) * (int32_t)(p >> 8) * (int32_t)(p >> 8) * (int32_t)s_cal.par_p10) >> 17;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + var3_ + ((int32_t)s_cal.par_p7 << 7)) >> 4));
    return p;   /* Pa */
}

static uint32_t compensate_H(uint16_t adc_H)
{
    int32_t T = ((s_t_fine * 5 + 128) >> 8);   /* grados * 100 */
    int32_t var1 = (int32_t)adc_H - (int32_t)((int32_t)s_cal.par_h1 << 4)
                   - (((T * (int32_t)s_cal.par_h3) / 100) >> 1);
    int32_t var2 = ((int32_t)s_cal.par_h2
                    * (((T * (int32_t)s_cal.par_h4) / 100)
                       + (((T * ((T * (int32_t)s_cal.par_h5) / 100)) >> 6) / 100)
                       + (1 << 14))) >> 10;
    int32_t var3 = var1 * var2;
    int32_t var4 = (int32_t)s_cal.par_h6 << 7;
    var4 = (var4 + ((T * (int32_t)s_cal.par_h7) / 100)) >> 4;
    int32_t var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    int32_t var6 = (var4 * var5) >> 1;
    int32_t H = (var3 + var6) >> 12;
    if (H > 102400) H = 102400;
    else if (H < 0) H = 0;
    return (uint32_t)H;   /* %RH * 1024 */
}

/* ================================================================
 * Temporizadores de un disparo: arranque, medida y sueño.
 * Cada callback marca su evento en la FSM y despierta a la tarea.
 * ================================================================ */
static void boot_cb(TimerHandle_t t) {
    (void)t;
    BmeFSM_NotifyBootTimer(&mi_bme);
    xTaskNotifyGive((TaskHandle_t)bmeTaskHandle);
}
static void medida_cb(TimerHandle_t t) {
    (void)t;
    BmeFSM_NotifyMedidaTimer(&mi_bme);
    xTaskNotifyGive((TaskHandle_t)bmeTaskHandle);
}
static void sleep_cb(TimerHandle_t t) {
    (void)t;
    BmeFSM_TriggerMedida(&mi_bme);
    xTaskNotifyGive((TaskHandle_t)bmeTaskHandle);
}

void BmePort_StartBootTimer(void)
{
    static TimerHandle_t timer = NULL;
    if (timer == NULL) {
        timer = xTimerCreate("BmeBoot", pdMS_TO_TICKS(5), pdFALSE, NULL, boot_cb);
        xTimerStart(timer, 0);
    } else {
        xTimerChangePeriod(timer, pdMS_TO_TICKS(5), 0);
    }
}

void BmePort_StartMedidaTimer(void)
{
    /* Con x2 en las tres magnitudes la conversión tarda unos 14 ms; 20 ms de margen */
    static TimerHandle_t timer = NULL;
    if (timer == NULL) {
        timer = xTimerCreate("BmeMedida", pdMS_TO_TICKS(20), pdFALSE, NULL, medida_cb);
        xTimerStart(timer, 0);
    } else {
        xTimerChangePeriod(timer, pdMS_TO_TICKS(20), 0);
    }
}

void BmePort_StartSleepTimer(uint32_t timeout_ms)
{
    static TimerHandle_t timer = NULL;
    if (timer == NULL) {
        timer = xTimerCreate("BmeSleep", pdMS_TO_TICKS(timeout_ms), pdFALSE, NULL, sleep_cb);
        xTimerStart(timer, 0);
    } else {
        xTimerChangePeriod(timer, pdMS_TO_TICKS(timeout_ms), 0);
    }
}

/* ================================================================
 * Funciones del port
 * ================================================================ */
void BmePort_SetPowerGPIO(bool estado)
{
    if (estado){
	HAL_GPIO_WritePin(PWR_BME_GPIO_Port, PWR_BME_Pin, GPIO_PIN_SET );
    }
    //if (!estado) s_cal_ok = false;   /* para releer la calibración al volver a alimentar */
}

bool BmePort_CheckStatusI2C(void)
{
    /* Cojo el lock solo para leer el chip ID. leer_calibracion ya lo coge
     * por dentro y el lock no se puede coger dos veces seguidas desde la
     * misma tarea: se quedaría bloqueada esperándose a sí misma. */
    uint8_t id = 0;
    if (!I2C2_Lock_Take()) return false;
    bool ok = HAL_I2C_Mem_Read(&hi2c2, BME680_I2C_ADDR, REG_CHIP_ID,
                               I2C_MEMADD_SIZE_8BIT, &id, 1, 50) == HAL_OK;
    I2C2_Lock_Give();
    if (!ok || id != 0x61) return false;

    if (!s_cal_ok) {
        if (!leer_calibracion()) return false;
        s_cal_ok = true;
    }
    return true;
}

/* Variables para depurar la lectura forzada. Guardan si las tres
 * escrituras han ido bien y lo que se relee de CTRL_HUM y CTRL_MEAS justo
 * después, para mirarlas con el depurador. Con ellas comprobé que el chip
 * se queda con la configuración que le mando, así que presión y humedad a
 * cero no es un problema del bus. */
volatile bool     s_dbg_w1 = false;
volatile bool     s_dbg_w2 = false;
volatile bool     s_dbg_w3 = false;
volatile bool     s_dbg_read_ok  = false;
volatile uint8_t  s_dbg_ctrl_hum  = 0xFF;
volatile uint8_t  s_dbg_ctrl_meas = 0xFF;

void BmePort_TriggerForcedReading(void)
{
    /* CTRL_HUM tiene que escribirse antes que CTRL_MEAS (datasheet 3.3.1):
     * el cambio de humedad solo se aplica al escribir CTRL_MEAS. */
    s_dbg_w1 = i2c_write_reg(REG_CTRL_HUM, CTRL_HUM_OSRS_1);
    s_dbg_w2 = i2c_write_reg(REG_CONFIG,   0x00);   /* sin filtro */
    s_dbg_w3 = i2c_write_reg(REG_CTRL_MEAS, CTRL_MEAS_OSRS_TP | CTRL_MEAS_MODE_FORCED);

    /* Releo los dos registros para ver qué se ha quedado escrito */
    uint8_t buf[1];
    bool ok_hum  = i2c_read_burst(REG_CTRL_HUM,  buf, 1);
    if (ok_hum)  s_dbg_ctrl_hum  = buf[0];
    bool ok_meas = i2c_read_burst(REG_CTRL_MEAS, buf, 1);
    if (ok_meas) s_dbg_ctrl_meas = buf[0];
    s_dbg_read_ok = ok_hum && ok_meas;
}

bool BmePort_IsDataReady(void)
{
    /* También con el lock: esta función acaba ejecutándose desde el
     * temporizador de medida y compite por el bus con las otras tareas. */
    uint8_t s = 0;
    if (!I2C2_Lock_Take()) return false;
    bool ok = HAL_I2C_Mem_Read(&hi2c2, BME680_I2C_ADDR, REG_EAS_STATUS_0,
                               I2C_MEMADD_SIZE_8BIT, &s, 1, 50) == HAL_OK;
    I2C2_Lock_Give();
    if (!ok) return false;
    /* bit 5 (measuring): 1 mientras convierte, 0 cuando ha terminado */
    return (s & (1u << 5)) == 0;
}

bool BmePort_ReadData(int16_t* temperatura, int16_t* humedad, int16_t* presion)
{
    if (!s_cal_ok) return false;
    uint8_t raw[8];
    if (!i2c_read_burst(REG_PRESS_MSB, raw, sizeof(raw))) return false;

    uint32_t adc_P = ((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4);
    uint32_t adc_T = ((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4);
    uint16_t adc_H = ((uint16_t)raw[6] << 8) | raw[7];

    int32_t  T_c100 = compensate_T(adc_T);      /* grados * 100 */
    uint32_t P_Pa   = compensate_P(adc_P);      /* Pa */
    uint32_t H_1024 = compensate_H(adc_H);      /* %RH * 1024 */

    /* Paso todo a décimas, que es lo que usa la FSM:
     *   temperatura en grados * 10 = T_c100 / 10
     *   humedad en %RH * 10        = H_1024 * 10 / 1024
     *   presión en hPa * 10        = Pa / 10 (1 hPa son 100 Pa) */
    *temperatura = (int16_t)(T_c100 / 10);
    *humedad     = (int16_t)((H_1024 * 10u) >> 10);
    *presion     = (int16_t)(P_Pa / 10u);
    return true;
}

uint32_t BmePort_GetTickMs(void)
{
    return (uint32_t)xTaskGetTickCount();
}
