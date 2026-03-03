#ifndef __DCMI_CAPTURE_H
#define __DCMI_CAPTURE_H

#include "stm32f4xx_hal.h"
#include "ov7670.h"
#include <stdint.h>

/* ============================================================
 * DCMI 行缓冲采集管理 (乒乓缓冲 - 12.8KB)
 * ============================================================ */

/* 定义行缓冲区大小：一次缓存20行，320像素宽，每个像素2字节(RGB565) */
/* 内存只占 12.8 KB */
#define LINE_BUFFER_LINES 20
#define LINE_BUFFER_SIZE  (320 * LINE_BUFFER_LINES * 2)

extern uint8_t g_line_buf[LINE_BUFFER_SIZE];
extern volatile uint8_t flag_half_ready;
extern volatile uint8_t flag_full_ready;
extern DMA_HandleTypeDef hdma_dcmi_local;  /* DMA句柄，供中断使用 */

/* ---------- 函数声明 ---------- */
void DCMI_Capture_Init(DCMI_HandleTypeDef *hdcmi);
uint8_t DCMI_Capture_Start(void);
void DCMI_Capture_Stop(void);
uint8_t DCMI_CheckSync(void);

#endif /* __DCMI_CAPTURE_H */
