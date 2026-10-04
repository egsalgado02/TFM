/**
 ******************************************************************************
 * @file    main_bringup.c
 * @brief   Firmware de puesta en marcha (bring-up) de la placa Rev.0.
 *
 * Es un programa aparte, que no usa nada del firmware de aplicación. Solo
 * se compila en la configuración "BringUp": ahí main.c está excluido (para
 * que no haya dos main) y en Debug y DebugStubs el excluido es este.
 *
 * Sirve para comprobar el hardware de la placa por partes, una fase detrás
 * de otra, sacando por UART el resultado de cada una. Todas las esperas
 * tienen límite, para que si algo no contesta el programa siga y no se
 * quede colgado.
 *
 * Fases:
 *   1. HAL, reloj HSI a 16 MHz, escalado de tensión Range 1 y USART2.
 *   2. Encender la alimentación de los sensores (PA15 BME, PA8 INA
 *      interno, PA1 INA de expansión).
 *   3. Arrancar I2C1 e I2C2.
 *   4. Barrido de direcciones 0x08 a 0x77 en los dos buses.
 *   5. WHO_AM_I del LIS2DW12 (0x18 en I2C2, tiene que dar 0x44).
 *   6. ID del BME680 (0x76 en I2C2, registro 0xD0, tiene que dar 0x61).
 *   7. Cristal de 32,768 kHz (ABS07-6, CL = 6 pF, condensadores de 6,8 pF).
 *   9. Interrupción del LIS2DW12 por movimiento en PC13.
 *  10. ADC en PB2 (ADC_IN4, divisor 1/2) para leer el supercondensador.
 *
 * No hay fase 8. Era la del cristal de 32 MHz, que va con la radio, y la
 * radio de la Rev.0 no contesta a ninguna orden por su bus interno. Por eso
 * la telemetría del firmware de aplicación va por UART al PC.
 *
 * En la fase 7 el tiempo máximo se cuenta con un contador a mano, sin usar
 * HAL_GetTick, mientras arranca el oscilador.
 *
 * Al terminar se queda parado. Para repetirlo, reset por NRST.
 *
 * I2C1 e I2C2 en la Rev.0:
 *   PB6/PB7 son I2C1 y PA11/PA12 son I2C2. Los sensores de la placa
 *   (LIS2DW12, BME680 e INA238 interno) están en PA11/PA12, así que
 *   contestan en I2C2. El INA238 de expansión y el mikroBUS están en
 *   PB6/PB7, en I2C1. Los drivers del firmware usan hi2c2 para los de la
 *   placa y hi2c1 para los de expansión.
 *
 * USART2 en la Rev.0:
 *   PA2 (TX) y PA3 (RX) salen por los pines 9 y 10 del encapsulado
 *   UFQFPN48 hasta el mikroBUS. USART1 (PA9/PA10) va al conector de
 *   expansión. Aquí se usa USART2 con un adaptador USB-UART pinchado en
 *   el mikroBUS.
 ******************************************************************************
 */

#include "main.h"
#include "i2c.h"
#include "gpio.h"
#include "adc.h"                /* hadc, MX_ADC_Init */
#include "stm32wlxx_ll_adc.h"   /* LL_ADC_SetCommonPathInternalCh, ... */
#include <stdio.h>
#include <stdint.h>

/* ============================================================
 * Direcciones I2C y registros de los sensores en la Rev.0
 * ============================================================ */
#define ADDR_LIS2DW12          0x18u
#define ADDR_BME680            0x76u
#define ADDR_INA238            0x40u

#define REG_LIS2DW12_WHO_AM_I  0x0Fu
#define WHO_AM_I_LIS2DW12      0x44u

#define REG_BME680_ID          0xD0u
#define ID_BME680              0x61u

/* Registros del LIS2DW12 para configurar wake-on-motion. */
#define REG_LIS2DW12_CTRL1     0x20u
#define REG_LIS2DW12_CTRL4_INT 0x23u
#define REG_LIS2DW12_CTRL7     0x3Fu
#define REG_LIS2DW12_WU_THS    0x34u

