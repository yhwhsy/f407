/**
 ******************************************************************************
 * @file    ov7670.c
 * @brief   OV7670 摄像头驱动实现
 *          使用软件I2C（GPIO模拟SCCB协议）
 *          输出格式：RGB565，分辨率：QVGA (320x240)
 ******************************************************************************
 */

#include "ov7670.h"

/* 软件I2C引脚定义 */
#define SCCB_SCL_PIN    GPIO_PIN_8
#define SCCB_SDA_PIN    GPIO_PIN_9
#define SCCB_GPIO_PORT  GPIOB

/* 延时函数微秒 */
static void SCCB_Delay(void)
{
    for(volatile int i = 0; i < 20; i++);  /* 约5-10us @168MHz */
}

/* SCL控制 */
#define SCCB_SCL_H()    HAL_GPIO_WritePin(SCCB_GPIO_PORT, SCCB_SCL_PIN, GPIO_PIN_SET)
#define SCCB_SCL_L()    HAL_GPIO_WritePin(SCCB_GPIO_PORT, SCCB_SCL_PIN, GPIO_PIN_RESET)

/* SDA控制 */
#define SCCB_SDA_H()    HAL_GPIO_WritePin(SCCB_GPIO_PORT, SCCB_SDA_PIN, GPIO_PIN_SET)
#define SCCB_SDA_L()    HAL_GPIO_WritePin(SCCB_GPIO_PORT, SCCB_SDA_PIN, GPIO_PIN_RESET)
#define SCCB_SDA_READ() HAL_GPIO_ReadPin(SCCB_GPIO_PORT, SCCB_SDA_PIN)

/* ============================================================
 *  软件I2C底层函数
 * ============================================================ */

/**
 * @brief 初始化软件I2C GPIO
 */
void SCCB_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();  /* PWDN引脚使用GPIOD */
    __HAL_RCC_GPIOE_CLK_ENABLE();  /* RESET引脚使用GPIOE */
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* SCL - 推挽输出 */
    GPIO_InitStruct.Pin = SCCB_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SCCB_GPIO_PORT, &GPIO_InitStruct);
    
    /* SDA - 开漏输出（双向） */
    GPIO_InitStruct.Pin = SCCB_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SCCB_GPIO_PORT, &GPIO_InitStruct);
    
    /* 初始状态：SCL=H, SDA=H */
    SCCB_SCL_H();
    SCCB_SDA_H();
    SCCB_Delay();
}

/**
 * @brief I2C起始信号
 */
void SCCB_Start(void)
{
    SCCB_SDA_H();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SDA_L();
    SCCB_Delay();
    SCCB_SCL_L();
    SCCB_Delay();
}

/**
 * @brief I2C停止信号
 */
void SCCB_Stop(void)
{
    SCCB_SDA_L();
    SCCB_SCL_L();
    SCCB_Delay();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SDA_H();
    SCCB_Delay();
}

/**
 * @brief 发送一个字节
 * @param dat 要发送的数据
 * @return 0=成功 (SCCB协议第9位是Don't care，忽略ACK)
 */
uint8_t SCCB_SendByte(uint8_t dat)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        if(dat & 0x80)
            SCCB_SDA_H();
        else
            SCCB_SDA_L();
        dat <<= 1;
        SCCB_Delay();
        SCCB_SCL_H();
        SCCB_Delay();
        SCCB_SCL_L();
        SCCB_Delay();
    }
    
    /* === 修改这里：忽略 ACK 位 (第9位 Don't care) === */
    SCCB_SDA_H();  /* 释放SDA */
    SCCB_Delay();
    SCCB_SCL_H();  /* 产生第9个时钟脉冲 */
    SCCB_Delay();
    SCCB_SCL_L();
    SCCB_Delay();
    
    return 0;  /* 强行认为发送成功，不由于无ACK而中断初始化 */
}

/**
 * @brief 接收一个字节
 * @param ack 发送的应答 0=ACK, 1=NACK
 * @return 接收到的数据
 */
uint8_t SCCB_ReceiveByte(uint8_t ack)
{
    uint8_t dat = 0;
    
    SCCB_SDA_H();  /* 释放SDA */
    
    for(uint8_t i = 0; i < 8; i++)
    {
        dat <<= 1;
        SCCB_SCL_H();
        SCCB_Delay();
        if(SCCB_SDA_READ())
            dat |= 0x01;
        SCCB_SCL_L();
        SCCB_Delay();
    }
    
    /* 发送ACK/NACK */
    if(ack)
        SCCB_SDA_H();
    else
        SCCB_SDA_L();
    SCCB_Delay();
    SCCB_SCL_H();
    SCCB_Delay();
    SCCB_SCL_L();
    SCCB_Delay();
    SCCB_SDA_H();
    
    return dat;
}

