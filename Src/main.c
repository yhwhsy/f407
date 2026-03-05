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
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ov7670.h"
#include "st7789.h"
#include "string.h"
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
volatile uint16_t g_row = 0;           /* 当前屏幕写入行位置（全局变量，防撕裂保护用） */
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

/* DCMI DMA全传输完成回调 - 手动注册 */
static void DCMI_FullFrame_Callback(DMA_HandleTypeDef *hdma)
{
    UNUSED(hdma);
    flag_full_ready = 1;
}

/* DCMI 错误回调 - 用于捕获同步错误或DMA错误 */
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
    UNUSED(hdcmi);
    /* 错误处理：红色闪烁表示DCMI错误 */
    static uint8_t err_cnt = 0;
    err_cnt++;
    
    /* 短暂显示红色警告 */
    ST7789_Fill(COLOR_RED);
    HAL_Delay(200);
    ST7789_Fill(COLOR_BLACK);
    
    /* 可选：自动重启DCMI */
    /* HAL_DCMI_Stop(hdcmi); */
    /* HAL_DCMI_Start_DMA(hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)g_line_buf, (320 * 20 * 2) / 4); */
}

/* DMA 错误回调 - 处理DMA传输错误 */
void HAL_DMA_ErrorCallback(DMA_HandleTypeDef *hdma)
{
    UNUSED(hdma);
    /* DMA错误处理：蓝色闪烁表示DMA错误 */
    static uint8_t dma_err_cnt = 0;
    dma_err_cnt++;
    
    /* 短暂显示蓝色警告 */
    ST7789_Fill(COLOR_BLUE);
    HAL_Delay(300);
    ST7789_Fill(COLOR_BLACK);
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
  MX_DMA_Init();
  MX_DCMI_Init();
  MX_SPI1_Init();
  MX_USART3_Init();
  /* USER CODE BEGIN 2 */
  /* 初始化TFT */
  ST7789_Init(&hspi1);
  ST7789_SetRotation(1);
  ST7789_Fill(COLOR_BLACK);

  /* 初始化OV7670 */
  uint8_t ov_ret = OV7670_Init();
  uint8_t check_val = 0;
  OV7670_ReadReg(REG_COM7, &check_val);
  if (ov_ret != 0)
  {
      ST7789_Fill(COLOR_RED);  /* 失败 */
      while(1);
  }

  /* 启动DCMI DMA捕获 - 使用CubeMX生成的配置 */
  /* 启动DCMI捕获，使用行缓冲模式 - 20行 x 320像素 x 2字节 = 12800字节 */
  HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)g_line_buf, (320 * 20 * 2) / 4);
  
  /* 手动注册半传输回调 - HAL_DCMI_Start_DMA不会自动设置XferHalfCpltCallback */
  hdcmi.DMA_Handle->XferHalfCpltCallback = DCMI_HalfFrame_Callback;
  hdcmi.DMA_Handle->XferCpltCallback = DCMI_FullFrame_Callback;
  
  /* 显示绿色表示启动成功 */
  ST7789_Fill(COLOR_GREEN);
  HAL_Delay(500);
  ST7789_Fill(COLOR_BLACK);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_check = HAL_GetTick();
  uint32_t last_irq_count = 0;
  
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 处理DMA半传输完成 - 发送前10行 */
    if (flag_half_ready)
    {
        flag_half_ready = 0;
        dma_irq_count++;
        
        /* 临时保存当前行位置，防止中断中g_row被修改导致窗口设置错误 */
        uint16_t current_row;
        __disable_irq();
        current_row = g_row;
        g_row += 10;
        if (g_row >= 240) g_row = 0;
        __enable_irq();
        
        ST7789_SetWindow(0, current_row, 319, current_row + 9);
        TFT_DC_HIGH();
        TFT_CS_LOW();
        HAL_SPI_Transmit(&hspi1, g_line_buf, 320 * 10 * 2, HAL_MAX_DELAY);
        TFT_CS_HIGH();
    }
    
    /* 处理DMA全传输完成 - 发送后10行 */
    if (flag_full_ready)
    {
        flag_full_ready = 0;
        dma_irq_count++;
        
        /* 临时保存当前行位置，防止中断中g_row被修改导致窗口设置错误 */
        uint16_t current_row;
        __disable_irq();
        current_row = g_row;
        g_row += 10;
        if (g_row >= 240) g_row = 0;
        __enable_irq();
        
        ST7789_SetWindow(0, current_row, 319, current_row + 9);
        TFT_DC_HIGH();
        TFT_CS_LOW();
        HAL_SPI_Transmit(&hspi1, g_line_buf + 320 * 10 * 2, 320 * 10 * 2, HAL_MAX_DELAY);
        TFT_CS_HIGH();
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

/* DCMI帧完成回调 - VSYNC触发，用于帧同步 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    UNUSED(hdcmi);
    /* 防画面撕裂"安全带"：帧事件时重置行位置 */
    g_row = 0;
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
