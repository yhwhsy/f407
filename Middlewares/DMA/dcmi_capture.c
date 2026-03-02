#include "dcmi_capture.h"

/* 分配 12.8KB 的行缓冲区 */
__attribute__((aligned(4)))
uint8_t g_line_buf[LINE_BUFFER_SIZE];

volatile uint8_t flag_half_ready = 0;
volatile uint8_t flag_full_ready = 0;

/* DCMI DMA 句柄 - 供 main.c 使用 */
DMA_HandleTypeDef hdma_dcmi;

static DCMI_HandleTypeDef *g_hdcmi = NULL;

void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi, DMA_HandleTypeDef *hdma)
{
    g_hdcmi = hdcmi;
    
    /* 初始化DMA句柄 - DCMI使用DMA2 Stream1 Channel1 */
    hdma_dcmi.Instance = DMA2_Stream1;
    hdma_dcmi.Init.Channel = DMA_CHANNEL_1;
    hdma_dcmi.Init.Direction = DMA_PERIPH_TO_MEMORY;  /* 外设到内存 */
    hdma_dcmi.Init.PeriphInc = DMA_PINC_DISABLE;      /* 外设地址不递增 */
    hdma_dcmi.Init.MemInc = DMA_MINC_ENABLE;          /* 内存地址递增 */
    hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;  /* DCMI_DR是32位 */
    hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;     /* 内存32位对齐 */
    hdma_dcmi.Init.Mode = DMA_CIRCULAR;               /* 循环模式用于乒乓缓冲 */
    hdma_dcmi.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dcmi.Init.FIFOMode = DMA_FIFOMODE_ENABLE;    /* 启用FIFO */
    hdma_dcmi.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_dcmi.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_dcmi.Init.PeriphBurst = DMA_PBURST_SINGLE;
    
    /* 初始化DMA */
    HAL_DMA_Init(&hdma_dcmi);
    
    /* 链接DMA到DCMI外设 */
    __HAL_LINKDMA(hdcmi, DMA_Handle, hdma_dcmi);
}

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

void DCMI_Capture_Stop(void)
{
    HAL_DCMI_Stop(g_hdcmi);
}
