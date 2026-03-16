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
#include "w25q64.h"
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
uint8_t crash_mode = 0;           // 0:正常模式, 1:碰撞报警抓拍模式
uint8_t crash_photo_count = 0;    // 已发送的照片数
uint32_t last_photo_time = 0;     // 上一次发送完毕的时间戳
uint32_t total_saved_photos = 0;  // 记录 Flash 里总共存了多少张照片
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
  MX_SPI2_Init();
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
  /*初始化 MPU6050 */
  if (MPU6050_Init() != 0)
  {
      // 如果 MPU6050 初始化失败，可以在屏幕上提示一下，或者不影响主流程继续运行
      UI_DrawString(5, 30, "MPU6050 ERROR!", COLOR_RED, COLOR_BLACK);
  }
  /* 初始化 W25Q64 */
  W25Q64_Init();
  uint16_t flash_id = W25Q64_ReadID();
  if (flash_id != 0xEF16) { // 检查 ID 是否正确
      UI_DrawString(5, 45, "FLASH ERROR!", COLOR_RED, COLOR_BLACK);
      HAL_Delay(2000); // 延时一下让你看清报错
  }
  /* 初始化ESP8266 */
  if (ESP8266_ConnectTo_TCP_Server("yhwhsy", "13616338678", "192.168.120.77", 8080) != 0)
  {
      ST7789_Fill(COLOR_RED); 
      while(1); // 如果返回 1 (失败)，则亮红屏死机
  }
  is_online = 1; // 成功联网，切换到在线模式
  ST7789_Fill(COLOR_BLACK);
  HAL_Delay(500);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // === 1. DMA 摄像头抓拍与屏幕显示 (保持不变) ===
    HAL_DCMI_Stop(&hdcmi);
    if (HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)full_frame_buf, 25600) == HAL_OK)
    {
        uint32_t wait_time = HAL_GetTick();
        while(HAL_DCMI_GetState(&hdcmi) != HAL_DCMI_STATE_READY) 
        {
            if (HAL_GetTick() - wait_time > 200) break; 
        }
        TFT_ShowWidescreen(full_frame_buf);
    }

    // === 2. 顶部 UI 刷新逻辑 ===
    static uint32_t last_ui_time = 0;
    if (HAL_GetTick() - last_ui_time > 1000) 
    {                 
        if (crash_mode == 0) 
        {
            // --- 正常模式：显示光照和速度 ---
            uint8_t current_light = Sensor_GetLightPercent();
            if (current_light > 70) {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET); 
            } else {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); 
            }
            
            current_speed = speed_pulse_count; 
            speed_pulse_count = 0; 
            
            char ui_buf[64];
            sprintf(ui_buf, "Light: %3d %%  Speed: %3d", current_light, current_speed);
            UI_DrawString(5, 12, ui_buf, COLOR_WHITE, COLOR_BLACK); 
        }
        else 
        {
            // --- 碰撞模式：显示红色警报 ---
            // 注意这里后面加了空格，是为了用黑底完美覆盖掉原来长长的 "Light: xx %" 字符
            UI_DrawString(5, 12, "CRASH DETECTED!         ", COLOR_RED, COLOR_BLACK);
        }
        last_ui_time = HAL_GetTick();
    }

    // === 3. MPU6050 碰撞检测 (每50ms检测一次) ===
    static uint32_t last_mpu_time = 0;
    if (HAL_GetTick() - last_mpu_time > 50) 
    {
        if (MPU6050_CheckCollision() == 1) 
        {
            if (crash_mode == 0) 
            {
                crash_mode = 1;          // 触发碰撞模式
                crash_photo_count = 0;   // 进度清零
                last_photo_time = 0;     // 立刻准备发送第一张
                
                // 立即在屏幕上打出红字警告
                UI_DrawString(5, 12, "CRASH DETECTED!         ", COLOR_RED, COLOR_BLACK);
            }
        }
        last_mpu_time = HAL_GetTick();
    }

    // === 4. 碰撞模式下的定时发送逻辑 ===
    if (crash_mode == 1) 
    {
        // 这里的 10000 是两次发送之间的间隔毫秒数(10秒)。
        // 如果你希望 3 张照片在发送完毕后立刻接力发送下一张，把这里的 10000 改为 0 即可。
        if (HAL_GetTick() - last_photo_time > 10000 || crash_photo_count == 0) 
        {
            // 将当前的 full_frame_buf 保存到 W25Q64 闪存中！
            if (total_saved_photos >= 80) total_saved_photos = 0; // 防止溢出，循环覆盖
            W25Q64_SavePhoto(total_saved_photos, (uint8_t*)full_frame_buf);
            total_saved_photos++;
            if (is_online == 1) 
            {
                // 可以加个串口信息提示目前是第几张
                char status_buf[64];
                sprintf(status_buf, "[CRASH]Photo:%d/3\r\n[FRAME_START]", crash_photo_count + 1);
                HAL_UART_Transmit(&huart3, (uint8_t*)status_buf, strlen(status_buf), 100);
                // 发送图片切片
                for(int i = 0; i < 200; i++) 
                {
                    HAL_UART_Transmit(&huart3, ((uint8_t*)full_frame_buf) + (i * 512), 512, 1000);
                    HAL_Delay(50); 
                }
            }
            crash_photo_count++;               // 拍完一张，计数+1
            last_photo_time = HAL_GetTick();   // 记录刚发完的时间戳
            // 检查是否已经发够了 3 张
            if (crash_photo_count >= 3) 
            {
                crash_mode = 0; // 任务完成，解除碰撞模式
                UI_DrawString(5, 12, "                        ", COLOR_WHITE, COLOR_BLACK); 
            }
        }
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
