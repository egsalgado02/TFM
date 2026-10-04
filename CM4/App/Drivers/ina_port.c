/*
 * ina_port.c - driver del INA238 (monitor de corriente y tensión de TI).
 *
 * Es la implementación real de ina_port.h. Hay dos chips iguales en la
 * placa y el mismo código sirve para los dos; se distinguen por hw_id:
 *   0 -> INA interno: bus I2C2, dirección 0x40, alimentado por PA8
 *        (RAIL_INTERNO, el mismo pin que conecta el divisor del ADC).
 *   1 -> INA de expansión: bus I2C1 (PB6/PB7), dirección 0x40, alimentado
 *        por PA1 (RAIL_EXPANSION, que también alimenta el mikroBUS).
 * Los dos tienen la misma dirección, pero como están en buses distintos
 * no chocan.
 *
 * Se usa en modo disparado: no está midiendo todo el rato, mide una vez
 * cuando se le pide. En cada ciclo la FSM (ina_fsm.c) llama a:
 *   TriggerForcedReading   pide una conversión de tensión de bus y shunt
 *   StartMedidaTimer       espera 5 ms a que termine
 *   IsDataReady            mira el bit de conversión lista en DIAG_ALRT
 *   ReadData               lee CURRENT y VBUS y los pasa a mA y mV
 *
 * Valores de la placa: shunt de 1 ohmio (1 %, 100 mW). El pico que se
 * espera es el de una transmisión LoRa, unos 30 mA, así que el fondo de
 * escala se pone en 50 mA.
 *
 * Tres comprobaciones que hace el driver:
 *   - En compilación, que el fondo de escala por el shunt quepa en el
 *     rango del ADC del chip (ver el _Static_assert). Si alguien cambia el
 *     shunt o la corriente máxima sin revisar esto, deja de compilar en
 *     vez de dar medidas saturadas.
 *   - Al arrancar, además del fabricante (MFG_ID), el modelo (DEV_ID). El
 *     INA228 tiene el mismo MFG_ID pero otros registros, y si se soldara
 *     uno por error los datos saldrían mal sin avisar.
 *   - Todos los accesos van con el lock de su bus (I2C2 lo comparte con
 *     el acelerómetro y el BME680; I2C1 es el de expansión).
 */

#include "ina_port.h"
#include "ina_fsm.h"
#include "main.h"                     /* PWR_INA_INT_Pin, PWR_EXP_Pin (nombres del .ioc) */
#include "stm32wlxx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "cmsis_os.h"
#include "i2c1_lock.h"                /* lock del bus I2C1 (expansión) */
#include "i2c2_lock.h"                /* lock del bus I2C2 (el de los sensores de la placa) */
#include <string.h>
#include <stdint.h>

/* ================================================================
 * Valores de la placa
 * ================================================================ */
#define SHUNT_MOHM              1000u      /* shunt de 1 ohmio = 1000 miliohmios */
#define MAX_CURRENT_A_x1000     50u        /* fondo de escala de 50 mA (30 mA de pico en TX) */

/* Número de chips: 0 = interno, 1 = expansión */
#define INA_INSTANCES           2u

/* Direcciones I2C. Los dos llevan A1 y A0 a GND, así que los dos son 0x40,
 * pero uno está en I2C2 y el otro en I2C1. */
#define INA_ADDR_INTERNO        (0x40 << 1)   /* bus I2C2 */
#define INA_ADDR_EXPANSION      (0x40 << 1)   /* bus I2C1 */

/* ================================================================
 * Registros del INA238 (datasheet TI SBOS926A)
 * ================================================================ */
#define REG_CONFIG              0x00
#define REG_ADC_CONFIG          0x01
#define REG_SHUNT_CAL           0x02
#define REG_VSHUNT              0x04
#define REG_VBUS                0x05   /* 16 bits sin signo */
#define REG_DIETEMP             0x06
#define REG_CURRENT             0x07   /* 16 bits con signo, en pasos de CURRENT_LSB */
#define REG_POWER               0x08
#define REG_DIAG_ALRT           0x0B   /* bit1 = conversión lista */
#define REG_MFG_ID              0x3E   /* debe leer 0x5449, "TI" en ASCII */
#define REG_DEV_ID              0x3F

