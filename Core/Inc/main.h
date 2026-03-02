#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"

/* 外设句柄声明（main.c中定义）*/
extern DCMI_HandleTypeDef  hdcmi;
extern DMA_HandleTypeDef   hdma_dcmi;
extern I2C_HandleTypeDef   hi2c1;
extern SPI_HandleTypeDef   hspi1;
extern DMA_HandleTypeDef   hdma_spi1_tx;

#endif /* __MAIN_H */
