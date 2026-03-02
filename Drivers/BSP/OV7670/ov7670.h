#ifndef __OV7670_H
#define __OV7670_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ============================================================
 *  OV7670 摄像头驱动
 *  接口：DCMI (数字摄像头接口) + I2C1 (SCCB配置)
 *  输出：RGB565，QVGA (320x240)
 * ============================================================ */

/* ---------- OV7670 SCCB 地址 ---------- */
#define OV7670_SCCB_ADDR    0x42  /* 写地址 (8bit，HAL会自动处理方向位) */
#define OV7670_SCCB_ADDR_7  0x21  /* 7bit地址 */

/* ---------- GPIO 控制引脚 ---------- */
#define OV7670_RESET_GPIO   GPIOE
#define OV7670_RESET_PIN    GPIO_PIN_3
#define OV7670_PWDN_GPIO    GPIOD
#define OV7670_PWDN_PIN     GPIO_PIN_6

#define OV7670_RESET_LOW()  HAL_GPIO_WritePin(OV7670_RESET_GPIO, OV7670_RESET_PIN, GPIO_PIN_RESET)
#define OV7670_RESET_HIGH() HAL_GPIO_WritePin(OV7670_RESET_GPIO, OV7670_RESET_PIN, GPIO_PIN_SET)
#define OV7670_PWDN_LOW()   HAL_GPIO_WritePin(OV7670_PWDN_GPIO,  OV7670_PWDN_PIN,  GPIO_PIN_RESET)
#define OV7670_PWDN_HIGH()  HAL_GPIO_WritePin(OV7670_PWDN_GPIO,  OV7670_PWDN_PIN,  GPIO_PIN_SET)

/* ---------- 图像参数 ---------- */
#define OV7670_WIDTH    320
#define OV7670_HEIGHT   240
/* RGB565: 2字节/像素，总帧大小 */
#define OV7670_FRAME_SIZE   (OV7670_WIDTH * OV7670_HEIGHT * 2)