/* Configuraciones para el modo wake-on-motion. */
#define CFG_CTRL1_200HZ_HP     0x54u
#define CFG_CTRL4_INT1_WU      0x20u
#define CFG_CTRL7_INT_ON       0x20u
#define CFG_WU_THS_LOW         0x02u

/* ============================================================
 * USART2 configurado aquí mismo, sin depender del usart.c generado,
 * que a veces se queda a medias al regenerar con CubeMX.
 * ============================================================ */
UART_HandleTypeDef bu_huart2;

/* ============================================================
 * Prototipos
 * ============================================================ */
static void SystemClock_Config_BU(void);
static void BU_USART2_Init(void);
static void EncenderRailesSensores(void);
static void EscanearBus(I2C_HandleTypeDef *hi2c, const char *nombre_bus);
static void LeerWhoAmI_LIS2DW12(void);
static void LeerID_BME680(void);
static void ActivarLSE(void);
static void ConfigurarLIS2DW12_WakeUp_Y_Esperar(void);
static void LeerADC_Condensador(void);
static void Error_Handler_BU(void);

/* Lo pone a 1 la interrupción del INT1 del acelerómetro */
static volatile uint8_t s_accel_int_flag = 0;

/* ============================================================
 * printf sale por USART2
 * ============================================================ */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&bu_huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; ++i) {
        __io_putchar(ptr[i]);
    }
    return len;
}

/* ============================================================
 * main
 * ============================================================ */
int main(void)
{
    HAL_Init();
    /* Reloj lo más simple posible: HSI a 16 MHz sin PLL. Para UART, I2C y
     * ADC basta. El cristal de 32 MHz no se usa porque va con la radio, y la
     * radio de la Rev.0 no funciona. */
    SystemClock_Config_BU();

    /* Escalado de tensión en Range 1 (el normal, no el de bajo consumo).
     * El código de ejemplo de ST lo pone antes de tocar los osciladores: en
     * Range 2 el amplificador del LSE va más justo y hay cristales que no
     * arrancan. */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* MX_GPIO_Init deja PA15, PA8 y PA1 como salidas a 0 */
    MX_GPIO_Init();

    /* USART2: reloj, pines PA2/PA3 y HAL */
    BU_USART2_Init();

    printf("\r\n\r\n=== BringUp_WL55 arrancado ===\r\n");
    printf("HSI 16 MHz, USART2 115200 8N1 (PA2/PA3 = pines 9/10 UFQFPN48 = MikroBUS)\r\n\r\n");

    /* --- Fase 2: rails de sensores --- */
    printf("[Fase 2] Encendiendo rails de sensores...\r\n");
    EncenderRailesSensores();
    printf("  PA15 (PWR_BME)           -> HIGH\r\n");
    printf("  PA8  (PWR_INA_INTERNO)   -> HIGH\r\n");
    printf("  PA1  (PWR_INA_EXPANSION) -> HIGH\r\n");
    printf("  Esperando 100 ms para arranque de sensores...\r\n");
    HAL_Delay(100);

    /* --- Fase 3: init de I2C --- */
    printf("\r\n[Fase 3] Inicializando I2C1 e I2C2...\r\n");
    MX_I2C1_Init();
    MX_I2C2_Init();
    printf("  OK.\r\n");

    /* --- Fase 4: scanner I2C --- */
    printf("\r\n[Fase 4] Scanner I2C\r\n");
    EscanearBus(&hi2c1, "I2C1 (expansion)");
    EscanearBus(&hi2c2, "I2C2 (sensores internos)");

    /* --- Fase 5: LIS2DW12 --- */
    printf("\r\n[Fase 5] LIS2DW12 (acelerometro) en 0x%02X del I2C2\r\n",
           (unsigned)ADDR_LIS2DW12);
    LeerWhoAmI_LIS2DW12();

    /* --- Fase 6: BME680 --- */
    printf("\r\n[Fase 6] BME680 (T/H/P) en 0x%02X del I2C2\r\n",
           (unsigned)ADDR_BME680);
    LeerID_BME680();

    /* --- Fase 7: LSE --- */
    printf("\r\n[Fase 7] Activando LSE 32.768 kHz (cristal ABS07-6)...\r\n");
    ActivarLSE();

    /* Sin fase 8 (cristal de 32 MHz): la radio de la Rev.0 no responde.
     * La telemetría va por UART al PC. */

    /* --- Fase 9: EXTI del acelerometro --- */
    printf("\r\n[Fase 9] LIS2DW12 wake-on-motion + EXTI\r\n");
    ConfigurarLIS2DW12_WakeUp_Y_Esperar();

    /* --- Fase 10: ADC del supercondensador (PB2 = ADC_IN4, divisor 1/2) --- */
    printf("\r\n[Fase 10] ADC bateria/condensador (PB2, divisor 1:2)\r\n");
    LeerADC_Condensador();

    printf("\r\n=== Bring-up completado. Sistema en reposo. ===\r\n");

    /* Fin. Para repetir, reset por NRST. */
    while (1) {
        HAL_Delay(60000);
    }
}

