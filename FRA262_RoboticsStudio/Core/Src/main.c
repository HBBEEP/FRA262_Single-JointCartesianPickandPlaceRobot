/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
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
#include "ModBusRTU.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim11;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
ModbusHandleTypedef hmodbus;
u16u8_t registerFrame[70];

// Joy Stick -----------
struct PortPin {
	GPIO_TypeDef *PORT;
	uint16_t PIN;
};

struct PortPin joyPin[4] = { { GPIOC, GPIO_PIN_2 }, // Button M
		{ GPIOA, GPIO_PIN_4 }, // Button R / D
		{ GPIOB, GPIO_PIN_0 }, // Button L / O
		{ GPIOB, GPIO_PIN_7 }, // Button A / P
		};

struct _GPIOState {
	GPIO_PinState Current;
	GPIO_PinState Last;
};

struct _GPIOState Button1[5];
typedef union {
	struct {
		uint16_t xAxis;
		uint16_t yAxis;
	} subdata;
} DMA_ADC_BufferType;
DMA_ADC_BufferType buffer[10];

float refXPos = 2000;
float refYPos = 2000;
uint8_t countBottomB = 0;
uint8_t countTopB = 0;
// MOTOR -------------------
float duty = 0;
uint8_t motorDir = 1;
// ENCODER -----------------
int32_t QEIReadRaw;

// PID ---------------------
struct pidVariables {
	float pTerm;
	float iTerm;
	float dTerm;
	float eIntegral;
};

struct pidVariables positionPID = { 0, 0, 0, 0 };
struct pidVariables velocityPID = { 0, 0, 0, 0 };

float mmActPos = 0;
float mmTargetPos = 0;
float mmError = 0;
// Calibrate ---------------
uint8_t startQEI = 0;
// SENSOR ------------------
uint8_t photoSig[3];

// TRAJ --------------------

typedef struct {
	float x;
	float y;
} Point;

Point objPickPos[9];
Point objPlacePos[9];

typedef struct {
	float pos1;
	float pos2;
	float pos3;
	float pos4;
	float pos5;
	float pos6;
} trayPos;

trayPos trayPickX = { .pos1 = 0, .pos2 = 0, .pos3 = 0 };
trayPos trayPickY = { .pos1 = 0, .pos2 = 0, .pos3 = 0 };

trayPos trayPlaceX = { .pos1 = 0, .pos2 = 0, .pos3 = 0 };
trayPos trayPlaceY = { .pos1 = 0, .pos2 = 0, .pos3 = 0 };

typedef struct {
	float posTraj;
	float velTraj;
	float accTraj;
	uint8_t reachTraj;
} calculationTraj;

//float qddm = 2000; // 210*11.205 -> 2117.745
//float qdm = 2100; // 189*11.205 -> 2353.05

float qddm = 500; // 210*11.205 -> 2117.745
float qdm = 600; // 189*11.205 -> 2353.05

float actualTime = 0;

uint16_t subTrajState = 0;

float timeTriSeg1 = 0;

float checkPos = 0;
float checkVel = 0;
float checkAcc = 0;

// Kalman Filter ----------
double K = 0;
double x = 0;
double P = 0;
double P_pre = 0;

double R = 708.5903334;
double C = 1;
double Q = 10000;

float kalmanVel = 0;
float kalmanPos = 0;
// TEST Variable ----------

float mmActVel = 0;
float mmActAcc = 0;
float prePos = 0;
float preVel = 0;
float pidVel = 0;

// Robot State
typedef enum {
	HOME, CALIBRATE, RUN
} robotState;

// Home State
typedef enum {
	INIT, HOMING, REACHHOME, PHOTODETECT
} homeState;

