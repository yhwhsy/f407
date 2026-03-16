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
#include "adc.h"
#include "dcmi.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ov7670.h"
#include "st7789.h"
#include "string.h"
#include "esp8266.h"
#include "sensor.h"
#include "ui.h"
#include "mpu6050.h"
#include "stdio.h"
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
uint8_t photo_buf[38400];     // 开辟 38.4KB 内存，存放提取后的照片
volatile uint8_t flag_half_ready = 0;  /* DMA半传输完成：前10行就绪 */
volatile uint8_t flag_full_ready = 0;  /* DMA全传输完成：后10行就绪 */
volatile uint32_t dma_irq_count = 0;   /* DMA中断计数器 */
volatile uint16_t g_row = 0;           /* 当前屏幕写入行位置（全局变量，防撕裂保护用） */
uint8_t rx_buffer[100];         // 串口接收缓冲区
volatile uint8_t esp_ok_flag = 0; // 成功收到OK的标志位
__attribute__((aligned(4))) 
uint16_t full_frame_buf[320 * 160]; 
uint8_t is_online = 0;        // 0: 脱机模式(仅屏幕当记录仪), 1: 联网模式(发图片给电脑)
uint8_t take_photo_state = 0; // 0: 平时刷屏, 1: 准备抓拍, 2: 正在发送
volatile uint32_t speed_pulse_count = 0; // 记录脉冲数的全局变量
uint16_t current_speed = 0;              // 当前计算出的速度 (转/秒 或 km/h)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void TFT_ShowWidescreen(uint16_t* buf) 
{
    // 屏幕高度240，图像高度160。上下各留白 40 像素，居中显示！
    ST7789_SetWindow(0, 40, 319, 199);
    TFT_DC_HIGH();
    TFT_CS_LOW();
    
    // 因为 HAL_SPI_Transmit 最大只能发 65535 字节，我们把 102400 字节分两次轰进去！
    HAL_SPI_Transmit(&hspi1, (uint8_t*)buf, 51200, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&hspi1, ((uint8_t*)buf) + 51200, 51200, HAL_MAX_DELAY);
    
    TFT_CS_HIGH();
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
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
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
  HAL_Delay(500);
  /* 初始化ESP8266 */
  // if (ESP8266_ConnectTo_TCP_Server("yhwhsy", "13616338678", "192.168.120.77", 8080) != 0)
  // {
  //     ST7789_Fill(COLOR_RED); 
  //     while(1); // 如果返回 1 (失败)，则亮红屏死机
  // }
  // is_online = 1; // 成功联网，切换到在线模式
  ST7789_Fill(COLOR_BLACK);
  HAL_Delay(500);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 1. 防卡死：每次抓拍前强制停止上一次的状态
    HAL_DCMI_Stop(&hdcmi);
    // 2. 启动 DMA 抓拍 (截取前 160 行，实现 320x160 宽屏)
    // 25600 Words = 102400 字节 = 100KB
    if (HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)full_frame_buf, 25600) == HAL_OK)
    {
        uint32_t wait_time = HAL_GetTick();
        // 带超时保护的死等：最多等 200 毫秒
        while(HAL_DCMI_GetState(&hdcmi) != HAL_DCMI_STATE_READY) 
        {
            if (HAL_GetTick() - wait_time > 200) {
                break; // 超时强行跳出，防止单片机彻底卡死变砖！
            }
        }
        TFT_ShowWidescreen(full_frame_buf);
    }
    static uint32_t last_ui_time = 0;
    if (HAL_GetTick() - last_ui_time > 1000) 
    {                 
        uint8_t current_light = Sensor_GetLightPercent();
        if (current_light > 70) 
        {
            // 环境太暗了！点亮 LED (假设你用的是 PB1)
            // 注意：如果你的 LED 是接在 VCC 上的，这里可能要改成 RESET 才能亮
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET); 
        } else {
            // 光线充足，熄灭 LED 节能
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); 
        }
        // 2. 测速传感器计算
        current_speed = speed_pulse_count; 
        speed_pulse_count = 0; // 读完清零，为下一个1秒重新计数
        char ui_buf[32];
        sprintf(ui_buf, "L: %3d %%  Spd: %3d", current_light, current_speed);
        UI_DrawString(5, 12, ui_buf, COLOR_WHITE, COLOR_BLACK); // 记得把之前的 UI_Update_TopBar 替换成直接画字符串
        last_ui_time = HAL_GetTick();
    }
    // 3. 事件触发发送逻辑
    if (take_photo_state == 1) 
    {
        take_photo_state = 2; // 标记正在发送，防止主循环重复触发
        if (is_online == 1) 
        {
            uint8_t light_val = Sensor_GetLightPercent();
            char status_buf[64];
            sprintf(status_buf, "[DATA]Light:%d%%,Speed:%d\r\n[FRAME_START]", light_val, current_speed);
            HAL_UART_Transmit(&huart3, (uint8_t*)status_buf, strlen(status_buf), 100);
            // 将 100KB 的超清图片切片发送
            for(int i = 0; i < 200; i++) 
            {
                HAL_UART_Transmit(&huart3, ((uint8_t*)full_frame_buf) + (i * 512), 512, 1000);
                HAL_Delay(50); 
            }
        }
        // 发完收工，状态归零，单片机马上恢复流畅的视频监控
        take_photo_state = 0; 
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

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        // 给字符串末尾加上结束符，防止越界
        if (Size < sizeof(rx_buffer)) {
            rx_buffer[Size] = '\0'; 
        } else {
            rx_buffer[sizeof(rx_buffer)-1] = '\0';
        }
        
        // 检查 ESP8266 的回复中是否包含 "OK"
        if (strstr((char*)rx_buffer, "OK") != NULL)
        {
            esp_ok_flag = 1; // 成功标志置1
        }
        
        // 重新开启中断接收，等待下一次数据
        HAL_UARTEx_ReceiveToIdle_IT(&huart3, rx_buffer, sizeof(rx_buffer));
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_5) 
    {
        speed_pulse_count++; // 脉冲计数器 +1
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
