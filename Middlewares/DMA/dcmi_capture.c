#include "dcmi_capture.h"

/* 分配 12.8KB 的行缓冲区 */
__attribute__((aligned(4)))
uint8_t g_line_buf[LINE_BUFFER_SIZE];

volatile uint8_t flag_half_ready = 0;
volatile uint8_t flag_full_ready = 0;

/* DCMI DMA 句柄 - 供 main.c 使用 */
DMA_HandleTypeDef hdma_dcmi;

static DCMI_HandleTypeDef *g_hdcmi = NULL;
static DMA_HandleTypeDef  *g_hdma  = NULL;

void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi, DMA_HandleTypeDef *hdma)
{
    g_hdcmi = hdcmi;
    g_hdma  = hdma;
}

void DCMI_Capture_Start(void)
{
    flag_half_ready = 0;
    flag_full_ready = 0;
    
    /* 启动 DCMI 的 DMA 连续传输，使用循环模式 (Circular) */
    HAL_DCMI_Start_DMA(g_hdcmi,
                       DCMI_MODE_CONTINUOUS,
                       (uint32_t)g_line_buf,
                       LINE_BUFFER_SIZE / 4);
}

void DCMI_Capture_Stop(void)
{
    HAL_DCMI_Stop(g_hdcmi);
}

/* ============================================================
 * DMA 乒乓中断回调函数
 * ============================================================ */

/* 1. DMA 传输过半中断回调 (此时前半段填满) */
void HAL_DCMI_HalfEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    flag_half_ready = 1;
}

/* 2. DMA 传输完成中断回调 (此时后半段填满) */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    flag_full_ready = 1;
}