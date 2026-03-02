#ifndef __ST7789_H
#define __ST7789_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ============================================================
 *  ST7789 TFT 驱动  (240x320 / 2.4寸 / SPI接口)
 *  硬件SPI1 + DMA
 * ============================================================ */

/* ---------- 引脚宏定义 ---------- */
#define TFT_CS_GPIO_PORT    GPIOB
#define TFT_CS_PIN          GPIO_PIN_12

#define TFT_DC_GPIO_PORT    GPIOC
#define TFT_DC_PIN          GPIO_PIN_5

#define TFT_RST_GPIO_PORT   GPIOC
#define TFT_RST_PIN         GPIO_PIN_4

#define TFT_BLK_GPIO_PORT   GPIOB
#define TFT_BLK_PIN         GPIO_PIN_0

/* ---------- 操作宏 ---------- */
#define TFT_CS_LOW()    HAL_GPIO_WritePin(TFT_CS_GPIO_PORT,  TFT_CS_PIN,  GPIO_PIN_RESET)
#define TFT_CS_HIGH()   HAL_GPIO_WritePin(TFT_CS_GPIO_PORT,  TFT_CS_PIN,  GPIO_PIN_SET)
#define TFT_DC_LOW()    HAL_GPIO_WritePin(TFT_DC_GPIO_PORT,  TFT_DC_PIN,  GPIO_PIN_RESET)
#define TFT_DC_HIGH()   HAL_GPIO_WritePin(TFT_DC_GPIO_PORT,  TFT_DC_PIN,  GPIO_PIN_SET)
#define TFT_RST_LOW()   HAL_GPIO_WritePin(TFT_RST_GPIO_PORT, TFT_RST_PIN, GPIO_PIN_RESET)
#define TFT_RST_HIGH()  HAL_GPIO_WritePin(TFT_RST_GPIO_PORT, TFT_RST_PIN, GPIO_PIN_SET)
#define TFT_BLK_ON()    HAL_GPIO_WritePin(TFT_BLK_GPIO_PORT, TFT_BLK_PIN, GPIO_PIN_SET)
#define TFT_BLK_OFF()   HAL_GPIO_WritePin(TFT_BLK_GPIO_PORT, TFT_BLK_PIN, GPIO_PIN_RESET)

/* ---------- 屏幕尺寸 ---------- */
#define ST7789_WIDTH    320
#define ST7789_HEIGHT   240

/* ---------- 颜色定义 (RGB565) ---------- */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

/* ---------- ST7789 命令 ---------- */
#define ST7789_NOP        0x00
#define ST7789_SWRESET    0x01
#define ST7789_SLPIN      0x10
#define ST7789_SLPOUT     0x11
#define ST7789_NORON      0x13
#define ST7789_INVOFF     0x20
#define ST7789_INVON      0x21
#define ST7789_DISPOFF    0x28
#define ST7789_DISPON     0x29
#define ST7789_CASET      0x2A
#define ST7789_RASET      0x2B
#define ST7789_RAMWR      0x2C
#define ST7789_RAMRD      0x2E
#define ST7789_MADCTL     0x36
#define ST7789_COLMOD     0x3A
#define ST7789_PORCTRL    0xB2
#define ST7789_GCTRL      0xB7
#define ST7789_VCOMS      0xBB
#define ST7789_LCMCTRL    0xC0
#define ST7789_VDVVRHEN   0xC2
#define ST7789_VRHS       0xC3
#define ST7789_VDVS       0xC4
#define ST7789_FRCTRL2    0xC6
#define ST7789_PWCTRL1    0xD0
#define ST7789_PVGAMCTRL  0xE0
#define ST7789_NVGAMCTRL  0xE1

/* MADCTL方向 */
#define ST7789_MADCTL_MY  0x80
#define ST7789_MADCTL_MX  0x40
#define ST7789_MADCTL_MV  0x20
#define ST7789_MADCTL_ML  0x10
#define ST7789_MADCTL_BGR 0x08

/* ---------- 函数声明 ---------- */
void ST7789_Init(SPI_HandleTypeDef *hspi);
void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ST7789_Fill(uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_WriteFrameBuffer(uint8_t *buf, uint32_t len);
void ST7789_WriteFrameBufferDMA(uint8_t *buf, uint32_t len);
void ST7789_SetRotation(uint8_t rotation); /* 0=Portrait, 1=Landscape, 2,3=翻转 */
void ST7789_DMA_TxCpltCallback(void);
/* 内部函数（供驱动内部使用）*/
void ST7789_WriteCmd(uint8_t cmd);
void ST7789_WriteData(uint8_t *data, uint16_t len);
void ST7789_WriteData8(uint8_t data);
void ST7789_WriteData16(uint16_t data);
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data);
#endif /* __ST7789_H */
