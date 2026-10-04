/*
 * adc_port.c - driver del ADC que mide la tensión del supercondensador.
 *
 * Es la implementación real de adc_port.h.
 *
 * Qué se mide: el canal 4 del ADC (PB2, ADC_IN4 en el .ioc). A esa pata
 * llega la tensión del supercondensador a través de un divisor 1/2 de dos
 * resistencias de 10 k. El divisor solo está conectado cuando está
 * encendido el raíl interno (PA8, el mismo pin que alimenta al INA238
 * interno). Quien lo enciende y lo apaga es adc_fsm.c, con
 * PowerRail_Request y PowerRail_Release alrededor de esta lectura.
 *
 * VDDA no es fija. El micro va alimentado por el reductor del BQ25570, y
 * cuando el condensador baja de 3,3 V el reductor deja pasar la tensión
 * del condensador tal cual, así que VDDA baja con él. Por eso en cada
 * lectura mido primero VREFINT y con la constante de calibración de
 * fábrica calculo la VDDA real antes de convertir la medida a mV.
 *
 * Tiempo de muestreo: CubeMX deja el grupo común 2 en 7,5 ciclos, que es
 * poco para VREFINT (pide al menos 4 us). Lo subo a 160,5 ciclos, unos
 * 10 us, escribiendo solo ese registro con la LL, sin reinicializar el ADC
 * ni tocar el adc.c generado.
 *
 * Temporizador: timer de FreeRTOS de un disparo, como en los demás
 * sensores. Al vencer marca la medida en la FSM y despierta a la tarea.
 */

#include "adc_port.h"
#include "adc_fsm.h"
#include "adc.h"               /* hadc, ADC_RESOLUTION_12B (CubeMX) */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "cmsis_os.h"
#include "stm32wlxx_ll_adc.h"  /* LL_ADC_SetSamplingTimeCommonChannels,
                                   LL_ADC_GetCommonPathInternalCh,
                                   LL_ADC_SetCommonPathInternalCh */
#include <stdio.h>             /* snprintf, solo para la traza por UART */
#include <string.h>            /* strlen, solo para la traza por UART */

/* Traza por el puerto de consola, igual que en app_tasks.c y main.c.
 * Es para depurar; en una versión final se quitaría. */
extern UART_HandleTypeDef hcom_uart[];
#define ADC_DBG(msg, len) HAL_UART_Transmit(&hcom_uart[0], (uint8_t*)(msg), (len), 50)

/* La instancia de la FSM y el handle de la tarea están en app_tasks.c y
 * app_freertos.c */
extern adc_fsm_t    mi_adc_fsm;
extern osThreadId_t adcTaskHandle;

/* ======================================================================
 * Callback del temporizador: toca medir, despierto a la tarea.
 * ====================================================================== */
static void sleep_cb(TimerHandle_t t)
{
    (void)t;
    AdcFSM_TriggerMedida(&mi_adc_fsm);
    xTaskNotifyGive((TaskHandle_t)adcTaskHandle);
}

/* ======================================================================
 * AdcPort_StartSleepTimer
 *   Arranca el temporizador con el periodo que se le pasa. La primera vez
 *   lo crea; las siguientes solo le cambia el periodo, que es lo que hace
 *   falta cuando cambia el modo de energía.
 * ====================================================================== */
void AdcPort_StartSleepTimer(uint32_t ms)
{
    static TimerHandle_t timer = NULL;

    if (timer == NULL) {
        timer = xTimerCreate("AdcSleep",
                             pdMS_TO_TICKS(ms),
                             pdFALSE,   /* un solo disparo */
                             NULL,
                             sleep_cb);
        xTimerStart(timer, 0);  /* xTimerCreate lo crea parado */
    } else {
        /* xTimerChangePeriod también lo vuelve a arrancar */
        xTimerChangePeriod(timer, pdMS_TO_TICKS(ms), 0);
    }
}