/* ============================================================
 *  SCCB (I2C) 寄存器读写
 * ============================================================ */

/**
 * @brief 写OV7670寄存器
 * @param reg  寄存器地址
 * @param val  写入值
 * @return 0=成功, 1=失败
 */
uint8_t OV7670_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t ret = 0;
    
    SCCB_Start();
    if(SCCB_SendByte(OV7670_SCCB_ADDR)) ret = 1;  /* 写地址 */
    if(SCCB_SendByte(reg)) ret = 1;               /* 寄存器地址 */
    if(SCCB_SendByte(val)) ret = 1;               /* 数据 */
    SCCB_Stop();
    
    HAL_Delay(1);  /* SCCB需要稳定时间 */
    return ret;
}

/**
 * @brief 读OV7670寄存器
 * @param reg  寄存器地址
 * @param val  读取值指针
 * @return 0=成功, 1=失败
 */
uint8_t OV7670_ReadReg(uint8_t reg, uint8_t *val)
{
    uint8_t ret = 0;
    
    /* 阶段1：写寄存器地址 */
    SCCB_Start();
    if(SCCB_SendByte(OV7670_SCCB_ADDR)) ret = 1;
    if(SCCB_SendByte(reg)) ret = 1;
    SCCB_Stop();
    
    HAL_Delay(1);
    
    /* 阶段2：读数据 */
    SCCB_Start();
    if(SCCB_SendByte(OV7670_SCCB_ADDR | 0x01)) ret = 1;  /* 读地址 */
    *val = SCCB_ReceiveByte(1);  /* 接收并发送NACK */
    SCCB_Stop();
    
    return ret;
}

/* ============================================================
 *  OV7670 RGB565 QVGA 寄存器配置表
 *  格式：{寄存器地址, 寄存器值}
 *  0xFF结尾
 * ============================================================ */
typedef struct {
    uint8_t reg;
    uint8_t val;
} RegVal_t;

