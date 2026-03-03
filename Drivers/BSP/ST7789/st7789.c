/**
 ******************************************************************************
 * @file    st7789.c
 * @brief   ST7789 TFT驱动实现
 *          硬件SPI1 + DMA传输
 *          支持320x240显示（用于显示OV7670 QVGA图像）
 ******************************************************************************
 */

#include "st7789.h"

static SPI_HandleTypeDef *g_hspi = NULL;
/* DMA传输完成标志 */
volatile uint8_t g_spi_dma_done = 1;

/* ============================================================
 *  底层SPI通信
 * ============================================================ */

/**
 * @brief 发送命令字节
 */
void ST7789_WriteCmd(uint8_t cmd)
{
    TFT_DC_LOW();   /* 命令模式 */
    TFT_CS_LOW();
    HAL_SPI_Transmit(g_hspi, &cmd, 1, HAL_MAX_DELAY);
    TFT_CS_HIGH();
}

/**
 * @brief 发送单字节数据
 */
void ST7789_WriteData8(uint8_t data)
{
    TFT_DC_HIGH();  /* 数据模式 */
    TFT_CS_LOW();
    HAL_SPI_Transmit(g_hspi, &data, 1, HAL_MAX_DELAY);
    TFT_CS_HIGH();
}

/**
 * @brief 发送双字节数据（16位颜色）
 */
void ST7789_WriteData16(uint16_t data)
{
    uint8_t buf[2];
    buf[0] = (data >> 8) & 0xFF;
    buf[1] = data & 0xFF;
    TFT_DC_HIGH();
    TFT_CS_LOW();
    HAL_SPI_Transmit(g_hspi, buf, 2, HAL_MAX_DELAY);
    TFT_CS_HIGH();
}

/**
 * @brief 发送多字节数据
 */
void ST7789_WriteData(uint8_t *data, uint16_t len)
{
    TFT_DC_HIGH();
    TFT_CS_LOW();
    HAL_SPI_Transmit(g_hspi, data, len, HAL_MAX_DELAY);
    TFT_CS_HIGH();
}

/* ============================================================
 *  初始化序列
 * ============================================================ */

/**
 * @brief ST7789初始化
 * @param hspi  SPI句柄指针（SPI1）
 */
