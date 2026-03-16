#ifndef __UI_H
#define __UI_H

#include "stm32f4xx_hal.h"

void UI_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color);
void UI_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color);

#endif