/* ============================================================
 * SystemClock_Config_BU: HSI a 16 MHz, sin PLL y sin cristal de 32 MHz.
 * El de 32,768 kHz se arranca luego, en la fase 7.
 * ============================================================ */
static void SystemClock_Config_BU(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler_BU();
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK   |
                         RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1  |
                         RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler_BU();
    }
}

/* ============================================================
 * BU_USART2_Init: reloj, pines PA2/PA3 en AF7 y HAL.
 * ============================================================ */
static void BU_USART2_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    bu_huart2.Instance                    = USART2;
    bu_huart2.Init.BaudRate               = 115200;
    bu_huart2.Init.WordLength             = UART_WORDLENGTH_8B;
    bu_huart2.Init.StopBits               = UART_STOPBITS_1;
    bu_huart2.Init.Parity                 = UART_PARITY_NONE;
    bu_huart2.Init.Mode                   = UART_MODE_TX_RX;
    bu_huart2.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    bu_huart2.Init.OverSampling           = UART_OVERSAMPLING_16;
    bu_huart2.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    bu_huart2.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    bu_huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&bu_huart2) != HAL_OK) {
        Error_Handler_BU();
    }
    HAL_UARTEx_SetTxFifoThreshold(&bu_huart2, UART_TXFIFO_THRESHOLD_1_8);
    HAL_UARTEx_SetRxFifoThreshold(&bu_huart2, UART_RXFIFO_THRESHOLD_1_8);
    HAL_UARTEx_DisableFifoMode(&bu_huart2);
}

/* ============================================================
 * EncenderRailesSensores: pone a 1 los pines que alimentan el BME680
 * (PA15), el INA238 interno (PA8) y el INA238 de expansión (PA1).
 * ============================================================ */
static void EncenderRailesSensores(void)
{
    HAL_GPIO_WritePin(PWR_BME_GPIO_Port,     PWR_BME_Pin,     GPIO_PIN_SET);
    HAL_GPIO_WritePin(PWR_INA_INT_GPIO_Port, PWR_INA_INT_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PWR_EXP_GPIO_Port,     PWR_EXP_Pin,     GPIO_PIN_SET);
}

/* ============================================================
 * EscanearBus: prueba las direcciones 0x08 a 0x77 con
 * HAL_I2C_IsDeviceReady y saca por UART las que contestan.
 *
 * Si un dispositivo se queda a medias (por ejemplo, con la tensión
 * justa) puede dejar SDA a 0, y entonces HAL_I2C_IsDeviceReady se
 * queda esperando a que el bus se libere. Por eso, antes de barrer,
 * miro que SDA y SCL estén a 1. Si SDA está a 0, intento liberarlo
 * dando 9 pulsos de reloj a mano.
 * ============================================================ */
