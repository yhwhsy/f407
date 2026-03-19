#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f4xx_hal.h"

extern SPI_HandleTypeDef hspi2;

#define W25Q_CS_LOW()   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET)
#define W25Q_CS_HIGH()  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET)

uint16_t W25Q64_ReadID(void);
void W25Q64_Init(void);
void W25Q64_EraseSector(uint32_t Dst_Addr);
void W25Q64_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25Q64_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
void W25Q64_SavePhoto(uint32_t photo_index, uint8_t *photo_data); // 专门用于存照片的函数

#endif