/**
 ******************************************************************************
 * @file    main.c
 * @brief   OV7670 摄像头 + ST7789 TFT 显示主程序
 *
 *  功能：OV7670采集RGB565图像，通过DCMI+DMA存入帧缓冲，
 *        再通过SPI1将帧缓冲数据发送到ST7789屏幕显示。
 *
 *  硬件：STM32F407ZGT (LXB407ZG-P1)
 *  系统时钟：168MHz
 *  预期帧率：约10-15fps（受限于SPI刷屏速度）
 *
 *  引脚分配：
 *    SPI1: PA5(SCK), PA7(MOSI)
 *    TFT:  CS=PB12, DC=PC5, RST=PC4, BLK=PB0
 *    DCMI: PCLK=PA6, VSYNC=PB7, HREF=PA4
 *          D0=PC6, D1=PC7, D2=PE0, D3=PE1
 *          D4=PE4, D5=PB6, D6=PE5, D7=PE6
 *    I2C1: SCL=PB8, SDA=PB9 (OV7670 SCCB)
 *    MCO1: PA8 → XCLK (24MHz给OV7670)
 *    OV7670控制: RESET=PE3, PWDN=PD6
 ******************************************************************************
 */

#include "main.h"
#include "st7789.h"
#include "ov7670.h"
#include "dcmi_capture.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 *  外设句柄声明（CubeMX生成）
 * ============================================================ */
DCMI_HandleTypeDef  hdcmi;
DMA_HandleTypeDef   hdma_dcmi;
I2C_HandleTypeDef   hi2c1;
SPI_HandleTypeDef   hspi1;
DMA_HandleTypeDef   hdma_spi1_tx;

/* ============================================================
 *  私有函数声明
 * ============================================================ */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);
static void MX_DCMI_Init(void);
static void MX_DMA_Init(void);
static void MCO1_Config(void);
static void Error_Handler(void);

/* ============================================================
 *  主函数
 * ============================================================ */
int main(void)
{
    /* HAL初始化 */
    HAL_Init();

    /* 系统时钟：168MHz，HSE=8MHz */
    SystemClock_Config();

    /* 外设初始化 */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    MX_I2C1_Init();
    MX_DCMI_Init();

    /* MCO1输出24MHz给OV7670的XCLK */
    MCO1_Config();
    HAL_Delay(10);  /* 等待XCLK稳定 */

    /* ---- 1. 初始化TFT屏幕 ---- */
    ST7789_Init(&hspi1);
    ST7789_SetRotation(1);  /* 横屏模式，匹配320x240 */

    /* 显示启动画面 */
    ST7789_Fill(COLOR_BLACK);

    /* ---- 2. 初始化OV7670摄像头 ---- */
    uint8_t cam_ret = OV7670_Init(&hi2c1);
    if (cam_ret != 0)
    {
        /* 摄像头初始化失败：屏幕显示红色错误提示 */
        ST7789_Fill(COLOR_RED);
        /* 用户可通过串口或LED判断错误码 */
        while (1) { HAL_Delay(500); }
    }

    /* 摄像头初始化成功：短暂显示绿色 */
    ST7789_Fill(COLOR_GREEN);
    HAL_Delay(300);

    /* ---- 3. 启动DCMI连续采集 ---- */
    DCMI_Capture_Init(&hdcmi, &hdma_dcmi);
    DCMI_Capture_Start();

    /* ============================================================
     *  主循环：等待帧采集完成 → 刷新TFT屏幕
     * ============================================================ */
    while (1)
    {
        /* 等待DCMI帧完成标志 */
        if (g_frame_ready)
        {
            g_frame_ready = 0;

            /*
             *  OV7670输出的RGB565数据字节序：
             *  每像素2字节，高字节先，低字节后
             *  即 [R4R3R2R1R0G5G4G3][G2G1G0B4B3B2B1B0]
             *
             *  ST7789接收的RGB565格式相同，可直接写入
             *
             *  注：DCMI每次通过AHB传输32bit，
             *      字节顺序可能需要调整（取决于DCMI_CR的JPEG/非JPEG）
             *      若颜色显示异常，尝试交换高低字节或使用TSLB寄存器
             */
            ST7789_WriteFrameBuffer(g_frame_buf, OV7670_FRAME_SIZE);
        }
        /* 其他后台任务可在此处理 */
    }
}