/* CONFIG (0x00): RST = bit15, CONVDLY = bits 13..6, ADCRANGE = bit4 (0 = +-163,84 mV, 1 = +-40,96 mV) */
#define CONFIG_RESET            (1u << 15)
#define CONFIG_ADCRANGE_163MV   0u
#define CONFIG_DEFAULT          CONFIG_ADCRANGE_163MV

/* ADC_CONFIG (0x01): MODE[15:12], VBUSCT[11:9], VSHCT[8:6], VTCT[5:3], AVG[2:0]
 * MODE = 0x0B: una sola conversión de tensión de bus y de shunt, sin
 * temperatura. VBUSCT = VSHCT = 4: 1052 us por conversión. */
#define ADC_CONFIG_TRIG_BS      ((0x0Bu << 12) | (0x4u << 9) | (0x4u << 6))

/* Bit de conversión lista en DIAG_ALRT */
#define DIAG_CNVR_BIT           (1u << 1)

/* ================================================================
 * Cosas que vienen de fuera (app_tasks.c y CubeMX)
 * ================================================================ */
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern ina_fsm_t         mi_ina_int;   /* hw_id 0, I2C2, RAIL_INTERNO   */
extern ina_fsm_t         mi_ina_exp;   /* hw_id 1, I2C1, RAIL_EXPANSION */
extern osThreadId_t      inaTaskHandle;

/* FSM, bus y lock que le tocan a cada hw_id */
static ina_fsm_t* fsm_de(uint8_t hw_id)
{
    return (hw_id == 0u) ? &mi_ina_int : &mi_ina_exp;
}
static I2C_HandleTypeDef* bus_de(uint8_t hw_id)
{
    return (hw_id == 0u) ? &hi2c2 : &hi2c1;
}
static bool lock_take(uint8_t hw_id)
{
    return (hw_id == 0u) ? I2C2_Lock_Take() : I2C1_Lock_Take();
}
static void lock_give(uint8_t hw_id)
{
    if (hw_id == 0u) I2C2_Lock_Give(); else I2C1_Lock_Give();
}

/* ================================================================
 * Cálculo de SHUNT_CAL (datasheet del INA238, apartados 7.5.1.2 y 8.1.2)
 *
 *   CURRENT_LSB = I_max / 2^15
 *   SHUNT_CAL   = 819,2e6 * CURRENT_LSB * R_shunt
 *
 *   Con I_max = 50 mA y R_shunt = 1 ohmio:
 *     CURRENT_LSB = 0,050 / 32768 = 1,526 uA por paso
 *     SHUNT_CAL   = 819,2e6 * 1,526e-6 * 1 = 1250
 *
 *   Con ADCRANGE = 0 no hay que multiplicar por 4.
 *   Ojo: esta es la fórmula del INA238. La del INA226 es distinta y no
 *   vale para este chip.
 *
 *   Resolución: el chip mide en pasos de 1,526 uA, pero ReadData devuelve
 *   mA enteros, que es lo que usa la FSM. Por debajo de 1 mA sale 0, así
 *   que el consumo en reposo, de microamperios, no se ve desde aquí.
 * ================================================================ */
#define SHUNT_CAL_VALUE         1250u

/* CURRENT_LSB en nanoamperios, para no perder los decimales de 1,526 uA */
#define CURRENT_LSB_NA          1526u

/* Paso de VBUS: 3,125 mV (datasheet 7.6.1.2) */
#define VBUS_LSB_UV             3125u

/* La caída máxima en el shunt (I_max * R_shunt) tiene que caber en el
 * rango del ADC del chip con ADCRANGE = 0, +-163,84 mV. Uso 163 mV. */
_Static_assert((MAX_CURRENT_A_x1000 * SHUNT_MOHM / 1000u) <= 163u,
               "INA238: I_max * R_shunt supera el rango del ADC (163.84 mV con ADCRANGE=0)");

/* DEV_ID esperado: los 12 bits altos son el modelo y los 4 bajos la
 * revisión del silicio, que no se comprueba. */