void ST7789_Init(SPI_HandleTypeDef *hspi)
{
    g_hspi = hspi;

    /* 背光关闭 */
    TFT_BLK_OFF();
    TFT_CS_HIGH();
    TFT_DC_HIGH();

    /* 硬件复位 */
    TFT_RST_HIGH();
    HAL_Delay(10);
    TFT_RST_LOW();
    HAL_Delay(10);
    TFT_RST_HIGH();
    HAL_Delay(120);  /* 等待复位稳定 */

    /* ---- 软件复位 ---- */
    ST7789_WriteCmd(ST7789_SWRESET);
    HAL_Delay(150);

    /* ---- 退出睡眠 ---- */
    ST7789_WriteCmd(ST7789_SLPOUT);
    HAL_Delay(120);

    /* ---- 颜色格式：RGB565（16bit）---- */
    ST7789_WriteCmd(ST7789_COLMOD);
    ST7789_WriteData8(0x55);  /* 0x55 = 16bit/pixel */
    HAL_Delay(10);

    /* ---- 屏幕方向：横屏，显示OV7670 320x240 ---- */
    ST7789_WriteCmd(ST7789_MADCTL);
    /* 
     * MADCTL设置：
     * - MX: 列地址顺序镜像（横屏需要）
     * - MV: 行/列交换（横屏需要）
     * - BGR: 使用BGR颜色顺序（与OV7670的RGB565输出匹配时需要考虑字节顺序）
     * 
     * OV7670输出RGB565格式: RRRRRGGG GGGBBBBB (大端序)
     * ST7789期望: 对于RGB565模式，数据按接收顺序解析
     * 实际测试发现OV7670数据在ST7789上显示时需要BGR模式
     */
    ST7789_WriteData8(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_BGR); /* 横屏 + BGR模式 */

    /* ---- 帧频控制 ---- */
    ST7789_WriteCmd(ST7789_PORCTRL);
    {
        uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
        ST7789_WriteData(d, 5);
    }

    /* ---- Gate控制 ---- */
    ST7789_WriteCmd(ST7789_GCTRL);
    ST7789_WriteData8(0x35);

    /* ---- VCOMS ---- */
    ST7789_WriteCmd(ST7789_VCOMS);
    ST7789_WriteData8(0x19);

    /* ---- LCM控制 ---- */
    ST7789_WriteCmd(ST7789_LCMCTRL);
    ST7789_WriteData8(0x2C);

    /* ---- VDV/VRH使能 ---- */
    ST7789_WriteCmd(ST7789_VDVVRHEN);
    ST7789_WriteData8(0x01);

    /* ---- VRH设置 ---- */
    ST7789_WriteCmd(ST7789_VRHS);
    ST7789_WriteData8(0x12);

    /* ---- VDV设置 ---- */
    ST7789_WriteCmd(ST7789_VDVS);
    ST7789_WriteData8(0x20);

    /* ---- 帧率：60Hz ---- */
    ST7789_WriteCmd(ST7789_FRCTRL2);
    ST7789_WriteData8(0x0F);

    /* ---- 电源控制 ---- */
    ST7789_WriteCmd(ST7789_PWCTRL1);
    {
        uint8_t d[] = {0xA4, 0xA1};
        ST7789_WriteData(d, 2);
    }

    /* ---- 正极性Gamma ---- */
    ST7789_WriteCmd(ST7789_PVGAMCTRL);
    {
        uint8_t d[] = {0xD0, 0x04, 0x0D, 0x11, 0x13,
                       0x2B, 0x3F, 0x54, 0x4C, 0x18,
                       0x0D, 0x0B, 0x1F, 0x23};
        ST7789_WriteData(d, 14);
    }

    /* ---- 负极性Gamma ---- */
    ST7789_WriteCmd(ST7789_NVGAMCTRL);
    {
        uint8_t d[] = {0xD0, 0x04, 0x0C, 0x11, 0x13,
                       0x2C, 0x3F, 0x44, 0x51, 0x2F,
                       0x1F, 0x1F, 0x20, 0x23};
        ST7789_WriteData(d, 14);
    }

    /* ---- 关闭反色，使用正常显示 ---- */
    ST7789_WriteCmd(ST7789_INVOFF);
    HAL_Delay(10);

    /* ---- 正常显示模式 ---- */
    ST7789_WriteCmd(ST7789_NORON);
    HAL_Delay(10);

    /* ---- 开启显示 ---- */
    ST7789_WriteCmd(ST7789_DISPON);
    HAL_Delay(10);

    /* ---- 清屏为黑色 ---- */
    ST7789_Fill(COLOR_BLACK);

    /* ---- 背光开启 ---- */
    TFT_BLK_ON();
}

/* ============================================================
 *  显示控制
 * ============================================================ */

/**
 * @brief 设置显示窗口（地址范围）
 */
void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* 列地址 */
    ST7789_WriteCmd(ST7789_CASET);
    {
        uint8_t d[] = {(x0 >> 8), (x0 & 0xFF), (x1 >> 8), (x1 & 0xFF)};
        ST7789_WriteData(d, 4);
    }
    /* 行地址 */
    ST7789_WriteCmd(ST7789_RASET);
    {
        uint8_t d[] = {(y0 >> 8), (y0 & 0xFF), (y1 >> 8), (y1 & 0xFF)};
        ST7789_WriteData(d, 4);
    }
    /* 写入GRAM */
    ST7789_WriteCmd(ST7789_RAMWR);
}

/**
 * @brief 全屏填充单色
 */
void ST7789_Fill(uint16_t color)
{
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    ST7789_SetWindow(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);

    TFT_DC_HIGH();
    TFT_CS_LOW();

    /* 320x240 = 76800 像素，每像素2字节 */
    for (uint32_t i = 0; i < (uint32_t)ST7789_WIDTH * ST7789_HEIGHT; i++)
    {
        HAL_SPI_Transmit(g_hspi, &hi, 1, HAL_MAX_DELAY);
        HAL_SPI_Transmit(g_hspi, &lo, 1, HAL_MAX_DELAY);
    }

    TFT_CS_HIGH();
}

