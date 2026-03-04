/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"
#include "main.h"

/* USER CODE BEGIN 0 */
#include "stdio.h"
#include "string.h"

// USART2中断接收变量
uint8_t rx_data = 0;
uint8_t rx_buffer[100] = {0}; // 增大缓冲区到100字节
uint8_t rx_index = 0;
uint32_t last_rx_time = 0; // 上次接收时间戳
#define RX_TIMEOUT_MS 1000 // 接收超时时间

// 步进电机标志
extern uint8_t stepper_flag;

// 外部函数声明
extern void Stepper_Test(void);
/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  // 启动USART2中断接收
  HAL_UART_Receive_IT(&huart2, &rx_data, 1);
  /* USER CODE END USART2_Init 2 */

}

/* USER CODE BEGIN 1 */
// USART2中断回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART2)
  {
    // 更新接收时间戳
    last_rx_time = HAL_GetTick();
    
    // 接收到数据
    if(rx_data == '\n' || rx_data == '\r')
    {
      // 字符串结束，处理命令
      rx_buffer[rx_index] = '\0';
      
      // 检查是否是蓝牙连接信息
      if(strstr((char*)rx_buffer, "ble") || strstr((char*)rx_buffer, "BLE") || 
         strstr((char*)rx_buffer, "connect") || strstr((char*)rx_buffer, "Connect"))
      {
        // 忽略蓝牙连接信息
        printf("DEBUG: Bluetooth connection message received: %s\r\n", rx_buffer);
      }
      // 检查命令 - 不区分大小写
      else if(strstr((char*)rx_buffer, "LAY ON") || strstr((char*)rx_buffer, "lay on"))
      {
        // 拉高LAY，打开继电器
        HAL_GPIO_WritePin(LAY_GPIO_Port, LAY_Pin, GPIO_PIN_SET);
        HAL_UART_Transmit(&huart2, (uint8_t*)"LAY ON\r\n", 7, 100);
        printf("DEBUG: Received 'lay on' command, relay turned on\r\n");
      }
      else if(strstr((char*)rx_buffer, "LAY OFF") || strstr((char*)rx_buffer, "lay off"))
      {
        // 拉低LAY，关闭继电器
        HAL_GPIO_WritePin(LAY_GPIO_Port, LAY_Pin, GPIO_PIN_RESET);
        HAL_UART_Transmit(&huart2, (uint8_t*)"LAY OFF\r\n", 8, 100);
        printf("DEBUG: Received 'lay off' command, relay turned off\r\n");
      }
      else if(strstr((char*)rx_buffer, "ULN ON") || strstr((char*)rx_buffer, "uln on"))
      {
        // 发送响应
        HAL_UART_Transmit(&huart2, (uint8_t*)"ULN ON\r\n", 7, 100);
        printf("DEBUG: Received 'uln on' command, setting stepper flag\r\n");
        // 设置步进电机标志，在main函数中执行
        stepper_flag = 1;
      }
      else if(rx_index > 0)
      {
        // 回显接收到的命令
        HAL_UART_Transmit(&huart2, (uint8_t*)"Received: ", 9, 100);
        HAL_UART_Transmit(&huart2, rx_buffer, rx_index, 100);
        HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100);
        printf("DEBUG: Received unknown command: %s\r\n", rx_buffer);
      }
      
      // 重置缓冲区
      rx_index = 0;
      memset(rx_buffer, 0, sizeof(rx_buffer));
    }
    else
    {
      // 继续接收数据
      if(rx_index < sizeof(rx_buffer) - 1)
      {
        rx_buffer[rx_index++] = rx_data;
      }
      else
      {
        // 缓冲区满，重置
        printf("DEBUG: Buffer full, resetting\r\n");
        rx_index = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));
      }
    }
    
    // 重新启动中断接收
    HAL_UART_Receive_IT(&huart2, &rx_data, 1);
  }
}
/* USER CODE END 1 */

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspInit 1 */

  /* USER CODE END USART2_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);

    /* USART2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
int fputc(int ch,FILE *f)
{
	HAL_UART_Transmit (&huart1 ,(uint8_t *)&ch,1,HAL_MAX_DELAY );
	return ch;
}

/* USER CODE END 1 */