#define INA238_DEV_ID_MASK      0xFFF0u
#define INA238_DEV_ID_VALUE     0x2380u

/* ================================================================
 * Si cada chip ya está configurado
 * ================================================================ */
static bool s_cfg_done[INA_INSTANCES] = { false, false };

/* ================================================================
 * Lectura y escritura de registros de 16 bits, con el lock del bus
 * ================================================================ */
static uint16_t addr_de(uint8_t hw_id)
{
    switch (hw_id) {
        case 0:  return INA_ADDR_INTERNO;
        case 1:  return INA_ADDR_EXPANSION;
        default: return INA_ADDR_INTERNO;
    }
}

static bool i2c_write_reg16(uint8_t hw_id, uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };   /* primero el byte alto */
    if (!lock_take(hw_id)) return false;
    bool ok = HAL_I2C_Mem_Write(bus_de(hw_id), addr_de(hw_id), reg,
                                I2C_MEMADD_SIZE_8BIT, buf, 2, 50) == HAL_OK;
    lock_give(hw_id);
    return ok;
}

static bool i2c_read_reg16(uint8_t hw_id, uint8_t reg, uint16_t* out)
{
    uint8_t buf[2];
    if (!lock_take(hw_id)) return false;
    bool ok = HAL_I2C_Mem_Read(bus_de(hw_id), addr_de(hw_id), reg,
                               I2C_MEMADD_SIZE_8BIT, buf, 2, 50) == HAL_OK;
    lock_give(hw_id);
    if (!ok) return false;
    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}

/* ================================================================
 * Configuración del chip: reset, rango del ADC y SHUNT_CAL
 * ================================================================ */
static bool configurar(uint8_t hw_id)
{
    /* Soft reset */
    if (!i2c_write_reg16(hw_id, REG_CONFIG, CONFIG_RESET)) return false;
    HAL_Delay(2);
    /* Rango de +-163,84 mV */
    if (!i2c_write_reg16(hw_id, REG_CONFIG, CONFIG_DEFAULT)) return false;
    /* SHUNT_CAL calculado arriba */
    if (!i2c_write_reg16(hw_id, REG_SHUNT_CAL, SHUNT_CAL_VALUE)) return false;
    return true;
}

/* ================================================================
 * Temporizadores de un disparo, uno de cada tipo por chip.
 * Los dos INA tienen el mismo periodo y despiertan a la vez; si
 * compartieran temporizador, el segundo rearmaría el del primero y se
 * perdería un aviso. El hw_id va guardado en el ID del temporizador para
 * saber en el callback a qué chip corresponde.
 * ================================================================ */
static void boot_cb(TimerHandle_t t) {
    uint8_t hw_id = (uint8_t)(uintptr_t)pvTimerGetTimerID(t);
    InaFSM_NotifyBootTimer(fsm_de(hw_id));
    xTaskNotifyGive((TaskHandle_t)inaTaskHandle);
}
static void medida_cb(TimerHandle_t t) {
    uint8_t hw_id = (uint8_t)(uintptr_t)pvTimerGetTimerID(t);
    InaFSM_NotifyMedidaTimer(fsm_de(hw_id));
    xTaskNotifyGive((TaskHandle_t)inaTaskHandle);
}
static void sleep_cb(TimerHandle_t t) {
    uint8_t hw_id = (uint8_t)(uintptr_t)pvTimerGetTimerID(t);
    InaFSM_TriggerMedida(fsm_de(hw_id));
    xTaskNotifyGive((TaskHandle_t)inaTaskHandle);
}

static void arm_oneshot(TimerHandle_t* slot, const char* name, uint32_t ms,
                        TimerCallbackFunction_t cb, uint8_t hw_id)
{
    if (*slot == NULL) {
        *slot = xTimerCreate(name, pdMS_TO_TICKS(ms), pdFALSE,
                             (void*)(uintptr_t)hw_id, cb);
        xTimerStart(*slot, 0);
    } else {
        xTimerChangePeriod(*slot, pdMS_TO_TICKS(ms), 0);
    }
}

