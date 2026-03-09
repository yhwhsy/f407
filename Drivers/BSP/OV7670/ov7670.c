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

/* 延时函数（微秒级） */
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
    /* ── 格式 / 字节序 ── */
    {0x3a, 0x04},           /* TSLB: UYVY字节序 */
    {0x40, 0xd0},           /* COM15: RGB565, 输出范围00-FF */
    {0x12, 0x14},           /* COM7: QVGA, RGB输出 */
    {0x32, 0x80},           /* HREF控制 */
    {0x17, 0x16},           /* HSTART */
    {0x18, 0x04},           /* HSTOP */
    {0x19, 0x02},           /* VSTRT */
    {0x1a, 0x7b},           /* VSTOP */
    {0x03, 0x06},           /* VREF */
    {0x0c, 0x04},           /* COM3: 缩放使能 */
    {0x3e, 0x00},           /* COM14: PCLK不分频 */

    /* ── 缩放与时钟 ── */
    {0x70, 0x3a},           /* SCALING_XSC */
    {0x71, 0x35},           /* SCALING_YSC */
    {0x72, 0x11},           /* SCALING_DCWCTR: H/V各降采样2x */
    {0x73, 0x00},           /* SCALING_PCLK_DIV */
    {0xa2, 0x02},           /* SCALING_PCLK_DELAY */
    {0x11, 0x81},           /* CLKRC: 内部PLL使能, 预分频/2 */

    /* ── Gamma曲线 (16点) ── */
    {0x7a, 0x20},           /* SLOP */
    {0x7b, 0x1c},           /* GAM1 */
    {0x7c, 0x28},           /* GAM2 */
    {0x7d, 0x3c},           /* GAM3 */
    {0x7e, 0x55},           /* GAM4 */
    {0x7f, 0x68},           /* GAM5 */
    {0x80, 0x76},           /* GAM6 */
    {0x81, 0x80},           /* GAM7 */
    {0x82, 0x88},           /* GAM8 */
    {0x83, 0x8f},           /* GAM9 */
    {0x84, 0x96},           /* GAM10 */
    {0x85, 0xa3},           /* GAM11 */
    {0x86, 0xaf},           /* GAM12 */
    {0x87, 0xc4},           /* GAM13 */
    {0x88, 0xd7},           /* GAM14 */
    {0x89, 0xe8},           /* GAM15 */

    /* ── AEC/AGC 阶段1: 关闭自动控制，手动设置基础参数 ── */
    {0x13, 0xe0},           /* COM8: 关闭AEC/AGC/AWB */
    {0x00, 0x00},           /* GAIN: AGC增益清零 */
    {0x10, 0x00},           /* AECH: 曝光高位清零 */
    {0x0d, 0x00},           /* COM4 */
    {0x14, 0x28},           /* COM9: 最大增益x4 */
    {0xa5, 0x05},           /* BD50MAX */
    {0xab, 0x07},           /* BD60MAX */
    {0x24, 0x75},           /* AEW */
    {0x25, 0x63},           /* AEB */
    {0x26, 0xa5},           /* VPT */
    {0x9f, 0x78},           /* HAECC1 */
    {0xa0, 0x68},           /* HAECC2 */
    {0xa1, 0x03},
    {0xa6, 0xdf},
    {0xa7, 0xdf},
    {0xa8, 0xf0},
    {0xa9, 0x90},
    {0xaa, 0x94},

    /* ── AEC/AGC 阶段2: 使能AEC+AGC+AWB ── */
    {0x13, 0xe5},           /* COM8: AEC+AGC+AWB使能 */

    /* ── 系统杂项 ── */
    {0x0e, 0x61},           /* COM5 */
    {0x0f, 0x4b},           /* COM6 */
    {0x16, 0x02},
    {0x1e, 0x07},           /* MVFP: 水平镜像+垂直翻转 */
    {0x21, 0x02},           /* ADCCTR0 */
    {0x22, 0x91},           /* ADCCTR1 */
    {0x29, 0x07},
    {0x33, 0x0b},           /* CHLF */
    {0x35, 0x0b},
    {0x37, 0x1d},           /* ADC */
    {0x38, 0x71},           /* ACOM */
    {0x39, 0x2a},           /* OFON */
    {0x3c, 0x78},           /* COM12 */
    {0x4d, 0x40},
    {0x4e, 0x20},
    {0x69, 0x00},           /* GFIX */
    {0x6b, 0x60},           /* DBLV: PLL x4 */
    {0x74, 0x19},           /* REG74 */
    {0x8d, 0x4f},
    {0x8e, 0x00},
    {0x8f, 0x00},
    {0x90, 0x00},
    {0x91, 0x00},
    {0x92, 0x00},
    {0x96, 0x00},
    {0x9a, 0x80},
    {0xb0, 0x84},
    {0xb1, 0x0c},           /* ABLC1 */
    {0xb2, 0x0e},
    {0xb3, 0x82},           /* THL_ST */
    {0xb8, 0x0a},

    /* ── AWB精调系数 ── */
    {0x43, 0x14},           /* AWBC1 */
    {0x44, 0xf0},           /* AWBC2 */
    {0x45, 0x34},           /* AWBC3 */
    {0x46, 0x58},           /* AWBC4 */
    {0x47, 0x28},           /* AWBC5 */
    {0x48, 0x3a},           /* AWBC6 */
    {0x59, 0x88},
    {0x5a, 0x88},
    {0x5b, 0x44},
    {0x5c, 0x67},
    {0x5d, 0x49},
    {0x5e, 0x0e},
    {0x64, 0x04},
    {0x65, 0x20},
    {0x66, 0x05},
    {0x94, 0x04},
    {0x95, 0x08},
    {0x6c, 0x0a},           /* AWBCTR3 */
    {0x6d, 0x55},           /* AWBCTR2 */
    {0x6e, 0x11},           /* AWBCTR1 */
    {0x6f, 0x9f},           /* AWBCTR0: 快速AWB */
    {0x6a, 0x40},           /* GGAIN */
    {0x01, 0x40},           /* BLUE */
    {0x02, 0x40},           /* RED */

    /* ── AEC/AGC 阶段3: 最终全功能使能 ── */
    {0x13, 0xe7},           /* COM8: 全功能+快速AEC */
    {0x15, 0x00},           /* COM10: 同步极性正常 */

    /* ── 色彩矩阵 (CCM) ── */
    {0x4f, 0x80},           /* MTX1 */
    {0x50, 0x80},           /* MTX2 */
    {0x51, 0x00},           /* MTX3 */
    {0x52, 0x22},           /* MTX4 */
    {0x53, 0x5e},           /* MTX5 */
    {0x54, 0x80},           /* MTX6 */
    {0x58, 0x9e},           /* MTXS */

    /* ── 边缘增强 / 去噪 / 饱和度 / 对比度 ── */
    {0x41, 0x08},           /* COM16: 边缘增强使能 */
    {0x3f, 0x00},           /* EDGE: 自动 */
    {0x75, 0x05},
    {0x76, 0xe1},
    {0x4c, 0x00},           /* DNSTH */
    {0x77, 0x01},
    {0x3d, 0xc2},           /* COM13: Gamma使能, UV自动调整 */
    {0x4b, 0x09},
    {0xc9, 0x60},           /* SATCTR */
    {0x41, 0x38},           /* COM16: 完整DSP使能 */
    {0x56, 0x40},           /* CONTRAS */

    /* ── 同步 / 频闪 ── */
    {0x34, 0x11},           /* ARBLM */
    {0x3b, 0x02},           /* COM11: 50Hz频闪消除 */

    /* ── LENC / 保留矩阵 ── */
    {0xa4, 0x89},           /* NT_CTRL */
    {0x96, 0x00},
    {0x97, 0x30},
    {0x98, 0x20},
    {0x99, 0x30},
    {0x9a, 0x84},
    {0x9b, 0x29},
    {0x9c, 0x03},
    {0x9d, 0x4c},           /* BD50ST */
    {0x9e, 0x3f},           /* BD60ST */
    {0x78, 0x04},

    /* ── DSP间接寄存器写入 (0x79地址/0xC8数据) ── */
    {0x79, 0x01}, {0xc8, 0xf0},
    {0x79, 0x0f}, {0xc8, 0x00},
    {0x79, 0x10}, {0xc8, 0x7e},
    {0x79, 0x0a}, {0xc8, 0x80},
    {0x79, 0x0b}, {0xc8, 0x01},
    {0x79, 0x0c}, {0xc8, 0x0f},
    {0x79, 0x0d}, {0xc8, 0x20},
    {0x79, 0x09}, {0xc8, 0x80},
    {0x79, 0x02}, {0xc8, 0xc0},
    {0x79, 0x03}, {0xc8, 0x40},
    {0x79, 0x05}, {0xc8, 0x30},
    {0x79, 0x26},
    {0x09, 0x00},           /* COM2: 正常驱动电流 */

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
