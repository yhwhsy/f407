#include "dcmi_capture.h"

/* 分配 12.8KB 的行缓冲区 */
__attribute__((aligned(4)))
uint8_t g_line_buf[LINE_BUFFER_SIZE];

volatile uint8_t flag_half_ready = 0;
volatile uint8_t flag_full_ready = 0;

static DCMI_HandleTypeDef *g_hdcmi = NULL;

/**
 * @brief 初始化 DCMI 捕获
 * @param hdcmi DCMI 句柄（CubeMX 生成）
 */
void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi)
{
    g_hdcmi = hdcmi;
    
    /* CubeMX 已经初始化了 DMA，这里只需要链接到 DCMI */
    /* DMA 配置在 dcmi.c 的 HAL_DCMI_MspInit 中完成 */
}

/**
 * @brief 启动 DCMI 捕获（循环模式）
 */
void DCMI_Capture_Start(void)
{
    flag_half_ready = 0;
    flag_full_ready = 0;
    
    /* 启动 DCMI 的 DMA 连续传输 */
    /* LINE_BUFFER_SIZE/4 是因为DCMI_DR是32位，DMA按32位传输 */
    HAL_DCMI_Start_DMA(g_hdcmi,
                       DCMI_MODE_CONTINUOUS,
                       (uint32_t)g_line_buf,
                       LINE_BUFFER_SIZE / 4);
}

/**
 * @brief 停止 DCMI 捕获
 */
void DCMI_Capture_Stop(void)
{
    HAL_DCMI_Stop(g_hdcmi);
}