// Test Start ------------
uint8_t startTraj = 0; // TRACK 18 path (Only Position)
uint8_t startKalman = 0; // Lab Kalman
uint8_t startOnlyPosControl = 0; // Feedback From
uint8_t startCascadeControl = 0; // Feedback From Sensor
uint8_t startJoyStick = 0; // Start JoyStick
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM2_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM11_Init(void);
/* USER CODE BEGIN PFP */
void readEncoder();
void setMotor();
void cascadePIDControl();
float positionLoop(float targetPos);
void velocityLoop(float targetVel, float velFromPID);
void handleJoystick();
void handleJoystick2();
void buttonInput();
void buttonLogic(uint8_t state);
void photoDetect();
calculationTraj trapezoidalTraj(float initPos, float targetPos);
float kalmanFilter(float y);
void calibrateTrayTest();

void setHome(homeState state);
void kalmanLap();
void onlyPositionControl(float initPos, float targetPos);
void robotArmState(robotState state);

// ------
void calibrateTray(trayPos trayX, trayPos trayY, Point *objPos);
Point rotatePoint(float p1, float p2, float centerX, float centerY, float angle);
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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM5_Init();
  MX_TIM2_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM11_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_1 | TIM_CHANNEL_2);

	HAL_TIM_Base_Start(&htim1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

	HAL_TIM_Base_Start_IT(&htim2);

	HAL_ADC_Start_DMA(&hadc1, (uint16_t*) buffer, 20);

	hmodbus.huart = &huart2;
	hmodbus.htim = &htim11;
	hmodbus.slaveAddress = 0x15;
	hmodbus.RegisterSize = 70; // 70
	Modbus_init(&hmodbus, registerFrame);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		Modbus_Protocal_Worker();
		static uint32_t timestamp = 0;
		if (HAL_GetTick() >= timestamp) {
			timestamp = HAL_GetTick() + 100;
			registerFrame[0].U16 = 22881;

			// 0b0000000000000000 16 zeros
		}

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 99;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 9999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief TIM11 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM11_Init(void)
{

  /* USER CODE BEGIN TIM11_Init 0 */

  /* USER CODE END TIM11_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM11_Init 1 */

  /* USER CODE END TIM11_Init 1 */
  htim11.Instance = TIM11;
  htim11.Init.Prescaler = 99;
  htim11.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim11.Init.Period = 2005;
  htim11.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim11) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim11) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OnePulse_Init(&htim11, TIM_OPMODE_SINGLE) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_ACTIVE;
  sConfigOC.Pulse = 1433;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim11, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM11_Init 2 */

  /* USER CODE END TIM11_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 19200;
  huart2.Init.WordLength = UART_WORDLENGTH_9B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_EVEN;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6|GPIO_PIN_11|GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PC2 PC10 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB14 PB5 PB7
                           PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_14|GPIO_PIN_5|GPIO_PIN_7
                          |GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC11 PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim == &htim2) {
		photoDetect();
		readEncoder();
		if (startQEI) {
			readEncoder();
		}

		if (startOnlyPosControl) {
			//onlyPositionControl();
		}

		if (startTraj) {
			//onlyPositionControl();
		}

		if (startCascadeControl) {
			cascadePIDControl();
		}

	}
}

void setMotor() {
	if (motorDir) {
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
	}
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty);
}

void readEncoder() {
	QEIReadRaw = __HAL_TIM_GET_COUNTER(&htim5);
}

void cascadePIDControl() {
	// calculationTraj result = trapezoidalTraj();
	// velocityLoop(result.velTraj, positionLoop(result.posTraj));
}

float positionLoop(float targetPos) {

	mmActPos = QEIReadRaw * (2 * 3.14159 * 11.205 / 8192);
	mmActVel = (mmActPos - prePos) / 0.01;

//	mmActPos = kalmanFilter(mmActPos);
//	mmActVel = kalmanFilter(mmActVel);

	mmError = targetPos - mmActPos;
	positionPID.eIntegral = positionPID.eIntegral + (mmError * 0.01);
	pidVel = (positionPID.pTerm * mmError)
			+ (positionPID.iTerm * positionPID.eIntegral);
	return pidVel;

}

void velocityLoop(float targetVel, float velFromPID) {
	float velError = targetVel + velFromPID - mmActVel;
	duty = (velocityPID.pTerm * velError)
			+ (velocityPID.iTerm * velocityPID.eIntegral);
	if (duty < 0) {
		motorDir = 0;
		duty = (-1) * duty;
	} else {
		motorDir = 1;
	}
	if (duty > 1000) {
		duty = 1000;
	} else if (duty <= 120) {
		duty = 0;
	}
	prePos = mmActPos;
	setMotor();
}

void onlyPositionControl(float initPos, float targetPos) {
	calculationTraj result = trapezoidalTraj(initPos, targetPos);
	mmActPos = QEIReadRaw * (2 * 3.14159 * 11.205 / 8192);
	mmActVel = (mmActPos - prePos) / 0.01;
	mmActAcc = (mmActVel - preVel) / 0.01;

	//mmActPos = kalmanFilter(mmActPos);
	//mmActVel = kalmanFilter(mmActVel);

	mmError = result.posTraj - mmActPos;
	positionPID.eIntegral = positionPID.eIntegral + (mmError * 0.01);
	duty = (positionPID.pTerm * mmError)
			+ (positionPID.iTerm * positionPID.eIntegral);
	if (duty < 0) {
		motorDir = 0;
		duty = (-1) * duty;
	} else {
		motorDir = 1;
	}
	if (duty > 1000) {
		duty = 1000;
	} else if (duty <= 120) {
		duty = 0;
	}
	prePos = mmActPos;
	preVel = mmActVel;
	setMotor();
}

void handleJoystick() {
	refXPos = buffer[0].subdata.xAxis;
	if (refXPos > 2500) {
		motorDir = 1;
	} else if (refXPos < 1500) {
		motorDir = 0;
	}
	if (refXPos > 3600 || refXPos < 100) {
		duty = 500;
	} else if (refXPos > 2500 && refXPos <= 3600) {
		duty = 300;
	}

	else if (refXPos > 100 && refXPos <= 1500) {
		duty = 300;
	} else {
		duty = 0;
	}
}

void handleJoystick2() {
	refXPos = buffer[0].subdata.xAxis;
	if (refXPos > 2500) {
		motorDir = 1;
	} else if (refXPos < 1500) {
		motorDir = 0;
	}
	if (refXPos > 3600 || refXPos < 100) {
		duty = 250;
	} else if (refXPos > 2500 && refXPos <= 3600) {
		duty = 200;
	}

	else if (refXPos > 100 && refXPos <= 1500) {
		duty = 200;
	} else {
		duty = 0;
	}
}

void buttonInput() {
	register int i;
	for (i = 0; i < 4; i++) {
		Button1[i].Current = HAL_GPIO_ReadPin(joyPin[i].PORT, joyPin[i].PIN);
		if (Button1[i].Last == 0 && Button1[i].Current == 1) {
			if (i == 0) {
				countTopB += 1;
			}
			if (i == 3) {
				countBottomB += 1;

			}
			buttonLogic(i);
		}
		Button1[i].Last = Button1[i].Current;
	}
}

void buttonLogic(uint8_t state) {
	if (countTopB % 2 == 0) {
		switch (state) {
		case 0:
			// ENTER JOG MODE
			jogMode();
			break;
		case 1:
			// Right
			state = 0;
			break;
		case 2:
			// Change Axis of Movement
			break;
		case 3:
			// Left
			state = 0;
			break;
		}
	}
	if (countTopB % 2 == 1) {
		//calibrateMode();
		switch (state) {
		case 0:
			// ENTER JOG MODE
			break;
		case 1:
			// Pick
			trayPickX.pos1 = 0;
			trayPickY.pos1 = 0;

			trayPickX.pos2 = 0;
			trayPickY.pos2 = 0;

			trayPickX.pos3 = 0;
			trayPickY.pos3 = 0;

			trayPlaceX.pos1 = 0;
			trayPlaceY.pos1 = 0;

			trayPlaceX.pos2 = 0;
			trayPlaceY.pos2 = 0;

			trayPlaceX.pos3 = 0;
			trayPlaceY.pos3 = 0;
			break;
		case 2:
			// Open Laser
			break;
		case 3:
			//  Delete
			break;
		}
	}

}

void jogMode() {
	handleJoystick2();
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);
}

void photoDetect() {
	photoSig[0] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5);  // Motor Photo Sensor
	photoSig[1] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9); // CENTER Photo Sensor
	photoSig[2] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);  // Encoder Photo Sensor
}

calculationTraj trapezoidalTraj(float qi, float qf) {
	calculationTraj result;

	float diffPos = abs(qf - qi);
	int8_t handleMinus = (qf - qi) / diffPos;
	float timeTrapSeg1 = qdm / qddm;
	timeTriSeg1 = pow((diffPos / qddm), 0.5);

	if (timeTriSeg1 < timeTrapSeg1) // triangle shape
			{
		float qTriSeg1 = 0.5 * qddm * timeTriSeg1 * timeTriSeg1;
		float qdTriSeg1 = qddm * timeTriSeg1;
		if (actualTime <= timeTriSeg1) {
			result.posTraj = qi
					+ (0.5 * qddm * actualTime * actualTime) * handleMinus;
			result.velTraj = qddm * actualTime;
			result.accTraj = qddm;
		}

		else if (actualTime > timeTriSeg1 && actualTime <= (timeTriSeg1 * 2)) {
			float actualTimeSeg2 = actualTime - timeTriSeg1;
			result.posTraj = qi
					+ (qTriSeg1 + (qdTriSeg1 * actualTimeSeg2)
							- (0.5 * qddm * actualTimeSeg2 * actualTimeSeg2))
							* handleMinus;
			result.velTraj = qdTriSeg1 - (qddm * actualTimeSeg2);
			result.accTraj = -qddm;
		}

		else {
			result.posTraj = qf;
			result.velTraj = 0;
			result.accTraj = 0;
		}

	}

	else // trapezoidal shape
	{
		float timeTrapSeg2 = (diffPos
				- (2 * 0.5 * qddm * (timeTrapSeg1) * (timeTrapSeg1))) / qdm;
		float timeTrapSeg3 = qdm / qddm;
		float qTrapSeg1 = 0.5 * qddm * (timeTrapSeg1) * (timeTrapSeg1);
		float qTrapSeg2 = qTrapSeg1 + (qdm * timeTrapSeg2);

		if (actualTime <= timeTrapSeg1) {
			result.posTraj = qi
					+ (0.5 * qddm * actualTime * actualTime) * handleMinus;
			result.velTraj = qddm * actualTime;
			result.accTraj = qddm;
		} else if (actualTime > timeTrapSeg1
				&& actualTime <= timeTrapSeg2 + timeTrapSeg1) {
			float t2 = actualTime - timeTrapSeg1;
			result.posTraj = qi + (qTrapSeg1 + qdm * (t2)) * handleMinus;
			result.velTraj = qdm;
			result.accTraj = 0;
		} else if (actualTime > timeTrapSeg2 + timeTrapSeg1
				&& actualTime <= timeTrapSeg3 + timeTrapSeg2 + timeTrapSeg1) {
			float t3 = actualTime - timeTrapSeg2 - timeTrapSeg1;
			result.posTraj = qi
					+ (qTrapSeg2 + (qdm * t3) - 0.5 * qddm * t3 * t3)
							* handleMinus;
			result.velTraj = -qddm * t3 + qdm;
			result.accTraj = -qddm;
		} else {
			result.posTraj = qf;
			result.velTraj = 0;
			result.accTraj = 0;

		}

	}

	checkPos = result.posTraj;
	checkVel = result.velTraj;
	checkAcc = result.accTraj;
	actualTime += 0.01;
	if (result.posTraj == qf) {
		result.reachTraj = 1;
		actualTime = 0;
	} else {
		result.reachTraj = 0;
	}

	return result;
}

void calibrateTray(trayPos trayX, trayPos trayY, Point *objPos) {
	float length1 = pow(
			pow(trayX.pos1 - trayX.pos2, 2) + pow(trayY.pos1 - trayY.pos2, 2),
			0.5);
	float length2 = pow(
			pow(trayX.pos2 - trayX.pos3, 2) + pow(trayY.pos2 - trayY.pos3, 2),
			0.5);
	float k = 50;
	int caseL[2] = { 1, 0 };
	if (length1 > length2) {
		k = 60;
		caseL[0] = 0;
		caseL[1] = 1;
	}

	float length3 = trayY.pos1 - trayY.pos2;
	float radians = acos(length3 / k);

	float a[3] = { 10.0f, 30.0f, 50.0f };
	float b[3] = { 10.0f, 25.0f, 40.0f };

	for (int i = 0; i < 9; i++) {
		int index = i % 3;
		objPos[i].x = trayX.pos1 + a[index] * (caseL[0])
				+ b[index] * (caseL[1]);

		int row = i / 3;
		objPos[i].y = trayY.pos1 - b[row] * (caseL[0]) - a[row] * (caseL[1]);
		objPos[i] = rotatePoint(objPos[i].x, objPos[i].y, trayX.pos1,
				trayY.pos1, radians);
	}

}

Point rotatePoint(float p1, float p2, float centerX, float centerY,
		float radians) {

	float cosTheta = cosf(radians);
	float sinTheta = sinf(radians);

	float translatedX = p1 - centerX;
	float translatedY = p2 - centerY;

	Point rotatedPoint;
	rotatedPoint.x = (translatedX * cosTheta) - (translatedY * sinTheta)
			+ centerX;
	rotatedPoint.y = (translatedX * sinTheta) + (translatedY * cosTheta)
			+ centerY;

	return rotatedPoint;
}

void robotArmState(robotState state) {
	switch (state) {
	case HOME:
		// SET HOME
		state = CALIBRATE;
		break;
	case CALIBRATE:
		// SET JOYSTICK
		break;
	case RUN:
		break;
	default:
		break;
	}
}

void setHome(homeState state) {
	switch (state) {
	case INIT:
		if (photoSig[0]) // Motor Photo Sensor
		{
			state = HOMING;
		} else // ANY Position
		{
			motorDir = 0;
			duty = 220;
			setMotor();
			state = HOMING;
		}
		break;
	case HOMING:
		if (photoSig[0]) // Motor Photo Sensor
		{
			motorDir = 1;
			duty = 220;
			setMotor();
		} else if (photoSig[1]) // Center Photo Sensor
		{
			state = REACHHOME;
		}
		break;
	case REACHHOME:
		startQEI = 1;
		duty = 0;
		setMotor();
		QEIReadRaw = 0;
		state = PHOTODETECT;
		break;
	case PHOTODETECT:
		if (photoSig[0] || photoSig[2]) // Motor Photo Sensor
				{
			duty = 0;
			setMotor();
		}
		break;
	}

}
void HAL_ADC_ConvCallback(ADC_HandleTypeDef *hadc) {

}

// --------------------------------------------------
float kalmanFilter(float y) {
	P_pre = P + Q;
	K = (P_pre * C) / ((C * P_pre * C) + R);
	x = x + K * (y - C * x);
	P = (1 - (K * C) * P_pre);
	return x;
}
void kalmanLap() {
	mmActPos = QEIReadRaw * (2 * 3.14159 * 11.205 / 8192);
	mmActVel = (mmActPos - prePos) / 0.01;
	setMotor();
	prePos = mmActPos;
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
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
