#ifndef __SENSOR_H
#define __SENSOR_H

#include "stm32f4xx_hal.h"

/* 获取环境亮度百分比 (0~100) */
uint8_t Sensor_GetLightPercent(void);

#endif