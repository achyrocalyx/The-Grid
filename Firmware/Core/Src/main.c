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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define MODULE_ID 25
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FRAME_DATA_LENGTH 589 // bytes
#define SWITCH_DATA_LENGTH 2 // bytes
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_rx;

/* USER CODE BEGIN PV */
uint8_t uart_rx_buffer[FRAME_DATA_LENGTH]; //rx buffer is 589 bytes
uint8_t uart_tx_buffer[SWITCH_DATA_LENGTH]; //tx buffer is 2 bytes

uint8_t microswitch[4] = {0}; //0 is off, 1 is on
uint8_t microswitch_off_next_cycle[4] = {0}; //set microswitch to be off on the next frame cycle

uint8_t microswitch_reported[4] = {0}; // reported values we send back to computer

uint8_t led_color_1[3]; // led color pixel [R val, G val, B val]
uint8_t led_color_2[3];
uint8_t led_color_3[3];
uint8_t led_color_4[3];

volatile uint8_t potentialMan[4] = {0}; // potentially microswitch may be on. slop variable to move while loop into main :(
volatile uint32_t timeStart[4] = {0};

volatile uint8_t resyncing = 0;
uint8_t sync_byte;

// state enums - idle, waiting, transmitted,
typedef enum {
	IDLE,
	WAITING,
	TRANSMITTED
} State;

typedef enum {
	RED,
	GREEN,
	BLUE
} Color;

State state = IDLE;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*== LED TIMER CHANNEL REFERENCE ==
PB1 -> TIM3_CH4 (R1)
PB0 -> TIM3_CH3 (G1)
PA7 -> TIM3_CH2 (B1)

PA6 -> TIM3_CH1 (R2)
PB6 -> TIM4_CH1 (G2)
PB7 -> TIM4_CH2 (B2)

PA2 -> TIM2_CH3 (R3)
PA1 -> TIM2_CH2 (G3)
PA0 -> TIM2_CH1 (B3)

PB8 -> TIM4_CH3 (R4)
PB9 -> TIM4_CH4 (G4)
PA3 -> TIM2_CH4 (B4) */

void command_led_1(uint8_t r, uint8_t g, uint8_t b) {
	led_color_1[RED] = r; // check protocol sheet for more information, sets colors to what is put in buffer (based on module ID)
	led_color_1[GREEN] = g;
	led_color_1[BLUE] = b;

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, r);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, g);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, b);
}

void command_led_2(uint8_t r, uint8_t g, uint8_t b) {
	led_color_2[RED] = r;
	led_color_2[GREEN] = g;
	led_color_2[BLUE] = b;

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, r);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, g);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, b);
}

void command_led_3(uint8_t r, uint8_t g, uint8_t b) {
	led_color_3[RED] = r;
	led_color_3[GREEN] = g;
	led_color_3[BLUE] = b;

	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, r);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, g);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, b);
}

