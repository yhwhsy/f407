/**
 ******************************************************************************
 * @file    dcmi_capture.c
 * @brief   DCMI连续采集 + DMA帧缓冲管理
 *
 *  STM32F407 DCMI外设说明：
 *  - DCMI连接到AHB2总线
 *  - DMA2 Stream1 Channel1 专用于DCMI
 *  - 帧缓冲必须在主SRAM（0x20000000），不能用CCMRAM
 *  - DMA传输单位：32bit word，帧大小需按4字节对齐
 *
 *  OV7670 QVGA RGB565帧大小：
 *  320 * 240 * 2 = 153600 字节 = 38400 个32bit words
 ******************************************************************************
 */

#include "dcmi_capture.h"

/* ============================================================
 *  帧缓冲（必须在主SRAM，DMA可访问区域）
 *  153600字节，4字节对齐
 * ============================================================ */
__attribute__((aligned(4)))
uint8_t g_frame_buf[OV7670_FRAME_SIZE];

/* 帧就绪标志（在DCMI帧完成中断中置位）*/
volatile uint8_t g_frame_ready = 0;

static DCMI_HandleTypeDef *g_hdcmi = NULL;
static DMA_HandleTypeDef  *g_hdma  = NULL;

/**
 * @brief 初始化DCMI采集（句柄已由CubeMX配置好）
 */
void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi, DMA_HandleTypeDef *hdma)
{
    g_hdcmi = hdcmi;
    g_hdma  = hdma;
    g_frame_ready = 0;
}

/**
 * @brief 启动DCMI连续采集
 *        DMA将数据搬运到g_frame_buf
 */
void DCMI_Capture_Start(void)
{
    g_frame_ready = 0;

    /*
     * HAL_DCMI_Start_DMA 参数：
     *   DCMI_MODE_CONTINUOUS = 连续采集
     *   pData = 帧缓冲地址（必须32位对齐）
     *   Length = 以32bit word为单位的帧大小
     *            153600 / 4 = 38400
     */
    HAL_DCMI_Start_DMA(g_hdcmi,
                       DCMI_MODE_CONTINUOUS,
                       (uint32_t)g_frame_buf,
                       OV7670_FRAME_SIZE / 4);
}

/**
 * @brief 停止DCMI采集
 */
void DCMI_Capture_Stop(void)
{
    HAL_DCMI_Stop(g_hdcmi);
}

/**
 * @brief 帧完成回调（由HAL_DCMI_FrameEventCallback调用）
 */
void DCMI_FrameComplete_Callback(void)
{
    g_frame_ready = 1;
}