static const RegVal_t ov7670_rgb565_qvga_regs[] = {
    /* 软件复位与时钟配置 */
    {REG_COM7,    COM7_RESET}, // 0x12, 0x80 (宏定义的值通常是0x80)
    {0x3a,        0x04},
    {0x40,        0xd0},
    
    /* 输出格式：RGB565，QVGA*/
    {REG_COM7,    0x14}, // 原来的 COM7_FMT_QVGA | COM7_RGB
    {0x32,        0x80},
    {0x17,        0x16}, // HSTART
    {0x18,        0x04}, // HSTOP
    {0x19,        0x02}, // VSTART
    {0x1a,        0x7b}, // VSTOP
    {0x03,        0x06}, // VREF
    {0x0c,        0x04}, // COM3
    {0x3e,        0x00}, // COM14
    
    /* 缩放控制 */
    {0x70,        0x3a}, // SCALING_XSC
    {0x71,        0x35}, // SCALING_YSC
    {0x72,        0x11}, // SCALING_DCWCTR
    {0x73,        0x00}, // SCALING_PCLK_DIV
    {0xa2,        0x02}, // SCALING_PCLK_DELAY
    
    /* 时钟分频*/
    {REG_CLKRC,   0x81}, // 0x11
    
    /* Gamma 曲线调校 */
    {0x7a,        0x20},
    {0x7b,        0x1c},
    {0x7c,        0x28},
    {0x7d,        0x3c},
    {0x7e,        0x55},
    {0x7f,        0x68},
    {0x80,        0x76},
    {0x81,        0x80},
    {0x82,        0x88},
    {0x83,        0x8f},
    {0x84,        0x96},
    {0x85,        0xa3},
    {0x86,        0xaf},
    {0x87,        0xc4},
    {0x88,        0xd7},
    {0x89,        0xe8},
    
    /* AEC/AGC/AWB 控制与设定 */
    {REG_COM8,    0xe0}, // 0x13
    {REG_GAIN,    0x00}, // 0x00
    {0x10,        0x00}, // AECH
    {0x0d,        0x00}, // COM4
    {REG_COM9,    0x28}, // 0x14 
    {0xa5,        0x05}, // BD50MAX
    {0xab,        0x07}, // BD60MAX
    {0x24,        0x75}, // AEW
    {0x25,        0x63}, // AEB
    {0x26,        0xA5}, // VPT
    {0x9f,        0x78}, // HAECC1
    {0xa0,        0x68}, // HAECC2
    {0xa1,        0x03}, // HAECC3
    {0xa6,        0xdf}, // HAECC4
    {0xa7,        0xdf}, // HAECC5
    {0xa8,        0xf0}, // HAECC6
    {0xa9,        0x90}, // HAECC7
    {0xaa,        0x94}, // HAECC8
    {REG_COM8,    0xe5}, // 0x13
    {0x0e,        0x61}, // COM5
    {0x0f,        0x4b}, // COM6
    {0x16,        0x02},
    {0x1e,        0x07}, // MVFP (控制镜像/翻转)
    {0x21,        0x02}, // ADCCTR1
    {0x22,        0x91}, // ADCCTR2
    {0x29,        0x07}, // RSVD
    {0x33,        0x0b}, // AWBC1
    {0x35,        0x0b}, // AWBC3
    {0x37,        0x1d}, // ADC
    {0x38,        0x71}, // ACOM
    {0x39,        0x2a}, // OFON
    {0x3c,        0x78}, // COM12
    {0x4d,        0x40}, // RSVD
    {0x4e,        0x20}, // RSVD
    {0x69,        0x00}, // GFIX
    {0x6b,        0x60}, // DBLV
    {0x74,        0x19}, // REG74
    {0x8d,        0x4f}, // RSVD
    {0x8e,        0x00}, // RSVD
    {0x8f,        0x00}, // RSVD
    {0x90,        0x00}, // RSVD
    {0x91,        0x00}, // RSVD
    {0x92,        0x00}, // RSVD
    {0x96,        0x00}, // RSVD
    {0x9a,        0x80}, // RSVD
    {0xb0,        0x84}, // RSVD
    {0xb1,        0x0c}, // ABLC1
    {0xb2,        0x0e}, // RSVD
    {0xb3,        0x82}, // THL_ST
    {0xb8,        0x0a}, // RSVD

    /* 额外寄存器调校 */
    {0x43,        0x14},
    {0x44,        0xf0},
    {0x45,        0x34},
    {0x46,        0x58},
    {0x47,        0x28},
    {0x48,        0x3a},
    {0x59,        0x88},
    {0x5a,        0x88},
    {0x5b,        0x44},
    {0x5c,        0x67},
    {0x5d,        0x49},
    {0x5e,        0x0e},
    {0x64,        0x04},
    {0x65,        0x20},
    {0x66,        0x05},
    {0x94,        0x04},
    {0x95,        0x08},
    {0x6c,        0x0a}, // AWBCTR2
    {0x6d,        0x55}, // AWBCTR3
    {0x6e,        0x11}, // AWBCTR1
    {0x6f,        0x9f}, // AWBCTR0
    {0x6a,        0x40}, // GGAIN
    {0x01,        0x40}, // BLUE
    {0x02,        0x40}, // RED
    {REG_COM8,    0xe7}, // 0x13
    {REG_COM10,   0x0A}, // 0x15
    /* 颜色矩阵 */
    {0x4f,        0x80}, // MTX1
    {0x50,        0x80}, // MTX2
    {0x51,        0x00}, // MTX3
    {0x52,        0x22}, // MTX4
    {0x53,        0x5e}, // MTX5
    {0x54,        0x80}, // MTX6
    {0x58,        0x9e}, // MTXS
    /* 镜头阴影校正 (Lens Shading Correction) 等 */
    {0x41,        0x08},
    {0x3f,        0x00}, // EDGE
    {0x75,        0x05},
    {0x76,        0xe1},
    {0x4c,        0x00},
    {0x77,        0x01},
    {0x3d,        0xc2}, // COM13
    {0x4b,        0x09},
    {0xc9,        0x60},
    {0x41,        0x38},
    {0x56,        0x40},
    
    /* 杂项 */
    {0x34,        0x11}, // ARCOM2
    {0x3b,        0x02}, // COM11
    {0xa4,        0x89}, // NT_CTRL
    {0x96,        0x00},
    {0x97,        0x30},
    {0x98,        0x20},
    {0x99,        0x30},
    {0x9a,        0x84},
    {0x9b,        0x29},
    {0x9c,        0x03},
    {0x9d,        0x4c},
    {0x9e,        0x3f},
    {0x78,        0x04},
    
    /* 降噪及其他边缘增强参数 */
    {0x79,        0x01},
    {0xc8,        0xf0},
    {0x79,        0x0f},
    {0xc8,        0x00},
    {0x79,        0x10},
    {0xc8,        0x7e},
    {0x79,        0x0a},
    {0xc8,        0x80},
    {0x79,        0x0b},
    {0xc8,        0x01},
    {0x79,        0x0c},
    {0xc8,        0x0f},
    {0x79,        0x0d},
    {0xc8,        0x20},
    {0x79,        0x09},
    {0xc8,        0x80},
    {0x79,        0x02},
    {0xc8,        0xc0},
    {0x79,        0x03},
    {0xc8,        0x40},
    {0x79,        0x05},
    {0xc8,        0x30},
    {0x79,        0x26}, 
    {0x09,        0x00}, // COM2
    
    /* 结束标记 */
    {0xFF, 0xFF}
};
/* ============================================================
 *  公共API
 * ============================================================ */

