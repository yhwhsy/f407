/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dma.c
  * @brief   This file provides code for the configuration
  *          of all the requested DMA streams.
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
#include "dma.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

DMA_HandleTypeDef hdma_dcmi;

/*----------------------------------------------------------------------------*/
/* Configure DMA                                                              */
/*----------------------------------------------------------------------------*/

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Enable DMA controller clock
  */
void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream1_IRQn interrupt configuration (DCMI) */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  
  /* DMA2_Stream3_IRQn interrupt configuration (SPI1 TX) */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/* DCMI DMA Init */
void MX_DCMI_DMA_Init(void)
{
  /* DCMI DMA Init */
  hdma_dcmi.Instance = DMA2_Stream1;
  hdma_dcmi.Init.Channel = DMA_CHANNEL_1;
  hdma_dcmi.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_dcmi.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_dcmi.Init.MemInc = DMA_MINC_ENABLE;
  hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdma_dcmi.Init.Mode = DMA_CIRCULAR;
  hdma_dcmi.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_dcmi.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_dcmi.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_dcmi.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_dcmi.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_dcmi) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);
}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

