#ifndef __DCMI_CAPTURE_H
#define __DCMI_CAPTURE_H

#include "stm32f4xx_hal.h"
#include "ov7670.h"
#include <stdint.h>

/* ============================================================
 *  DCMI 帧采集管理
 *  DCMI连续模式 + DMA2 Stream1 (Channel1)
 *  帧缓冲存放在SRAM中
 * ============================================================ */

/* 帧缓冲（RGB565，320x240，153600字节）*/
/* 放在CCMRAM可提升速度，但DCMI DMA不能访问CCMRAM！必须用SRAM */
extern uint8_t g_frame_buf[OV7670_FRAME_SIZE];

/* 帧就绪标志 */
extern volatile uint8_t g_frame_ready;

/* ---------- 函数声明 ---------- */
void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi, DMA_HandleTypeDef *hdma);
void DCMI_Capture_Start(void);
void DCMI_Capture_Stop(void);
void DCMI_FrameComplete_Callback(void);

#endif /* __DCMI_CAPTURE_H */