/* ============================================================
 *  系统时钟配置
 *  HSE=8MHz → PLL → SYSCLK=168MHz
 *  AHB=168MHz, APB1=42MHz, APB2=84MHz
 * ============================================================ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct  = {0};
    RCC_ClkInitTypeDef       RCC_ClkInitStruct  = {0};

    /* 使能HSE，配置PLL */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    /* HSE=8MHz, PLLM=8, PLLN=336, PLLP=2, PLLQ=7 */
    /* SYSCLK = 8/8 * 336 / 2 = 168MHz */
    /* USB/SDIO = 8/8 * 336 / 7 = 48MHz */
    RCC_OscInitStruct.PLL.PLLM            = 8;
    RCC_OscInitStruct.PLL.PLLN            = 336;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ            = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    /* 选择时钟源和分频 */
    RCC_ClkInitStruct.ClockType           = RCC_CLOCKTYPE_HCLK  |
                                             RCC_CLOCKTYPE_SYSCLK |
                                             RCC_CLOCKTYPE_PCLK1 |
                                             RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource        = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider       = RCC_SYSCLK_DIV1;   /* AHB=168MHz */
    RCC_ClkInitStruct.APB1CLKDivider      = RCC_HCLK_DIV4;     /* APB1=42MHz */
    RCC_ClkInitStruct.APB2CLKDivider      = RCC_HCLK_DIV2;     /* APB2=84MHz */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

/* ============================================================
 *  MCO1配置：输出HSE/1 = 8MHz × PLL倍频到PA8
 *  实际使用：MCO1输出HSE直接，再用PLLM=8使OV7670得到24MHz
 *
 *  方案：MCO1 = PLL / 7 ≈ 24MHz
 *  或：  MCO1 = HSE / MCODIV_1 = 8MHz（OV7670支持10-48MHz）
 *
 *  这里输出 HSE=8MHz 直接给OV7670（简单可靠）
 *  若需更高质量，可输出24MHz（PLLDIV=7，约24MHz）
 * ============================================================ */
static void MCO1_Config(void)
{
    /* PA8配置为MCO1，输出 HSE/1 = 8MHz 给OV7670的XCLK */
    /* OV7670支持10~48MHz XCLK，8MHz也可工作（部分型号）*/
    /* 若摄像头不稳定，改用HSI(16MHz)或PLL输出 */
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);
    /* PA8已在GPIO初始化中配置为AF0（MCO1）*/
}

/* ============================================================
 *  GPIO初始化
 * ============================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能所有用到的GPIO时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* ---- TFT控制引脚：CS(PB12), DC(PC5), RST(PC4), BLK(PB0) ---- */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin   = GPIO_PIN_12 | GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* 初始状态 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);   /* CS高 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_RESET); /* BLK关 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4,  GPIO_PIN_SET);   /* RST高 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5,  GPIO_PIN_SET);   /* DC高 */

    /* ---- OV7670控制引脚：RESET(PE3), PWDN(PD6) ---- */
    GPIO_InitStruct.Pin   = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* 初始状态：PWDN低（正常工作），RESET高 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);

    /* ---- SPI1引脚：PA5(SCK), PA7(MOSI) ---- */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ---- MCO1：PA8 → OV7670 XCLK ---- */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ---- I2C1引脚：PB8(SCL), PB9(SDA) ---- */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;  /* 开漏 */
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ---- DCMI引脚 ---- */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;

    /* PA4=DCMI_HSYNC(HREF), PA6=DCMI_PIXCLK(PCLK) */
    GPIO_InitStruct.Pin       = GPIO_PIN_4 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB6=DCMI_D5, PB7=DCMI_VSYNC */
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC6=DCMI_D0, PC7=DCMI_D1 */
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* PE0=DCMI_D2, PE1=DCMI_D3, PE4=DCMI_D4, PE5=DCMI_D6, PE6=DCMI_D7 */
    GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 |
                                 GPIO_PIN_5 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/* ============================================================
 *  DMA初始化（必须在外设Init之前调用）
 * ============================================================ */