static void EscanearBus_Recovery(uint32_t sda_port, uint16_t sda_pin,
                                 uint32_t scl_port, uint16_t scl_pin)
{
    /* SDA y SCL como salidas en drenador abierto con pull-up */
    GPIO_TypeDef *gp_sda = (GPIO_TypeDef *)sda_port;
    GPIO_TypeDef *gp_scl = (GPIO_TypeDef *)scl_port;

    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = sda_pin; HAL_GPIO_Init(gp_sda, &g);
    g.Pin   = scl_pin; HAL_GPIO_Init(gp_scl, &g);

    /* 9 pulsos en SCL: con 9 ciclos de reloj cualquier esclavo termina el
     * byte que tuviera a medias y suelta SDA. */
    for (int i = 0; i < 9; ++i) {
        HAL_GPIO_WritePin(gp_scl, scl_pin, GPIO_PIN_RESET);
        for (volatile uint32_t d = 0; d < 200; d++) { __NOP(); }
        HAL_GPIO_WritePin(gp_scl, scl_pin, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < 200; d++) { __NOP(); }
    }
    /* Condición de STOP: SDA de 0 a 1 con SCL a 1 */
    HAL_GPIO_WritePin(gp_sda, sda_pin, GPIO_PIN_RESET);
    for (volatile uint32_t d = 0; d < 200; d++) { __NOP(); }
    HAL_GPIO_WritePin(gp_sda, sda_pin, GPIO_PIN_SET);
}

static void EscanearBus(I2C_HandleTypeDef *hi2c, const char *nombre_bus)
{
    printf("  %s: escaneando 0x08..0x77...\r\n", nombre_bus);

    /* Miro cómo está el bus antes de barrer.
     * PB6 = I2C1_SCL, PB7 = I2C1_SDA
     * PA11 = I2C2_SDA, PA12 = I2C2_SCL */
    GPIO_TypeDef *port_scl, *port_sda;
    uint16_t pin_scl, pin_sda;
    if (hi2c == &hi2c1) {
        port_scl = GPIOB; pin_scl = GPIO_PIN_6;
        port_sda = GPIOB; pin_sda = GPIO_PIN_7;
    } else {
        port_sda = GPIOA; pin_sda = GPIO_PIN_11;
        port_scl = GPIOA; pin_scl = GPIO_PIN_12;
    }

    /* Pongo los pines como entradas un momento para leerlos */
    GPIO_InitTypeDef ig = {0};
    ig.Mode  = GPIO_MODE_INPUT;
    ig.Pull  = GPIO_NOPULL;
    ig.Pin   = pin_scl; HAL_GPIO_Init(port_scl, &ig);
    ig.Pin   = pin_sda; HAL_GPIO_Init(port_sda, &ig);
    HAL_Delay(1);

    GPIO_PinState sda_st = HAL_GPIO_ReadPin(port_sda, pin_sda);
    GPIO_PinState scl_st = HAL_GPIO_ReadPin(port_scl, pin_scl);
    printf("    Estado bus antes: SDA=%s SCL=%s\r\n",
           sda_st == GPIO_PIN_SET ? "HIGH" : "LOW (mal)",
           scl_st == GPIO_PIN_SET ? "HIGH" : "LOW (mal)");

    if (sda_st == GPIO_PIN_RESET) {
        printf("    SDA enclavado bajo, intentando bus recovery (9 pulsos SCL)...\r\n");
        EscanearBus_Recovery((uint32_t)port_sda, pin_sda,
                             (uint32_t)port_scl, pin_scl);
        HAL_Delay(2);
        sda_st = HAL_GPIO_ReadPin(port_sda, pin_sda);
        printf("    Estado bus tras recovery: SDA=%s\r\n",
               sda_st == GPIO_PIN_SET ? "HIGH (recuperado)" : "LOW (imposible)");
        if (sda_st == GPIO_PIN_RESET) {
            printf("    Bus muerto, saltando barrido.\r\n");
            return;
        }
        /* Vuelvo a arrancar el I2C después de liberar el bus */
        if (hi2c == &hi2c1) { HAL_I2C_DeInit(hi2c); MX_I2C1_Init(); }
        else                { HAL_I2C_DeInit(hi2c); MX_I2C2_Init(); }
    } else {
        /* Devuelvo los pines a su función de I2C */
        if (hi2c == &hi2c1) { HAL_I2C_DeInit(hi2c); MX_I2C1_Init(); }
        else                { HAL_I2C_DeInit(hi2c); MX_I2C2_Init(); }
    }

    uint8_t encontrados = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        /* Aviso cada 16 direcciones, para ver dónde se queda si se cuelga */
        if ((addr & 0x0F) == 0x08) {
            printf("    [barriendo 0x%02X..0x%02X]\r\n",
                   (unsigned)addr, (unsigned)(addr + 15 <= 0x77 ? addr + 15 : 0x77));
        }
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 1, 3) == HAL_OK) {
            printf("    -> Responde 0x%02X\r\n", (unsigned)addr);
            ++encontrados;
        }
    }
    printf("  %s: %u dispositivos.\r\n", nombre_bus, encontrados);
}

