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
#include "dcmi.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ov7670.h"
#include "st7789.h"
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

/* USER CODE BEGIN PV */
/* 行缓冲区 - 20行 x 320像素 x 2字节 */
__attribute__((aligned(4)))
uint8_t g_line_buf[320 * 20 * 2];

volatile uint8_t flag_half_ready = 0;  /* DMA半传输完成：前10行就绪 */
volatile uint8_t flag_full_ready = 0;  /* DMA全传输完成：后10行就绪 */
volatile uint32_t dma_irq_count = 0;   /* DMA中断计数器 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* DCMI DMA半传输完成回调 - 手动注册，HAL_DCMI_Start_DMA不会自动设置 */
static void DCMI_HalfFrame_Callback(DMA_HandleTypeDef *hdma)
{
    UNUSED(hdma);
    flag_half_ready = 1;
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
  HAL_Init();
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_DCMI_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  
  /* USER CODE BEGIN 2 */
  /* 初始化TFT */
  ST7789_Init(&hspi1);
  ST7789_SetRotation(1);
  ST7789_Fill(COLOR_BLACK);

  /* 初始化OV7670 */
  uint8_t ov_ret = OV7670_Init();
  if (ov_ret != 0)
  {
      ST7789_Fill(COLOR_RED);  /* 失败 */
      while(1);
  }

  /* 启动DCMI DMA捕获 - 使用CubeMX生成的配置 */
  /* 启动DCMI捕获，使用行缓冲模式 - 20行 x 320像素 x 2字节 = 12800字节 */
  HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)g_line_buf, (320 * 20 * 2) / 4);
  
  /* 手动注册半传输回调 - HAL_DCMI_Start_DMA不会自动设置XferHalfCpltCallback */
  /* 注意：XferCpltCallback由HAL内部使用，不能覆盖！ */
  hdcmi.DMA_Handle->XferHalfCpltCallback = DCMI_HalfFrame_Callback;
  
  /* 显示绿色表示启动成功 */
  ST7789_Fill(COLOR_GREEN);
  HAL_Delay(500);
  ST7789_Fill(COLOR_BLACK);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_check = HAL_GetTick();
  uint32_t last_irq_count = 0;
  uint16_t g_row = 0;  /* 当前屏幕写入行位置 */
  
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 处理DMA半传输完成 - 发送前10行 */
    if (flag_half_ready)
    {
        flag_half_ready = 0;
        dma_irq_count++;
        
        ST7789_SetWindow(0, g_row, 319, g_row + 9);
        TFT_DC_HIGH();
        TFT_CS_LOW();
        HAL_SPI_Transmit(&hspi1, g_line_buf, 320 * 10 * 2, HAL_MAX_DELAY);
        TFT_CS_HIGH();
        
        g_row += 10;
        if (g_row >= 240) g_row = 0;  /* 写满240行自动循环，不依赖帧中断 */
    }
    
    /* 处理DMA全传输完成 - 发送后10行 */
    if (flag_full_ready)
    {
        flag_full_ready = 0;
        dma_irq_count++;
        
        ST7789_SetWindow(0, g_row, 319, g_row + 9);
        TFT_DC_HIGH();
        TFT_CS_LOW();
        HAL_SPI_Transmit(&hspi1, g_line_buf + 320 * 10 * 2, 320 * 10 * 2, HAL_MAX_DELAY);
        TFT_CS_HIGH();
        
        g_row += 10;
        if (g_row >= 240) g_row = 0;  /* 写满240行自动循环 */
    }
    
    /* 每秒检查一次DMA状态 - 持续心跳监测 */
    if (HAL_GetTick() - last_check >= 1000)
    {
        last_check = HAL_GetTick();
        
        /* 如果DMA中断计数没有增加，显示蓝色警告 */
        if (dma_irq_count == last_irq_count)
        {
            ST7789_Fill(COLOR_BLUE);
            HAL_Delay(100);
            ST7789_Fill(COLOR_BLACK);
        }
        last_irq_count = dma_irq_count;
    }
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);
}

/* USER CODE BEGIN 4 */
/* 
 * 回调机制说明：
 * - 半传输：XferHalfCpltCallback = DCMI_HalfFrame_Callback（手动注册）
 * - 全传输：由HAL内部DCMI_DMAXferCplt处理，最终触发帧中断
 * - 帧中断：在单段DMA模式下每20行触发一次，不代表真正帧结束
 */

/* DCMI帧完成回调 - 在单段DMA模式下每20行触发一次 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    UNUSED(hdcmi);
    flag_full_ready = 1;   /* 通知：后10行数据就绪 */
    /* 注意：不能在这里归零g_row，因为此回调每20行触发一次，不是每帧一次 */
}

/* SPI DMA传输完成回调 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        ST7789_DMA_TxCpltCallback();
    }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{}
#endif