void command_led_4(uint8_t r, uint8_t g, uint8_t b) {
	led_color_4[RED] = r;
	led_color_4[GREEN] = g;
	led_color_4[BLUE] = b;

	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, r);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, g);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, b);
}

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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_SWJ_NOJTAG();   // releases JTAG-only pins (PA15, PB3, PB4) for GPIO use, keeps SWD (PA13/PA14) alive for debugging
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART3_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  /* Enable PWM Timers */
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);

    HAL_TIM_Base_Start(&htim1);

    HAL_UART_Receive_DMA(&huart3, uart_rx_buffer, FRAME_DATA_LENGTH); // Initialize receiving

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // data enable low

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  // microswitch sensing
		if (potentialMan[0] == 1) {
			if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) {
				microswitch[0] = 0;
			}
			else if (HAL_GetTick() - timeStart[0] >= 10) {
				/* MICROSWITCH HAS BEEN PRESSED */
				microswitch_off_next_cycle[0] = 0;
				microswitch[0] = 1;
				microswitch_reported[0] = 1;
				potentialMan[0] = 0;
			}
		}
		if (microswitch[0] == 1 || microswitch_reported[0] == 1) {
			if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) {
				/* MICROSWITCH IS NOT PRESSED */
				microswitch[0] = 0;
				microswitch_off_next_cycle[0] = 1;
			}
		}

		if (potentialMan[1] == 1) {
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) {
				microswitch[1] = 0;
			}
			else if (HAL_GetTick() - timeStart[1] >= 10) {
				/* MICROSWITCH HAS BEEN PRESSED */
				microswitch_off_next_cycle[1] = 0;
				microswitch[1] = 1;
				microswitch_reported[1] = 1;
				potentialMan[1] = 0;
			}
		}
		if (microswitch[1] == 1 || microswitch_reported[1] == 1) {
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_RESET) {
				/* MICROSWITCH IS NOT PRESSED */
				microswitch[1] = 0;
				microswitch_off_next_cycle[1] = 1;
			}
		}

		if (potentialMan[2] == 1) {
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
				microswitch[2] = 0;
			}
			else if (HAL_GetTick() - timeStart[2] >= 10) {
				/* MICROSWITCH HAS BEEN PRESSED */
				microswitch_off_next_cycle[2] = 0;
				microswitch[2] = 1;
				microswitch_reported[2] = 1;
				potentialMan[2] = 0;
			}
		}
		if (microswitch[2] == 1 || microswitch_reported[2] == 1) {
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
				/* MICROSWITCH IS NOT PRESSED */
				microswitch[2] = 0;
				microswitch_off_next_cycle[2] = 1;
			}
		}

		if (potentialMan[3] == 1) {
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) {
				microswitch[3] = 0;
			}
			else if (HAL_GetTick() - timeStart[3] >= 10) {
				/* MICROSWITCH HAS BEEN PRESSED */
				microswitch_off_next_cycle[3] = 0;
				microswitch[3] = 1;
				microswitch_reported[3] = 1;
				potentialMan[3] = 0;
			}
		}
		if (microswitch[3] == 1 || microswitch_reported[3] == 1) {
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) {
				/* MICROSWITCH IS NOT PRESSED */
				microswitch[3] = 0;
				microswitch_off_next_cycle[3] = 1;
			}
		}

	  if (state == WAITING) {
		  if ((TIM1 -> CNT) > (300 + ((50 + 150) * (MODULE_ID - 1)))) {
			  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); // data enable high
			  /* Format tx data buffer */
			  uart_tx_buffer[0] = MODULE_ID;
			  uart_tx_buffer[1] = (microswitch_reported[3] << 3) | (microswitch_reported[2] << 2) | (microswitch_reported[1] << 1) | (microswitch_reported[0] << 0);

//			  HAL_UART_Transmit_IT(&huart3, uart_tx_buffer, SWITCH_DATA_LENGTH);
			  HAL_UART_Transmit(&huart3, uart_tx_buffer, SWITCH_DATA_LENGTH, 10);
			  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
			  state = TRANSMITTED;
		  }
	  }
	  if (TIM1 -> CNT > 13000 && state != IDLE) { // if timer > 13ms
		  __HAL_TIM_SET_COUNTER(&htim1,0); // re-start millisecond timer @ 0
		  HAL_UART_Receive_DMA(&huart3, uart_rx_buffer, FRAME_DATA_LENGTH); // restart listening to UART
		  state = IDLE;
	  }

	  static uint32_t last_switch_clear_tick = 0;
	  if (HAL_GetTick() - last_switch_clear_tick >= 33) {  // ~33ms, reset once per frame
	      last_switch_clear_tick = HAL_GetTick();
	      for (int i = 0; i < 4; i++) {
	          if (microswitch_off_next_cycle[i]) {
	              microswitch_reported[i] = 0;
	              microswitch_off_next_cycle[i] = 0;
	          }
	      }
	  }

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 72-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65536-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 26-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 256 - 1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 3 - 1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 256 - 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 26-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 256 - 1;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 460800;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */
  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    __HAL_UART_CLEAR_PEFLAG(huart);
    resyncing = 1;
    HAL_UART_Receive_IT(&huart3, &sync_byte, 1);  // fall back to byte-scan resync
}

/* UART Receive Callback */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) { // when rx buffer is filled, this function will get called -- ie when biiig frame transmit guy done
	if (resyncing) {
        // scanning one byte at a time looking for 0xFF
        if (sync_byte == 0xFF) {
            resyncing = 0;
            uart_rx_buffer[0] = 0xFF;
            // receive the REMAINING 588 bytes into slot 1 onward
            HAL_UART_Receive_DMA(&huart3, &uart_rx_buffer[1], FRAME_DATA_LENGTH - 1);
        } else {
            HAL_UART_Receive_IT(&huart3, &sync_byte, 1); // keep scanning
        }
        return;
	}

	if (uart_rx_buffer[0] == 0xFF && state == IDLE) { // if header matches and state is idle
		state = WAITING;
		__HAL_TIM_SET_COUNTER(&htim1,0); // start millisecond timer @ 0

		command_led_1(uart_rx_buffer[(MODULE_ID * 12) - 11],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 1],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 2]);

		command_led_2(uart_rx_buffer[(MODULE_ID * 12) - 11 + 3],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 4],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 5]);

		command_led_3(uart_rx_buffer[(MODULE_ID * 12) - 11 + 6],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 7],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 8]);

		command_led_4(uart_rx_buffer[(MODULE_ID * 12) - 11 + 9],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 10],
					  uart_rx_buffer[(MODULE_ID * 12) - 11 + 11]);
	}
	else {
        // misaligned frame, do byte-scan resync until we hit header frame
        resyncing = 1;
        HAL_UART_Receive_IT(&huart3, &sync_byte, 1);
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) { // tx callback
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); //pull DE low
}

/* MICROSWITCH INTERRUPT */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_15) {
		potentialMan[0] = 1;
		timeStart[0] = HAL_GetTick();
	}
	if (GPIO_Pin == GPIO_PIN_3) {
		potentialMan[1] = 1;
		timeStart[1] = HAL_GetTick();
	}
	if (GPIO_Pin == GPIO_PIN_4) {
		potentialMan[2] = 1;
		timeStart[2] = HAL_GetTick();
	}
	if (GPIO_Pin == GPIO_PIN_5) {
		potentialMan[3] = 1;
		timeStart[3] = HAL_GetTick();
	}
}


/* USER CODE END 4 */

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