static TimerHandle_t s_boot_t  [INA_INSTANCES] = { NULL, NULL };
static TimerHandle_t s_medida_t[INA_INSTANCES] = { NULL, NULL };
static TimerHandle_t s_sleep_t [INA_INSTANCES] = { NULL, NULL };

void InaPort_StartBootTimer(uint8_t hw_id)
{
    if (hw_id >= INA_INSTANCES) return;
    /* 100 ms después de encender el raíl. Con 5 ms el chip todavía no
     * contestaba y CheckStatusI2C fallaba. */
    arm_oneshot(&s_boot_t[hw_id], "InaBoot", 100u, boot_cb, hw_id);
}

void InaPort_StartMedidaTimer(uint8_t hw_id)
{
    if (hw_id >= INA_INSTANCES) return;
    /* Dos conversiones de 1052 us sin promediado: unos 2 ms. Espero 5 ms. */
    arm_oneshot(&s_medida_t[hw_id], "InaMedida", 5u, medida_cb, hw_id);
}

void InaPort_StartSleepTimer(uint8_t hw_id, uint32_t time_ms)
{
    if (hw_id >= INA_INSTANCES) return;
    arm_oneshot(&s_sleep_t[hw_id], "InaSleep", time_ms, sleep_cb, hw_id);
}

/* ================================================================
 * Funciones del port
 * ================================================================ */
bool InaPort_CheckStatusI2C(uint8_t hw_id)
{
    if (hw_id >= INA_INSTANCES) return false;

    /* Fabricante: 0x5449 ("TI") */
    uint16_t mfg = 0;
    if (!i2c_read_reg16(hw_id, REG_MFG_ID, &mfg)) return false;
    if (mfg != 0x5449) return false;

    /* Modelo: tiene que ser un INA238 y no un INA228 */
    uint16_t dev = 0;
    if (!i2c_read_reg16(hw_id, REG_DEV_ID, &dev)) return false;
    if ((dev & INA238_DEV_ID_MASK) != INA238_DEV_ID_VALUE) return false;

    /* Se configura la primera vez que el chip contesta. Ojo: al soltar el
     * raíl el chip se apaga y vuelve a sus valores de fábrica, y como
     * s_cfg_done no se borra, no se vuelve a configurar. */
    if (!s_cfg_done[hw_id]) {
        if (!configurar(hw_id)) return false;
        s_cfg_done[hw_id] = true;
    }
    return true;
}

void InaPort_TriggerForcedReading(uint8_t hw_id)
{
    if (hw_id >= INA_INSTANCES) return;
    i2c_write_reg16(hw_id, REG_ADC_CONFIG, ADC_CONFIG_TRIG_BS);
}

bool InaPort_IsDataReady(uint8_t hw_id)
{
    if (hw_id >= INA_INSTANCES) return false;
    uint16_t diag = 0;
    if (!i2c_read_reg16(hw_id, REG_DIAG_ALRT, &diag)) return false;
    return (diag & DIAG_CNVR_BIT) != 0;
}

bool InaPort_ReadData(uint8_t hw_id, int16_t* corriente, int16_t* voltaje)
{
    if (hw_id >= INA_INSTANCES) return false;

    uint16_t raw_i = 0;
    uint16_t raw_v = 0;
    if (!i2c_read_reg16(hw_id, REG_CURRENT, &raw_i)) return false;
    if (!i2c_read_reg16(hw_id, REG_VBUS,    &raw_v)) return false;

    /* CURRENT tiene signo, VBUS no */
    int32_t i_na = ((int32_t)(int16_t)raw_i) * (int32_t)CURRENT_LSB_NA;   /* nA */
    int32_t v_uv = ((int32_t)raw_v) * (int32_t)VBUS_LSB_UV;                 /* uV */

    /* Paso a mA y mV. En int32 cabe de sobra: 50 mA son 50 millones de nA. */
    *corriente = (int16_t)(i_na / 1000000);
    *voltaje   = (int16_t)(v_uv / 1000);
    return true;
}

uint32_t InaPort_GetTickMs(void)
{
    return (uint32_t)xTaskGetTickCount();
}
