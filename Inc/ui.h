#ifndef __UI_H
#define __UI_H

#include "stm32f4xx_hal.h"

/* 声明直接画字符串的函数 */
void UI_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color);

#endif