/**
 * @brief OV7670初始化
 * @return 0=成功, 1=ID读取失败, 2=寄存器写入失败
 */
uint8_t OV7670_Init(void)
{
    /* 初始化软件I2C引脚 */
    SCCB_Init();

    /* 上电序列 */
    OV7670_PWDN_LOW();   /* 退出掉电模式 */
    HAL_Delay(10);
    OV7670_RESET_LOW();  /* 硬件复位 */
    HAL_Delay(10);
    OV7670_RESET_HIGH();
    HAL_Delay(30);       /* 等待稳定 */

    /* 验证芯片ID */
    if (OV7670_CheckID() != 0)
    {
        return 1;  /* ID错误 */
    }

    /* 写入RGB565 QVGA配置 */
    const RegVal_t *p = ov7670_rgb565_qvga_regs;
    while (p->reg != 0xFF)
    {
        /* 软件复位后需要等待 */
        if (p->reg == REG_COM7 && p->val == COM7_RESET)
        {
            OV7670_WriteReg(p->reg, p->val);
            HAL_Delay(30);
        }
        else
        {
            if (OV7670_WriteReg(p->reg, p->val) != 0)
            {
                return 2;
            }
        }
        p++;
    }

    HAL_Delay(100);  /* 等待曝光稳定 */
    return 0;
}

/**
 * @brief 检查OV7670芯片ID
 * @return 0=正确 (PID=0x76, VER=0x73), 非0=错误
 */
uint8_t OV7670_CheckID(void)
{
    uint8_t pid = 0, ver = 0;
    OV7670_ReadReg(REG_PID, &pid);
    OV7670_ReadReg(REG_VER, &ver);

    if (pid == 0x76 && ver == 0x73)
    {
        return 0;  /* OV7670 */
    }
    /* 部分OV7675也兼容 */
    if (pid == 0x76 && ver == 0x75)
    {
        return 0;  /* OV7675 */
    }
    return 1;
}

/**
 * @brief 强制设置为RGB565 QVGA格式（调用Init后无需再调用）
 */
void OV7670_SetFormat_RGB565_QVGA(void)
{
    OV7670_WriteReg(REG_COM7, COM7_FMT_QVGA | COM7_RGB);
    HAL_Delay(30);
    OV7670_WriteReg(REG_COM15, COM15_RGB565 | COM15_R00FF);
}

/**
 * @brief 设置亮度
 * @param brightness  -4~+4，0为默认
 */
void OV7670_SetBrightness(int8_t brightness)
{
    uint8_t val;
    if (brightness >= 0)
    {
        val = (uint8_t)(brightness << 4) & 0xF0;
        OV7670_WriteReg(0x55, val);  /* BRIGHT */
        OV7670_WriteReg(0x56, 0x40); /* CONTRAS：正亮度不反转 */
    }
    else
    {
        val = (uint8_t)((-brightness) << 4) | 0x08;
        OV7670_WriteReg(0x55, val);
        OV7670_WriteReg(0x56, 0x40);
    }
}

/**
 * @brief 设置饱和度
 * @param saturation  0~255，默认0x40
 */
void OV7670_SetSaturation(uint8_t saturation)
{
    OV7670_WriteReg(REG_GGAIN, saturation);
}

/**
 * @brief 设置镜像/翻转
 * @param hmirror  1=水平镜像
 * @param vflip    1=垂直翻转
 */
void OV7670_SetFlip(uint8_t hmirror, uint8_t vflip)
{
    uint8_t val = 0;
    if (hmirror) val |= 0x20;
    if (vflip)   val |= 0x10;
    OV7670_WriteReg(REG_MVFP, val);
}
