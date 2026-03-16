#include "w25q64.h"

// 内部函数：SPI读写一个字节
static uint8_t SPI2_ReadWriteByte(uint8_t TxData) {
    uint8_t Rxdata;
    HAL_SPI_TransmitReceive(&hspi2, &TxData, &Rxdata, 1, 1000);
    return Rxdata;
}

// 内部函数：写使能
static void W25Q64_Write_Enable(void) {
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(0x06); 
    W25Q_CS_HIGH();
}

// 内部函数：等待空闲
static void W25Q64_Wait_Busy(void) {
    uint8_t status = 0;
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(0x05); // 读状态寄存器指令
    do {
        status = SPI2_ReadWriteByte(0xFF);
    } while ((status & 0x01) == 0x01); // 等待 BUSY 位清零
    W25Q_CS_HIGH();
}

uint16_t W25Q64_ReadID(void) {
    uint16_t Temp = 0;	  
    W25Q_CS_LOW();				    
    SPI2_ReadWriteByte(0x90); // 读ID指令
    SPI2_ReadWriteByte(0x00); 	    
    SPI2_ReadWriteByte(0x00); 	    
    SPI2_ReadWriteByte(0x00); 	 			   
    Temp |= SPI2_ReadWriteByte(0xFF) << 8;  
    Temp |= SPI2_ReadWriteByte(0xFF);	 
    W25Q_CS_HIGH();				    
    return Temp;
}

void W25Q64_Init(void) {
    W25Q_CS_HIGH();
    HAL_Delay(10);
    W25Q64_ReadID(); // 假读一次，唤醒芯片
}

// 擦除一个扇区 (4KB)
void W25Q64_EraseSector(uint32_t Dst_Addr) {
    Dst_Addr *= 4096;
    W25Q64_Write_Enable();
    W25Q64_Wait_Busy();
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(0x20); // 扇区擦除指令
    SPI2_ReadWriteByte((uint8_t)((Dst_Addr) >> 16));
    SPI2_ReadWriteByte((uint8_t)((Dst_Addr) >> 8));
    SPI2_ReadWriteByte((uint8_t)Dst_Addr);
    W25Q_CS_HIGH();
    W25Q64_Wait_Busy();
}

// 写一页 (最多256字节)
void W25Q64_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite) {
    W25Q64_Write_Enable();
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(0x02); // 页写指令
    SPI2_ReadWriteByte((uint8_t)((WriteAddr) >> 16));
    SPI2_ReadWriteByte((uint8_t)((WriteAddr) >> 8));
    SPI2_ReadWriteByte((uint8_t)WriteAddr);
    for (uint16_t i = 0; i < NumByteToWrite; i++) {
        SPI2_ReadWriteByte(pBuffer[i]);
    }
    W25Q_CS_HIGH();
    W25Q64_Wait_Busy();
}

// 连续读数据
void W25Q64_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead) {
    W25Q_CS_LOW();
    SPI2_ReadWriteByte(0x03); // 读指令
    SPI2_ReadWriteByte((uint8_t)((ReadAddr) >> 16));
    SPI2_ReadWriteByte((uint8_t)((ReadAddr) >> 8));
    SPI2_ReadWriteByte((uint8_t)ReadAddr);
    for (uint16_t i = 0; i < NumByteToRead; i++) {
        pBuffer[i] = SPI2_ReadWriteByte(0xFF);
    }
    W25Q_CS_HIGH();
}

// --- 专为保存 100KB 图像设计的顶层函数 ---
// photo_index: 照片索引(0~70), photo_data: 图像数组(102400字节)
void W25Q64_SavePhoto(uint32_t photo_index, uint8_t *photo_data) {
    // 100KB = 102400 字节，正好是 25 个 Sector (4096 * 25)
    uint32_t start_sector = photo_index * 25; 
    uint32_t write_addr = photo_index * 102400;
    
    // 1. 擦除照片对应的 25 个扇区
    for(int i = 0; i < 25; i++) {
        W25Q64_EraseSector(start_sector + i);
    }
    
    // 2. 将 102400 字节按每页 256 字节分批写入 (共 400 页)
    for(int i = 0; i < 400; i++) {
        W25Q64_WritePage(photo_data + (i * 256), write_addr + (i * 256), 256);
    }
}