/* ---------- OV7670 寄存器地址 ---------- */
#define REG_GAIN        0x00  /* AGC增益 */
#define REG_BLUE        0x01  /* AWB蓝色增益 */
#define REG_RED         0x02  /* AWB红色增益 */
#define REG_VREF        0x03  /* 垂直帧控制 */
#define REG_COM1        0x04  /* 公共控制1 */
#define REG_BAVE        0x05  /* U/B平均水平 */
#define REG_GbAVE       0x06  /* Y/Gb平均水平 */
#define REG_AECHH       0x07  /* 曝光值[15:10] */
#define REG_RAVE        0x08  /* V/R平均水平 */
#define REG_COM2        0x09  /* 公共控制2 */
#define REG_PID         0x0A  /* 产品ID高字节 (0x76) */
#define REG_VER         0x0B  /* 产品ID低字节 (0x73) */
#define REG_COM3        0x0C  /* 公共控制3 */
#define REG_COM4        0x0D  /* 公共控制4 */
#define REG_COM5        0x0E  /* 公共控制5 */
#define REG_COM6        0x0F  /* 公共控制6 */
#define REG_AECH        0x10  /* 曝光值[9:2] */
#define REG_CLKRC       0x11  /* 时钟控制 */
#define REG_COM7        0x12  /* 公共控制7 */
#define REG_COM8        0x13  /* 公共控制8 */
#define REG_COM9        0x14  /* 公共控制9 */
#define REG_COM10       0x15  /* 公共控制10 */
#define REG_HSTART      0x17  /* 水平帧开始 */
#define REG_HSTOP       0x18  /* 水平帧结束 */
#define REG_VSTART      0x19  /* 垂直帧开始 */
#define REG_VSTOP       0x1A  /* 垂直帧结束 */
#define REG_PSHFT       0x1B  /* 像素延迟 */
#define REG_MIDH        0x1C  /* 制造商ID高 */
#define REG_MIDL        0x1D  /* 制造商ID低 */
#define REG_MVFP        0x1E  /* 镜像/垂直翻转 */
#define REG_LAEC        0x1F  /* 帧内AEC限制 */
#define REG_ADCCTR0     0x20  /* ADC控制 */
#define REG_ADCCTR1     0x21
#define REG_ADCCTR2     0x22
#define REG_ADCCTR3     0x23
#define REG_AEW         0x24  /* AGC/AEC上限 */
#define REG_AEB         0x25  /* AGC/AEC下限 */
#define REG_VPT         0x26  /* 快速模式操作区 */
#define REG_BBIAS       0x27  /* B通道信号偏置 */
#define REG_GbBIAS      0x28  /* Gb通道信号偏置 */
#define REG_EXHCH       0x2A  /* 虚拟像素高字节 */
#define REG_EXHCL       0x2B  /* 虚拟像素低字节 */
#define REG_RBIAS       0x2C  /* R通道信号偏置 */
#define REG_ADVFL       0x2D  /* 添加虚拟行低字节 */
#define REG_ADVFH       0x2E  /* 添加虚拟行高字节 */
#define REG_YAVE        0x2F  /* Y/G平均水平 */
#define REG_HSYST       0x30  /* HREF关闭延迟 */
#define REG_HSYEN       0x31  /* HREF开始延迟 */
#define REG_HREF        0x32  /* HREF控制 */
#define REG_CHLF        0x33  /* 数组电流控制 */
#define REG_ARBLM       0x34  /* 阵列参考控制 */
#define REG_ADC         0x37  /* ADC控制 */
#define REG_ACOM        0x38  /* ADC和模拟通用控制 */
#define REG_OFON        0x39  /* ADC偏置控制 */
#define REG_TSLB        0x3A  /* 行缓冲测试选项 */
#define REG_COM11       0x3B  /* 公共控制11 */
#define REG_COM12       0x3C  /* 公共控制12 */
#define REG_COM13       0x3D  /* 公共控制13 */
#define REG_COM14       0x3E  /* 公共控制14 */
#define REG_EDGE        0x3F  /* 边缘增强调整 */
#define REG_COM15       0x40  /* 公共控制15 */
#define REG_COM16       0x41  /* 公共控制16 */
#define REG_COM17       0x42  /* 公共控制17 */
#define REG_AWBC1       0x43
#define REG_AWBC2       0x44
#define REG_AWBC3       0x45
#define REG_AWBC4       0x46
#define REG_AWBC5       0x47
#define REG_AWBC6       0x48
#define REG_AWBCTR3     0x4C
#define REG_AWBCTR2     0x4D
#define REG_AWBCTR1     0x4E
#define REG_AWBCTR0     0x4F
#define REG_GGAIN       0x6A  /* G通道AWB增益 */
#define REG_DBLV        0x6B  /* PLL控制 */
#define REG_REG76       0x76  /* OV保留 */
#define REG_RGB444      0x8C  /* RGB444控制 */
#define REG_HAECC1      0x9F  /* 直方图自动曝光/增益控制1 */
#define REG_HAECC2      0xA0  /* 直方图自动曝光/增益控制2 */
#define REG_SCALING_XSC 0x70  /* 水平缩放因子 */
#define REG_SCALING_YSC 0x71  /* 垂直缩放因子 */
#define REG_SCALING_DCWCTR 0x72 /* DCW控制 */
#define REG_SCALING_PC  0x73  /* 降采样控制 */
#define REG_SCALING_PCLK_DIV 0xA2 /* 缩放输出时钟分频 */
#define REG_SCALING_PCLK_DELAY 0xA2

/* COM7 位定义 */
#define COM7_RESET      0x80  /* 寄存器复位 */
#define COM7_FMT_QVGA   0x10  /* QVGA格式 */
#define COM7_FMT_CIF    0x20  /* CIF格式 */
#define COM7_RGB        0x04  /* RGB输出 */
#define COM7_YUV        0x00  /* YUV/YCbCr输出 */
#define COM7_BAYER      0x01  /* Bayer RAW */
#define COM7_PBAYER     0x05  /* 处理过的Bayer */

/* COM15 位定义 */
#define COM15_R10F0     0x00  /* 数据范围10到F0 */
#define COM15_R01FE     0x80  /* 数据范围01到FE */
#define COM15_R00FF     0xC0  /* 数据范围00到FF（推荐）*/
#define COM15_RGB565    0x10  /* RGB565输出 */
#define COM15_RGB555    0x30  /* RGB555输出 */

/* ---------- 函数声明 ---------- */
uint8_t OV7670_Init(I2C_HandleTypeDef *hi2c);
uint8_t OV7670_ReadReg(uint8_t reg, uint8_t *val);
uint8_t OV7670_WriteReg(uint8_t reg, uint8_t val);
uint8_t OV7670_CheckID(void);
void    OV7670_SetFormat_RGB565_QVGA(void);
void    OV7670_SetBrightness(int8_t brightness);
void    OV7670_SetSaturation(uint8_t saturation);
void    OV7670_SetFlip(uint8_t hmirror, uint8_t vflip);

#endif /* __OV7670_H */