/* ============================================================
 * LeerWhoAmI_LIS2DW12: WHO_AM_I del acelerómetro, tiene que dar 0x44.
 * ============================================================ */
static void LeerWhoAmI_LIS2DW12(void)
{
    uint8_t val = 0;
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2,
                                            (uint16_t)(ADDR_LIS2DW12 << 1),
                                            REG_LIS2DW12_WHO_AM_I,
                                            I2C_MEMADD_SIZE_8BIT,
                                            &val, 1, 100);
    if (st != HAL_OK) {
        printf("  ERROR HAL=%d al leer WHO_AM_I\r\n", (int)st);
        return;
    }
    printf("  WHO_AM_I = 0x%02X (esperado 0x%02X)\r\n",
           (unsigned)val, (unsigned)WHO_AM_I_LIS2DW12);
    printf(val == WHO_AM_I_LIS2DW12
           ? "  -> LIS2DW12 OK.\r\n"
           : "  -> VALOR INESPERADO.\r\n");
}

/* ============================================================
 * LeerID_BME680: ID del BME680, tiene que dar 0x61.
 * ============================================================ */
static void LeerID_BME680(void)
{
    uint8_t val = 0;
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2,
                                            (uint16_t)(ADDR_BME680 << 1),
                                            REG_BME680_ID,
                                            I2C_MEMADD_SIZE_8BIT,
                                            &val, 1, 100);
    if (st != HAL_OK) {
        printf("  ERROR HAL=%d al leer ID\r\n", (int)st);
        return;
    }
    printf("  ID = 0x%02X (esperado 0x%02X)\r\n",
           (unsigned)val, (unsigned)ID_BME680);
    printf(val == ID_BME680
           ? "  -> BME680 OK.\r\n"
           : "  -> VALOR INESPERADO.\r\n");
}

/* ============================================================
 * ActivarLSE: intenta arrancar el cristal de 32,768 kHz.
 *
 * Cristal ABS07-32.768kHz-6-T:
 *   - CL nominal 6 pF
 *   - potencia máxima 1 uW (típica 0,5 uW)
 *   - ESR máxima 70 kohm
 *
 * En la placa lleva dos condensadores de 6,8 pF (GRM1885) de PC14 y
 * PC15 a GND. CL efectiva = 6,8 / 2 + 2,5 pF de parásitas = 5,9 pF,
 * prácticamente los 6 pF del cristal.
 *
 * Pasos:
 *   1. Desbloquear el dominio de backup (DBP = 1).
 *   2. Reset del dominio de backup con BDRST.
 *   3. LSEDRV = 11, el drive más alto.
 *   4. LSEON = 1.
 *   5. LSESYSEN = 1, para que el LSE llegue al resto del sistema.
 *   6. Esperar LSERDY contando vueltas, con un máximo de 3 millones.
 * ============================================================ */
static void ActivarLSE(void)
{
    /* Desbloqueo el dominio de backup */
    HAL_PWR_EnableBkUpAccess();

    /* Reset del dominio de backup para empezar de cero: borra el RTC y
     * apaga el LSE si se había quedado encendido. */
    RCC->BDCR |=  RCC_BDCR_BDRST;
    for (volatile int i = 0; i < 100; ++i) { }
    RCC->BDCR &= ~RCC_BDCR_BDRST;

    /* LSEDRV = 11 antes de encenderlo: solo se puede cambiar con
     * LSEON = 0. */
    RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_LSEDRV_Msk)
              | (0x3ul << RCC_BDCR_LSEDRV_Pos);

    /* Enciendo el LSE y lo paso al resto del sistema */
    RCC->BDCR |= RCC_BDCR_LSEON;
    RCC->BDCR |= RCC_BDCR_LSESYSEN;

    /* Espera contando vueltas, sin depender de HAL_GetTick */
    volatile uint32_t contador = 0;
    while (!(RCC->BDCR & RCC_BDCR_LSERDY)) {
        contador++;
        if (contador > 3000000UL) {
            RCC->BDCR &= ~RCC_BDCR_LSEON;
            printf("  -> ERROR: LSE no arranca tras timeout.\r\n");
            printf("     Verificado: cristal ABS07-6 con condensadores 6.8 pF\r\n");
            printf("     estan bien dimensionados. Sin osciloscopio para\r\n");
            printf("     confirmar oscilacion. Posibles causas: contaminacion\r\n");
            printf("     PCB, cristal defectuoso, capacidad parasita de layout.\r\n");
            printf("     LSE no imprescindible: se puede sustituir por LSI para RTC.\r\n");
            return;
        }
    }
    printf("  -> LSE ready (tras %lu vueltas del contador). "
           "Cristal 32.768 kHz OK.\r\n", (unsigned long)contador);
}