/* ======================================================================
 * AdcPort_LeerBateriaBloqueante
 *
 *   1. Espera a que el divisor esté bien encendido.
 *   2. La primera vez: calibración de offset y tiempo de muestreo.
 *   3. Lee VREFINT y calcula la VDDA real.
 *   4. Lee PB2 y lo pasa a mV del condensador.
 *
 * Cada conversión son unos 11 us (160,5 + 12,5 ciclos a 16 MHz), así que
 * leer bloqueando no molesta con periodos de medida de un segundo o más.
 * ====================================================================== */
bool AdcPort_LeerBateriaBloqueante(uint16_t* p_voltaje_mv)
{
    static bool init_done = false;

    /* Espera de 5 ms después de encender el divisor. La FSM enciende PA8 y
     * llama aquí enseguida; sin esta espera el interruptor del divisor
     * todavía no había terminado de conmutar al muestrear y salían lecturas
     * de 800 a 1500 mV con el condensador a unos 3,1 V. */
    vTaskDelay(pdMS_TO_TICKS(5));

    if (!init_done) {
        /* Calibración de offset del ADC. La función deshabilita el ADC,
         * calibra y lo deja listo. Basta con hacerla una vez (~1 ms). */
        if (HAL_ADCEx_Calibration_Start(&hadc) != HAL_OK) return false;

        /* Tiempo de muestreo del grupo común 2 a 160,5 ciclos */
        LL_ADC_SetSamplingTimeCommonChannels(ADC,
                                             LL_ADC_SAMPLINGTIME_COMMON_2,
                                             LL_ADC_SAMPLINGTIME_160CYCLES_5);
        init_done = true;
    }

    ADC_ChannelConfTypeDef ch = {0};
    ch.Rank         = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;   /* 160,5 ciclos, unos 10 us */

    /* ------------------------------------------------------------------
     * VREFINT -> VDDA real
     *
     * Al configurar este canal la HAL ya activa la referencia interna. Los
     * 10 us de muestreo cubren su tiempo de arranque (unos 10 us, RM0453
     * apartado 19).
     *
     * La macro usa la constante que ST graba en fábrica medida a
     * VDDA = 3300 mV. Si VDDA baja, la lectura de VREFINT sube y la macro
     * devuelve la VDDA que hay de verdad.
     * ------------------------------------------------------------------ */
    ch.Channel = ADC_CHANNEL_VREFINT;
    if (HAL_ADC_ConfigChannel(&hadc, &ch) != HAL_OK) return false;
    if (HAL_ADC_Start(&hadc)              != HAL_OK) return false;
    if (HAL_ADC_PollForConversion(&hadc, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc);
        return false;
    }
    uint32_t raw_vref = HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);

    uint32_t vdda_mv = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(raw_vref, ADC_RESOLUTION_12B);

    /* ------------------------------------------------------------------
     * PB2 (ADC_IN4) -> tensión del condensador
     * ------------------------------------------------------------------ */
    ch.Channel = ADC_CHANNEL_4;             /* PB2 = ADC_IN4 */
    if (HAL_ADC_ConfigChannel(&hadc, &ch) != HAL_OK) return false;
    if (HAL_ADC_Start(&hadc)              != HAL_OK) return false;
    if (HAL_ADC_PollForConversion(&hadc, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc);
        return false;
    }
    uint32_t raw_vbat = HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);

    /* V_cap = raw * 2 * VDDA / 4095. El 2 deshace el divisor 1/2.
     * El consumo del divisor lo corta la FSM al soltar el raíl interno. */
    *p_voltaje_mv = (uint16_t)((uint32_t)raw_vbat * 2UL * vdda_mv / 4095UL);

    /* Traza de depuración: VDDA y tensión medida en cada lectura */
    char buf[56];
    int  n = snprintf(buf, sizeof(buf), "[ADC] VDDA=%4lu mV  Vbat=%4u mV\r\n",
                      (unsigned long)vdda_mv, *p_voltaje_mv);
    if (n > 0) ADC_DBG(buf, (uint16_t)n);

    return true;
}

/* ======================================================================
 * Tiempo del sistema en ms, sacado del tick de FreeRTOS
 * ====================================================================== */
uint32_t AdcPort_GetTickMs(void)
{
    return (uint32_t)xTaskGetTickCount();
}
