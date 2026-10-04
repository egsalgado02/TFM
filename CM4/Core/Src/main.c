/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "i2c.h"
#include "app_lorawan.h"
#include "lptim.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_tasks.h"        // AppTasks_InitAll() y entry points de las tareas
#include <string.h>           // strlen para el mensaje inicial por UART
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

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
// Las instancias de las FSMs viven en app_tasks.c, no aquí.
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* Sin cristal de 32,768 kHz (LSE) en el arranque.
   *
   * El de la Rev.0 arranca cuando quiere, incluso con LSEDRIVE_HIGH y
   * reseteando el dominio de backup, así que lo he quitado todo lo que
   * dependía de él:
   *   - MX_LPTIM1_Init está comentado (no se usa el LPTIM1).
   *   - Fuera HAL_RCCEx_EnableMSIPLLMode (el reloj del sistema es el HSI).
   *   - En SystemClock_Config he quitado el LSE a mano del código
   *     generado. Si se regenera con CubeMX vuelve a aparecer y el arranque
   *     se queda colgado esperando al cristal: hay que desmarcar el LSE en
   *     el .ioc o volver a quitarlo.
   * En marcha no se nota: reloj del sistema HSI a 16 MHz, tick de FreeRTOS
   * con SysTick, base de tiempos de la HAL con TIM17 y los temporizadores
   * de los sensores son de FreeRTOS. Lo que se pierde es el bajo consumo
   * (tickless); para recuperarlo habría que usar LSI con LPTIM o el RTC.
   *
   * Apago el LSEON por si se ha quedado encendido de una sesión anterior:
   * el dominio de backup no se resetea porque lo mantiene el condensador. */
  HAL_PWR_EnableBkUpAccess();
  CLEAR_BIT(RCC->BDCR, RCC_BDCR_LSEON);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
//  MX_LPTIM1_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Sin HAL_RCCEx_EnableMSIPLLMode: necesita el LSE y además el reloj del
   * sistema es el HSI, así que calibrar el MSI no sirve de nada. */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* AppTasks_InitAll va aquí: después de MX_FREERTOS_Init, que crea las
   * colas y las tareas, y antes de osKernelStart. Si no se llama, las FSM
   * se quedan con la tabla de transiciones a NULL y el primer fsm_fire
   * salta a la dirección 0 y acaba en HardFault. */
  AppTasks_InitAll();

  /* Initialize leds */
  BSP_LED_Init(LED_BLUE);
  /* LED_GREEN de la Nucleo está en PA9, y en mi placa PA9 es la TX de
   * USART1 (EXP_TX), la del enlace serie con el PC. BSP_LED_Init(LED_GREEN)
   * pone PA9 como salida normal y le quita la función de UART que le había
   * dado MX_USART1_UART_Init: HAL_UART_Transmit se ejecuta pero por el pin
   * no sale nada. Por eso está comentado. Pasa lo mismo con los botones de
   * abajo. Si hace falta un LED, hay que ponerlo en otro pin libre, no
   * usar LED_GREEN del BSP. */
  // BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  /* Mi placa no tiene los botones SW1, SW2 y SW3 de la Nucleo. BSP_PB_Init
   * pone PA0, PA1 y PC6 como entradas con interrupción, y machaca la
   * configuración de salida que les da MX_GPIO_Init. Con SW2 se perdía PA1
   * (PWR_EXP): se quedaba como entrada flotante, a 1,29 V, que no llega
   * para alimentar el INA de expansión (necesita 2,7 V) ni para cerrar el
   * interruptor del dominio. Como los botones no existen, las tres llamadas
   * están comentadas.
   */
  //BSP_PB_Init(BUTTON_SW1, BUTTON_MODE_EXTI);
  //BSP_PB_Init(BUTTON_SW2, BUTTON_MODE_EXTI);
  //BSP_PB_Init(BUTTON_SW3, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Boot CPU2 */
  HAL_PWREx_ReleaseCore(PWR_CORE_CPU2);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  /* LSE quitado de OscillatorType a mano: el cristal arranca cuando quiere
   * y HAL_RCC_OscConfig acababa en Error_Handler por timeout. Si CubeMX
   * regenera esto, hay que volver a quitarlo o desmarcar el LSE en el .ioc.
   * Está explicado arriba, en USER CODE Init. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM17 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM17)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
