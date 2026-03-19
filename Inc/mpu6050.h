#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f4xx_hal.h"

extern I2C_HandleTypeDef hi2c2; 

uint8_t MPU6050_Init(void);
uint8_t MPU6050_CheckCollision(void); // 检测是否发生剧烈碰撞
void MPU6050_GetAttitude(float *pitch, float *roll); //获取角度
#endif