/* ============================================================
 * ConfigurarLIS2DW12_WakeUp_Y_Esperar: pone el acelerómetro a
 * detectar movimiento y espera hasta 10 s a que salte la
 * interrupción (hay que darle un golpe a la placa). La EXTI13
 * (ACCEL_INT) ya está configurada en el .ioc, asignada al M4.
 * ============================================================ */
static void ConfigurarLIS2DW12_WakeUp_Y_Esperar(void)
{
    const struct { uint8_t reg, val; } cfg[] = {
        { REG_LIS2DW12_CTRL1,     CFG_CTRL1_200HZ_HP },
        { REG_LIS2DW12_CTRL4_INT, CFG_CTRL4_INT1_WU  },
        { REG_LIS2DW12_CTRL7,     CFG_CTRL7_INT_ON   },
        { REG_LIS2DW12_WU_THS,    CFG_WU_THS_LOW     },
    };

    for (unsigned i = 0; i < sizeof(cfg)/sizeof(cfg[0]); ++i) {
        HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c2,
                                                 (uint16_t)(ADDR_LIS2DW12 << 1),
                                                 cfg[i].reg,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 (uint8_t *)&cfg[i].val, 1, 100);
        if (st != HAL_OK) {
            printf("  ERROR escribiendo reg 0x%02X (HAL=%d)\r\n",
                   cfg[i].reg, (int)st);
            return;
        }
    }
    printf("  LIS2DW12 configurado en wake-on-motion.\r\n");
    printf("  Golpea la placa en los proximos 10 segundos...\r\n");

    s_accel_int_flag = 0;
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < 10000) {
        if (s_accel_int_flag) {
            printf("  -> EXTI disparada a los %lu ms. Acelerometro + EXTI OK.\r\n",
                   (unsigned long)(HAL_GetTick() - t0));
            return;
        }
        HAL_Delay(50);
    }
    printf("  -> Timeout 10 s sin EXTI.\r\n");
}

/* ============================================================
 * Interrupción del INT1 del acelerómetro (PC13)
 * ============================================================ */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ACCEL_INT_Pin) {
        s_accel_int_flag = 1;
    }
}

/* ============================================================
 * LeerADC_Condensador: fase 10.
 *
 * Mide la tensión del supercondensador de 25 F, que llega a PB2
 * (ADC_IN4) por un divisor 1/2: el ADC ve la mitad.
 *
 * VDDA no es fija: cuando el condensador baja de 3,3 V el reductor
 * deja de regular y VDDA baja con él. Por eso primero se mide VREFINT
 * y se calcula la VDDA real con la constante de calibración que ST
 * graba en fábrica en 0x1FFF75AA (VREFINT_CAL_ADDR):
 *
 *   VDDA_mV = 3000 * VREFINT_CAL / VREFINT_medido
 *
 * Luego se lee PB2 y se pasa a mV con esa VDDA:
 *
 *   V_PB2_mV      = VDDA_mV * raw_pb2 / 4095
 *   V_condensador = V_PB2_mV * 2   (por el divisor)
 *
 * VREFINT necesita al menos 4 us de muestreo, más de 64 ciclos a
 * 16 MHz. Lo que viene del .ioc es mucho menos, así que para VREFINT
 * se usan 160,5 ciclos y para PB2 el tiempo normal.
 * ============================================================ */