/**
 * @brief 画单个像素点
 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    ST7789_SetWindow(x, y, x, y);
    ST7789_WriteData16(color);
}

/**
 * @brief 写入帧缓冲（阻塞方式）
 *        用于将OV7670采集到的RGB565数据直接写入屏幕
 * @param buf  RGB565帧数据指针
 * @param len  字节数（320*240*2 = 153600）
 */
void ST7789_WriteFrameBuffer(uint8_t *buf, uint32_t len)
{
    ST7789_SetWindow(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);

    TFT_DC_HIGH();
    TFT_CS_LOW();

    /* SPI最大单次发送65535字节，需分包 */
    uint32_t remaining = len;
    uint8_t  *ptr      = buf;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? 65535 : (uint16_t)remaining;
        HAL_SPI_Transmit(g_hspi, ptr, chunk, HAL_MAX_DELAY);
        ptr       += chunk;
        remaining -= chunk;
    }

    TFT_CS_HIGH();
}

/**
 * @brief DMA传输完成回调（在HAL_SPI_TxCpltCallback中调用）
 */
void ST7789_DMA_TxCpltCallback(void)
{
    g_spi_dma_done = 1;
    TFT_CS_HIGH();
}

/**
 * @brief 写入帧缓冲（DMA方式，非阻塞）
 *        注意：调用前确保上次DMA传输已完成(g_spi_dma_done==1)
 */
void ST7789_WriteFrameBufferDMA(uint8_t *buf, uint32_t len)
{
    /* 等待上次DMA完成 */
    while (!g_spi_dma_done) {}
    g_spi_dma_done = 0;

    ST7789_SetWindow(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);

    TFT_DC_HIGH();
    TFT_CS_LOW();

    /* DMA最大65535字节，帧缓冲153600字节需分两次 */
    /* 第一包：65535字节 */
    /* 第二包：剩余字节 */
    /* 此处简化：使用阻塞分包，保证稳定 */
    uint32_t remaining = len;
    uint8_t  *ptr      = buf;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? 65535 : (uint16_t)remaining;
        HAL_SPI_Transmit_DMA(g_hspi, ptr, chunk);
        /* 等待本包完成 */
        while (!g_spi_dma_done) {}
        g_spi_dma_done = 0;
        ptr       += chunk;
        remaining -= chunk;
    }

    g_spi_dma_done = 1;
    TFT_CS_HIGH();
}

/**
 * @brief 设置屏幕旋转方向
 * @param rotation  0=竖屏, 1=横屏, 2=竖屏翻转, 3=横屏翻转
 */
void ST7789_SetRotation(uint8_t rotation)
{
    ST7789_WriteCmd(ST7789_MADCTL);
    switch (rotation)
    {
        case 0: ST7789_WriteData8(0x00); break;                           /* 竖屏 */
        case 1: ST7789_WriteData8(ST7789_MADCTL_MX | ST7789_MADCTL_MV); break; /* 横屏 */
        case 2: ST7789_WriteData8(ST7789_MADCTL_MX | ST7789_MADCTL_MY); break; /* 竖屏翻转 */
        case 3: ST7789_WriteData8(ST7789_MADCTL_MY | ST7789_MADCTL_MV); break; /* 横屏翻转 */
        default: break;
    }
}

/**
 * @brief 在屏幕特定区域显示图像数据 (用于局部刷新)
 */
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) return;
    
    // 限制绘制范围不超过屏幕边界
    uint16_t x_end = (x + w - 1 >= ST7789_WIDTH)  ? ST7789_WIDTH - 1  : x + w - 1;
    uint16_t y_end = (y + h - 1 >= ST7789_HEIGHT) ? ST7789_HEIGHT - 1 : y + h - 1;
    
    ST7789_SetWindow(x, y, x_end, y_end);

    TFT_DC_HIGH();
    TFT_CS_LOW();

    uint32_t len = w * h * 2; // RGB565 每个像素占 2 字节
    uint32_t remaining = len;
    uint8_t  *ptr      = data;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining > 65535) ? 65535 : (uint16_t)remaining;
        HAL_SPI_Transmit(g_hspi, ptr, chunk, HAL_MAX_DELAY);
        ptr       += chunk;
        remaining -= chunk;
    }

    TFT_CS_HIGH();
}