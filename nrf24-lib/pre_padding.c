/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include <stdio.h>
#include <stdbool.h>

#include "rf24_lib.h"

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
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define BUF_SIZE 6
#define RF_24_ADDR 0x01

#define CE_LOW() HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);
#define CE_HIGH() HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);

#define CS_LOW() HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
#define CS_HIGH() HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

#define R_RX_PAYLOAD 0x61 //ADDR OF PAYLOAD (DATA)
#define STATUS_REG 0x07
#define NOP_REG	0xFF //NOP - none operation

#define R_REGISTER    0x00
#define W_REGISTER    0x20

#define INIT_ADDR 0x00
#define AUTO_ACK_ADDR 0x01
#define RF_CH_ADDR	0x05
#define RF_SETUP_ADDR	0x06

#define RF24_PWR_UP 1
#define RF24_RX_MODE 1
#define RF24_TX_MODE 0

#define PIPE_0 0x0A
#define PIPE_1 0x0B
#define PIPE_2 0x0C
#define PIPE_3 0x0D
#define PIPE_4 0x0E
#define PIPE_5 0x0F

uint8_t bufData[BUF_SIZE] = {0};

//User-define
#define CHANNEL_COM 10
#define PIPE PIPE_0
#define POWER 0x06

// Function
uint8_t writeConfigReg(uint8_t reg, uint8_t* data) {
	/*                ┌──────────┐
	MOSI →  CMD BYTE│ W_REGISTER | reg │
					└──────────┘
					┌──────────┐
	MOSI →  DATA BYTE│   value to write│
					└──────────┘
	 */
	uint8_t cmd = W_REGISTER | reg;
	CS_LOW()
	if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 2) != HAL_OK)
		return 0;
	if (HAL_SPI_Transmit(&hspi2, data, 1, 2) != HAL_OK)
		return 0;
	CS_HIGH()

	return 1;
}

uint8_t readConfigReg(uint8_t reg, uint8_t* val, uint8_t val_len) {
    uint8_t cmd = R_REGISTER | reg;
    CS_LOW();
	if (HAL_SPI_Transmit(&hspi2, &cmd, 1, 2) != HAL_OK)
		return 0;
	if (HAL_SPI_Receive(&hspi2, val, val_len, 2) != HAL_OK)
		return 0;
    CS_HIGH();

    return 1;
}

void readDataFromRF24(uint8_t* buffer, uint8_t len) {
	//normal mode buffer max is only have 5 byte
	readConfigReg(R_RX_PAYLOAD, buffer, len);
}

uint8_t checkIdleRF24() {
	//normal mode buffer max is only have 5 byte
	uint8_t status;
	readConfigReg(STATUS_REG, &status, 1);

	if (status & (1<<6))
		return 1; // Rx have data
	if (status & (1<<5))
		return 2; // Tx have data

	return 0;
}

uint8_t RF24_ReadRegister(uint8_t reg)
{
    uint8_t val = 0;
    uint8_t cmd = R_REGISTER | reg; // READ_REG
    CS_LOW();
	if (HAL_SPI_Transmit(&hspi2, &cmd, sizeof(cmd), 2) != HAL_OK)
		return 0;
	if (HAL_SPI_Receive(&hspi2, &val, sizeof(val), 2) != HAL_OK)
		return 0;
    CS_HIGH();
    return val;
}

int RF24_Init(uint8_t mode)
{
	//=======================
	uint8_t config;
	readConfigReg(INIT_ADDR, &config, 1);

	//Enable by mode
	config = (config & ~(1 << 0)) | (mode << 0);

	//Power-up function
	config |= (RF24_PWR_UP << 1);
	writeConfigReg(INIT_ADDR, &config);

	//=======================
	uint8_t ack = 0x01;  // Turn on auto ack
	writeConfigReg(AUTO_ACK_ADDR, &ack);

	uint8_t pipe_com = PIPE;
    writeConfigReg(RF_24_ADDR, &pipe_com);

    //=======================
    uint8_t channel = CHANNEL_COM;
	writeConfigReg(RF_CH_ADDR, &channel);
	uint8_t power = POWER;
	writeConfigReg(RF_SETUP_ADDR, &power);

    //=======================
	//Enable transmit upon mode
	if (mode) {
		CE_HIGH()
	}
	else {
		CE_LOW();
	}

	//For hardware okay
	HAL_Delay(2);

	return 1;
}



void RF24_Listen()
{
	if (checkIdleRF24() == 1)
	{
		readDataFromRF24(bufData, BUF_SIZE);
        // Reset RX_DR,
		// RX_Buffer automatically reset but RX Flag is not so we need set it by manual
        uint8_t clear;
        readConfigReg(STATUS_REG, &clear, 1);
        clear |= (1 << 6);

        writeConfigReg(STATUS_REG, &clear);
	}
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

  RF24_Init((uint8_t)RF24_RX_MODE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  RF24_Listen();

//	  readDataFromRF24();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CE_Pin */
  GPIO_InitStruct.Pin = CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CE_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