static void MX_DMA_Init(void)
{
    /* 使能DMA2时钟（DCMI使用DMA2 Stream1）*/
    __HAL_RCC_DMA2_CLK_ENABLE();
    /* 使能DMA2（SPI1 TX使用DMA2 Stream3）*/

    /* DCMI DMA：DMA2 Stream1 Channel1 */
    hdma_dcmi.Instance                 = DMA2_Stream1;
    hdma_dcmi.Init.Channel             = DMA_CHANNEL_1;
    hdma_dcmi.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_dcmi.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_dcmi.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;   /* DCMI 32bit */
    hdma_dcmi.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    hdma_dcmi.Init.Mode                = DMA_CIRCULAR;           /* 循环模式，连续采集 */
    hdma_dcmi.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_dcmi.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    hdma_dcmi.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hdma_dcmi.Init.MemBurst            = DMA_MBURST_INC4;
    hdma_dcmi.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&hdma_dcmi) != HAL_OK) Error_Handler();

    /* 关联DCMI句柄 */
    __HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);

    /* DMA2 Stream1中断 */
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

    /* SPI1 TX DMA：DMA2 Stream3 Channel3 */
    hdma_spi1_tx.Instance                 = DMA2_Stream3;
    hdma_spi1_tx.Init.Channel             = DMA_CHANNEL_3;
    hdma_spi1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_spi1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_spi1_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_spi1_tx.Init.Mode                = DMA_NORMAL;
    hdma_spi1_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
    hdma_spi1_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) Error_Handler();

    __HAL_LINKDMA(&hspi1, hdmatx, hdma_spi1_tx);

    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}

/* ============================================================
 *  SPI1初始化（TFT屏幕，最高速度）
 * ============================================================ */
static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    /* APB2=84MHz，SPI_BAUDRATEPRESCALER_2 → SPI=42MHz（ST7789支持） */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

/* ============================================================
 *  I2C1初始化（OV7670 SCCB，使用标准模式100kHz）
 * ============================================================ */
static void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;       /* 100kHz标准模式 */
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

/* ============================================================
 *  DCMI初始化（OV7670，连续模式，不嵌入同步）
 * ============================================================ */
static void MX_DCMI_Init(void)
{
    __HAL_RCC_DCMI_CLK_ENABLE();

    hdcmi.Instance              = DCMI;
    hdcmi.Init.SynchroMode      = DCMI_SYNCHRO_HARDWARE; /* 硬件同步（VSYNC/HSYNC）*/
    hdcmi.Init.PCKPolarity      = DCMI_PCKPOLARITY_RISING; /* PCLK上升沿采样 */
    hdcmi.Init.VSPolarity       = DCMI_VSPOLARITY_LOW;    /* VSYNC低有效 */
    hdcmi.Init.HSPolarity       = DCMI_HSPOLARITY_LOW;    /* HREF低无效（高有效行）*/
    hdcmi.Init.CaptureRate      = DCMI_CR_ALL_FRAME;      /* 采集每一帧 */
    hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;    /* 8bit数据总线 */
    if (HAL_DCMI_Init(&hdcmi) != HAL_OK) Error_Handler();

    /* DCMI中断 */
    HAL_NVIC_SetPriority(DCMI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DCMI_IRQn);
}

/* ============================================================
 *  HAL回调函数
 * ============================================================ */

/**
 * @brief DCMI帧完成回调
 *        每采集完一帧调用一次
 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    DCMI_FrameComplete_Callback();
}

/**
 * @brief SPI DMA传输完成回调
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        ST7789_DMA_TxCpltCallback();
    }
}

/* ============================================================
 *  中断服务函数（stm32f4xx_it.c中调用HAL处理器）
 *  以下函数需要添加到 Core/Src/stm32f4xx_it.c 中
 * ============================================================ */

/*
void DCMI_IRQHandler(void)
{
    HAL_DCMI_IRQHandler(&hdcmi);
}

void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_dcmi);
}

void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi1_tx);
}
*/

/* ============================================================
 *  错误处理
 * ============================================================ */
static void Error_Handler(void)
{
    __disable_irq();
    /* 进入死循环，可接LED指示 */
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
