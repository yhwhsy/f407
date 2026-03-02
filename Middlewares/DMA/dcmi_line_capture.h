/**
 ******************************************************************************
 * @file    dcmi_line_capture.c / .h（合并版）
 * @brief   行缓冲方案 —— 解决STM32F407主SRAM只有128KB放不下
 *          完整QVGA RGB565帧(153,600B)的问题。
 *
 *  原理：
 *    不使用整帧缓冲，改用"乒乓行缓冲"：
 *    - buf_A[320*2] = 640字节（第一行缓冲）
 *    - buf_B[320*2] = 640字节（第二行缓冲）
 *    DMA填满buf_A时，CPU把buf_A刷到屏幕；
 *    同时DMA填buf_B；循环交替。
 *    总缓冲：1280字节，SRAM占用极小。
 *
 *  注意：
 *    此方案需要DCMI能以行为单位触发中断（行结束中断DCMI_IT_LINE）。
 *    STM32F407 DCMI支持此功能。
 *
 *  适用场景：
 *    - 无外部SRAM
 *    - 希望降低延迟（边采集边显示，而非等整帧）
 *    - 分辨率较高（QVGA 320x240）
 *
 *  使用方法（替换main.c中的帧缓冲方案）：
 *    1. 将本文件加入项目
 *    2. main.c中用 LineCapture_Init / LineCapture_Start 代替帧缓冲函数
 *    3. 不需要 dcmi_capture.c
 ******************************************************************************
 */

#ifndef __DCMI_LINE_CAPTURE_H
#define __DCMI_LINE_CAPTURE_H

#include "stm32f4xx_hal.h"
#include "ov7670.h"
#include <stdint.h>

/* 每行字节数：320像素 × 2字节(RGB565) */
#define LINE_BUF_SIZE   (OV7670_WIDTH * 2)

/* 乒乓缓冲 */
extern uint8_t g_line_buf_A[LINE_BUF_SIZE];
extern uint8_t g_line_buf_B[LINE_BUF_SIZE];

void LineCapture_Init(DCMI_HandleTypeDef *hdcmi, DMA_HandleTypeDef *hdma,
                      SPI_HandleTypeDef  *hspi);
void LineCapture_Start(void);

/* 由DCMI行中断调用 */
void LineCapture_LineCallback(void);

#endif /* __DCMI_LINE_CAPTURE_H */


/* ============================================================
 *  实现（通常拆到 .c 文件，这里合并方便阅读）
 * ============================================================ */
#ifdef DCMI_LINE_CAPTURE_IMPL

#include "st7789.h"
#include "dcmi_line_capture.h"

__attribute__((aligned(4))) uint8_t g_line_buf_A[LINE_BUF_SIZE];
__attribute__((aligned(4))) uint8_t g_line_buf_B[LINE_BUF_SIZE];

static DCMI_HandleTypeDef *s_hdcmi;
static DMA_HandleTypeDef  *s_hdma;
static SPI_HandleTypeDef  *s_hspi;

static volatile uint8_t  s_active_buf  = 0;   /* 0=A当前DMA目标，1=B */
static volatile uint16_t s_line_count  = 0;   /* 当前行号 */
static volatile uint8_t  s_buf_ready   = 0;   /* 有行待刷屏 */
static uint8_t           *s_ready_ptr  = NULL;

void LineCapture_Init(DCMI_HandleTypeDef *hdcmi, DMA_HandleTypeDef *hdma,
                      SPI_HandleTypeDef *hspi)
{
    s_hdcmi = hdcmi;
    s_hdma  = hdma;
    s_hspi  = hspi;
    s_line_count  = 0;
    s_active_buf  = 0;
    s_buf_ready   = 0;
}

void LineCapture_Start(void)
{
    s_line_count = 0;
    s_active_buf = 0;
    /* 设置屏幕窗口覆盖整帧，从第0行开始 */
    ST7789_SetWindow(0, 0, OV7670_WIDTH - 1, OV7670_HEIGHT - 1);

    /* 启动DMA到buf_A，长度=一行word数 */
    HAL_DCMI_Start_DMA(s_hdcmi, DCMI_MODE_CONTINUOUS,
                       (uint32_t)g_line_buf_A,
                       LINE_BUF_SIZE / 4);

    /* 使能行中断 */
    __HAL_DCMI_ENABLE_IT(s_hdcmi, DCMI_IT_LINE);
}

/**
 * @brief 行完成中断回调（在HAL_DCMI_LineEventCallback中调用）
 *
 *  每当DCMI完成一行数据采集，此函数被调用：
 *  1. 将当前行数据标记为"已就绪"
 *  2. 切换DMA目标到另一个缓冲
 *  3. 主循环轮询 s_buf_ready，通过SPI发送该行到TFT
 */
void LineCapture_LineCallback(void)
{
    if (s_line_count >= OV7670_HEIGHT)
    {
        /* 一帧结束，回到第0行 */
        s_line_count = 0;
        /* 重置屏幕写地址到顶部 */
        ST7789_SetWindow(0, 0, OV7670_WIDTH - 1, OV7670_HEIGHT - 1);
    }

    /* 标记刚采集完的那个缓冲待刷屏 */
    if (s_active_buf == 0)
    {
        s_ready_ptr  = g_line_buf_A;
        s_active_buf = 1;
        /* 切换DMA到buf_B */
        /* 注：在连续模式下DMA地址切换需要停止重启或使用双缓冲DMA */
        /* 此处简化：先停止再重启（适合低帧率场景）*/
        HAL_DMA_Abort(s_hdma);
        HAL_DCMI_Start_DMA(s_hdcmi, DCMI_MODE_CONTINUOUS,
                           (uint32_t)g_line_buf_B,
                           LINE_BUF_SIZE / 4);
    }
    else
    {
        s_ready_ptr  = g_line_buf_B;
        s_active_buf = 0;
        HAL_DMA_Abort(s_hdma);
        HAL_DCMI_Start_DMA(s_hdcmi, DCMI_MODE_CONTINUOUS,
                           (uint32_t)g_line_buf_A,
                           LINE_BUF_SIZE / 4);
    }

    s_buf_ready = 1;
    s_line_count++;
}

/* ============================================================
 *  在main.c主循环中添加（替换帧缓冲版本）：
 *
 *   // 在初始化后：
 *   LineCapture_Init(&hdcmi, &hdma_dcmi, &hspi1);
 *   LineCapture_Start();
 *
 *   // 主循环：
 *   while (1) {
 *       if (s_buf_ready) {      // 注：需要将s_buf_ready/s_ready_ptr改为extern
 *           s_buf_ready = 0;
 *           // 直接发送一行数据到TFT（RAMWR状态已在SetWindow后持续）
 *           HAL_GPIO_WritePin(TFT_DC_GPIO_PORT, TFT_DC_PIN, GPIO_PIN_SET);
 *           HAL_GPIO_WritePin(TFT_CS_GPIO_PORT, TFT_CS_PIN, GPIO_PIN_RESET);
 *           HAL_SPI_Transmit(&hspi1, s_ready_ptr, LINE_BUF_SIZE, HAL_MAX_DELAY);
 *           HAL_GPIO_WritePin(TFT_CS_GPIO_PORT, TFT_CS_PIN, GPIO_PIN_SET);
 *       }
 *   }
 * ============================================================ */

#endif /* DCMI_LINE_CAPTURE_IMPL */
