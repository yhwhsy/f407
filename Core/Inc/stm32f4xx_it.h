#ifndef __STM32F4XX_IT_H
#define __STM32F4XX_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void DCMI_IRQHandler(void);
void DMA2_Stream1_IRQHandler(void);
void DMA2_Stream3_IRQHandler(void);

#endif /* __STM32F4XX_IT_H */
