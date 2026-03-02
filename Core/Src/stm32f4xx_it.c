/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @brief   中断服务函数
 *          包含DCMI、DMA2中断处理
 ******************************************************************************
 */

#include "main.h"
#include "stm32f4xx_it.h"

/* ============================================================
 *  Cortex-M4 内核异常处理
 * ============================================================ */

void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ============================================================
 *  外设中断处理
 * ============================================================ */

/**
 * @brief DCMI帧/行/同步中断
 */
void DCMI_IRQHandler(void)
{
    HAL_DCMI_IRQHandler(&hdcmi);
}

/**
 * @brief DMA2 Stream1 中断（DCMI数据传输）
 */
void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_dcmi);
}

/**
 * @brief DMA2 Stream3 中断（SPI1 TX，刷TFT屏）
 */
void DMA2_Stream3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi1_tx);
}
