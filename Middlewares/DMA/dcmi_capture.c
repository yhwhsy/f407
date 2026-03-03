#include "dcmi_capture.h"

/* 分配 12.8KB 的行缓冲区 */
__attribute__((aligned(4)))
uint8_t g_line_buf[LINE_BUFFER_SIZE];

volatile uint8_t flag_half_ready = 0;
volatile uint8_t flag_full_ready = 0;

static DCMI_HandleTypeDef *g_hdcmi = NULL;
DMA_HandleTypeDef hdma_dcmi_local;  /* DMA句柄，供中断使用 */

/**
 * @brief 初始化 DCMI DMA
 */
static void DCMI_DMA_Init(void)
{
    /* 使能 DMA2 时钟 */
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    /* 配置 DMA2 Stream1 for DCMI */
    hdma_dcmi_local.Instance = DMA2_Stream1;
    hdma_dcmi_local.Init.Channel = DMA_CHANNEL_1;
    hdma_dcmi_local.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_dcmi_local.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dcmi_local.Init.MemInc = DMA_MINC_ENABLE;
    hdma_dcmi_local.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_dcmi_local.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_dcmi_local.Init.Mode = DMA_CIRCULAR;  /* 循环模式用于乒乓缓冲 */
    hdma_dcmi_local.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dcmi_local.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    
    HAL_DMA_Init(&hdma_dcmi_local);
    
    /* 配置 DMA 中断 */
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
}

/**
 * @brief 初始化 DCMI 捕获
 * @param hdcmi DCMI 句柄（CubeMX 生成）
 */
void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi)
{
    g_hdcmi = hdcmi;
    
    /* 重新配置 DCMI 参数（覆盖 CubeMX 配置） */
    hdcmi->Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
    hdcmi->Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;  /* 尝试下降沿 */
    hdcmi->Init.VSPolarity = DCMI_VSPOLARITY_LOW;
    hdcmi->Init.HSPolarity = DCMI_HSPOLARITY_LOW;
    hdcmi->Init.CaptureRate = DCMI_CR_ALL_FRAME;
    hdcmi->Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
    hdcmi->Init.JPEGMode = DCMI_JPEG_DISABLE;
    
    /* 重新初始化 DCMI */
    HAL_DCMI_Init(hdcmi);
    
    /* 初始化 DMA */
    DCMI_DMA_Init();
    
    /* 链接 DMA 到 DCMI */
    __HAL_LINKDMA(hdcmi, DMA_Handle, hdma_dcmi_local);
}

/**
 * @brief 启动 DCMI 捕获（循环模式）
 * @return 0=成功, 1=启动失败
 */
uint8_t DCMI_Capture_Start(void)
{
    flag_half_ready = 0;
    flag_full_ready = 0;
    
    /* 检查 DCMI 状态 */
    if (g_hdcmi->State != HAL_DCMI_STATE_READY)
    {
        return 1;  /* DCMI 未就绪 */
    }
    
    /* 检查 DMA 状态 */
    if (hdma_dcmi_local.State != HAL_DMA_STATE_READY)
    {
        return 2;  /* DMA 未就绪 */
    }
    
    /* 尝试快照模式（单帧） */
    HAL_StatusTypeDef status = HAL_DCMI_Start_DMA(g_hdcmi,
                       DCMI_MODE_SNAPSHOT,  /* 改为快照模式 */
                       (uint32_t)g_line_buf,
                       320 * 2 / 4);  /* 只捕获一行测试 */
    
    if (status != HAL_OK)
    {
        return 3;  /* 启动失败 */
    }
    
    return 0;  /* 成功 */
}

/**
 * @brief 检查 DCMI 是否接收到同步信号
 * @return 1=有HSYNC, 2=有VSYNC, 3=都有, 0=都没有
 */
uint8_t DCMI_CheckSync(void)
{
    uint8_t result = 0;
    
    if (__HAL_DCMI_GET_FLAG(g_hdcmi, DCMI_FLAG_HSYNC))
    {
        result |= 1;  /* 有HSYNC */
    }
    
    if (__HAL_DCMI_GET_FLAG(g_hdcmi, DCMI_FLAG_VSYNC))
    {
        result |= 2;  /* 有VSYNC */
    }
    
    return result;
}

/**
 * @brief 停止 DCMI 捕获
 */
void DCMI_Capture_Stop(void)
{
    HAL_DCMI_Stop(g_hdcmi);
}