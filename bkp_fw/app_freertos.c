/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "app_lorawan.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LmHandler.h"          // LmHandlerJoin y ACTIVATION_TYPE_OTAA
#include "stm32wlxx_nucleo.h"   // BSP_LED_Toggle
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
/* Definitions for adcTask */
osThreadId_t adcTaskHandle;
const osThreadAttr_t adcTask_attributes = {
  .name = "adcTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 256 * 4
};
/* Definitions for accelTask */
osThreadId_t accelTaskHandle;
const osThreadAttr_t accelTask_attributes = {
  .name = "accelTask",
  .priority = (osPriority_t) osPriorityLow1,
  .stack_size = 256 * 4
};
/* Definitions for bmeTask */
osThreadId_t bmeTaskHandle;
const osThreadAttr_t bmeTask_attributes = {
  .name = "bmeTask",
  .priority = (osPriority_t) osPriorityLow2,
  .stack_size = 256 * 4
};
/* Definitions for inaTask */
osThreadId_t inaTaskHandle;
const osThreadAttr_t inaTask_attributes = {
  .name = "inaTask",
  .priority = (osPriority_t) osPriorityLow3,
  .stack_size = 256 * 4
};
/* Definitions for payloadTask */
osThreadId_t payloadTaskHandle;
const osThreadAttr_t payloadTask_attributes = {
  .name = "payloadTask",
  .priority = (osPriority_t) osPriorityLow4,
  .stack_size = 384 * 4
};
/* Definitions for powerTask */
osThreadId_t powerTaskHandle;
const osThreadAttr_t powerTask_attributes = {
  .name = "powerTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 384 * 4
};
/* Definitions for loraTask */
osThreadId_t loraTaskHandle;
const osThreadAttr_t loraTask_attributes = {
  .name = "loraTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for payloadQ */
osMessageQueueId_t payloadQHandle;
const osMessageQueueAttr_t payloadQ_attributes = {
  .name = "payloadQ"
};
/* Definitions for colaPower */
osMessageQueueId_t colaPowerHandle;
const osMessageQueueAttr_t colaPower_attributes = {
  .name = "colaPower"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
extern void StartAdcTask(void *argument);
extern void StartAccelTask(void *argument);
extern void StartBmeTask(void *argument);
extern void StartInaTask(void *argument);
extern void StartPayloadTask(void *argument);
extern void StartPowerTask(void *argument);
void StartLoraTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
		DWT->CYCCNT = 0;
}

__weak unsigned long getRunTimeCounterValue(void)
{
return DWT->CYCCNT;
}
/* USER CODE END 1 */

/* USER CODE BEGIN PREPOSTSLEEP */
__weak void PreSleepProcessing(uint32_t *ulExpectedIdleTime)
{
/* place for user code */
}

__weak void PostSleepProcessing(uint32_t *ulExpectedIdleTime)
{
/* place for user code */
}
/* USER CODE END PREPOSTSLEEP */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of payloadQ */
  payloadQHandle = osMessageQueueNew (32, 12, &payloadQ_attributes);

  /* creation of colaPower */
  colaPowerHandle = osMessageQueueNew (4, 2, &colaPower_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of adcTask */
  adcTaskHandle = osThreadNew(StartAdcTask, NULL, &adcTask_attributes);

  /* creation of accelTask */
  accelTaskHandle = osThreadNew(StartAccelTask, NULL, &accelTask_attributes);

  /* creation of bmeTask */
  bmeTaskHandle = osThreadNew(StartBmeTask, NULL, &bmeTask_attributes);

  /* creation of inaTask */
  inaTaskHandle = osThreadNew(StartInaTask, NULL, &inaTask_attributes);

  /* creation of payloadTask */
  payloadTaskHandle = osThreadNew(StartPayloadTask, NULL, &payloadTask_attributes);

  /* creation of powerTask */
  powerTaskHandle = osThreadNew(StartPowerTask, NULL, &powerTask_attributes);

  /* creation of loraTask */
  loraTaskHandle = osThreadNew(StartLoraTask, NULL, &loraTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LoRaWAN */
  /* Rev.0 (jul-2026): el subsistema RF del WL55 no responde al SPI.
   * MX_LoRaWAN_Init dispara MBMUXIF_SystemInit que salta a Error_Handler
   * y cuelga TODO el RTOS con __disable_irq + while(1). Consecuencia:
   * StartPayloadTask nunca arranca y UART_Link_Init nunca se llama,
   * asi que la telemetria por USART1 no funciona.
   *
   * Se comenta la llamada. El unico coste es que el M0+ no arranca y no
   * hay LoRa; la telemetria pasa integramente por USART1 al PC (ver
   * uart_link.c y tools/gateway_sim.py).
   *
   * AVISO: si regeneras el .ioc, CubeMX volvera a poner esta linea fuera
   * del USER CODE. Para hacerlo permanente hay que quitar el flag
   * "LoRaWAN Init" de defaultTask en el .ioc, o directamente borrar
   * defaultTask desde CubeMX. */
  /* MX_LoRaWAN_Init(); */
  /* USER CODE BEGIN StartDefaultTask */
  /* Init LoRa completo tambien comentado por el mismo motivo. Cuando la
   * Rev.1 tenga el subsistema RF operativo, descomentar los tres pasos:
   *   HAL_PWREx_ReleaseCore(PWR_CORE_CPU2);
   *   MX_LoRaWAN_Init();
   *   LmHandlerJoin(ACTIVATION_TYPE_OTAA, true);
   */

  /* Heartbeat visual: LED_BLUE (PB15) parpadea cada 500 ms. En Rev.0 no
   * hay LED fisico pero el pin PB15 esta libre en el .ioc y no colisiona
   * con nada, asi que el toggle es inofensivo. Sirve como sanity check
   * si en algun momento se conecta un LED al pin.
   *
   * El HELLO_UART de debug se retira: ya confirmamos que el pipeline
   * UART_Link_Send funciona. La telemetria real (INA/BME/ADC) sale por
   * StartPayloadTask cuando el PayloadFSM llama a LoRa_SendPayload. */
  for(;;)
  {
    BSP_LED_Toggle(LED_BLUE);
    osDelay(500);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartLoraTask */
/**
* @brief Function implementing the loraTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLoraTask */
__weak void StartLoraTask(void *argument)
{
  /* USER CODE BEGIN StartLoraTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartLoraTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