static uint16_t LeerCanal_ADC(uint32_t canal, uint32_t sampling_time)
{
    ADC_ChannelConfTypeDef s = {0};
    s.Channel      = canal;
    s.Rank         = ADC_REGULAR_RANK_1;
    s.SamplingTime = sampling_time;
    if (HAL_ADC_ConfigChannel(&hadc, &s) != HAL_OK) {
        return 0;
    }
    if (HAL_ADC_Start(&hadc) != HAL_OK) {
        return 0;
    }
    if (HAL_ADC_PollForConversion(&hadc, 100) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc);
        return 0;
    }
    uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc);
    (void)HAL_ADC_Stop(&hadc);
    return v;
}

static void LeerADC_Condensador(void)
{
    /* ADC con la configuración generada por CubeMX */
    MX_ADC_Init();

    /* Calibración. En el WL solo hay modo single-ended, así que la función
     * solo recibe el handle (en L4 o F3 lleva un segundo argumento). */
    if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK) {
        printf("  ERROR: calibracion del ADC fallo.\r\n");
        return;
    }
    printf("  ADC calibrado.\r\n");

    /* Conecto VREFINT al ADC */
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC),
                                   LL_ADC_PATH_INTERNAL_VREFINT);
    /* Espera de unos 10 us a que se estabilice (datasheet) */
    for (volatile uint32_t d = 0; d < 5000; d++) { __NOP(); }

    /* VREFINT, con tiempo de muestreo largo */
    uint16_t vrefint_raw = LeerCanal_ADC(ADC_CHANNEL_VREFINT,
                                         ADC_SAMPLETIME_160CYCLES_5);
    if (vrefint_raw == 0) {
        printf("  ERROR: no se pudo leer VREFINT.\r\n");
        return;
    }

    /* Constante de calibración de fábrica */
    uint16_t vrefint_cal = *((__IO uint16_t*)0x1FFF75AAUL);
    uint32_t vdda_mV = (3000UL * (uint32_t)vrefint_cal) / (uint32_t)vrefint_raw;

    printf("  VREFINT raw = %u (cal fabrica = %u)\r\n",
           (unsigned)vrefint_raw, (unsigned)vrefint_cal);
    printf("  VDDA real   = %lu mV (esperado ~3300 mV si LDO 3V3 en regulacion)\r\n",
           (unsigned long)vdda_mV);

    /* Desconecto VREFINT y paso a PB2 */
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC),
                                   LL_ADC_PATH_INTERNAL_NONE);

    /* PB2 (ADC_IN4), con el tiempo de muestreo normal */
    uint16_t pb2_raw = LeerCanal_ADC(ADC_CHANNEL_4, ADC_SAMPLETIME_7CYCLES_5);
    if (pb2_raw == 0) {
        printf("  ERROR: no se pudo leer PB2 (ADC_IN4).\r\n");
        return;
    }

    /* A mV en el pin */
    uint32_t v_pb2_mV = (vdda_mV * (uint32_t)pb2_raw) / 4095UL;

    /* Deshago el divisor: el condensador tiene el doble */
    uint32_t v_cap_mV = v_pb2_mV * 2UL;

    printf("  PB2 raw     = %u  (0..4095)\r\n", (unsigned)pb2_raw);
    printf("  V(PB2)      = %lu mV\r\n", (unsigned long)v_pb2_mV);
    printf("  V(condensador) = %lu mV = %lu.%03lu V\r\n",
           (unsigned long)v_cap_mV,
           (unsigned long)(v_cap_mV / 1000UL),
           (unsigned long)(v_cap_mV % 1000UL));
    printf("  (rango util del condensador: 2500-3600 mV)\r\n");
}

/* ============================================================
 * Gestión de errores
 * ============================================================ */
static void Error_Handler_BU(void)
{
    __disable_irq();
    while (1) { }
}

void Error_Handler(void)
{
    Error_Handler_BU();
}

/* ============================================================
 * Función vacía para que enlace: stm32wlxx_it.c llama a
 * BSP_PB_IRQHandler por los botones de la Nucleo, que en mi placa
 * no existen.
 * ============================================================ */
void BSP_PB_IRQHandler(Button_TypeDef Button)
{
    (void)Button;
}
