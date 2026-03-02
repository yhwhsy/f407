/**
 ******************************************************************************
 * @file    ov7670.c
 * @brief   OV7670 摄像头驱动实现
 *          使用I2C1（SCCB协议）配置寄存器
 *          输出格式：RGB565，分辨率：QVGA (320x240)
 ******************************************************************************
 */

#include "ov7670.h"

static I2C_HandleTypeDef *g_hi2c = NULL;

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
    uint8_t buf[2] = {reg, val};
    HAL_StatusTypeDef ret;
    ret = HAL_I2C_Master_Transmit(g_hi2c, OV7670_SCCB_ADDR, buf, 2, 100);
    HAL_Delay(1);  /* SCCB需要稳定时间 */
    return (ret == HAL_OK) ? 0 : 1;
}

/**
 * @brief 读OV7670寄存器
 * @param reg  寄存器地址
 * @param val  读取值指针
 * @return 0=成功, 1=失败
 */
uint8_t OV7670_ReadReg(uint8_t reg, uint8_t *val)
{
    HAL_StatusTypeDef ret;
    /* SCCB读取：先发寄存器地址，再读数据 */
    ret = HAL_I2C_Master_Transmit(g_hi2c, OV7670_SCCB_ADDR, &reg, 1, 100);
    if (ret != HAL_OK) return 1;
    HAL_Delay(1);
    ret = HAL_I2C_Master_Receive(g_hi2c, OV7670_SCCB_ADDR | 0x01, val, 1, 100);
    return (ret == HAL_OK) ? 0 : 1;
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
    /* 软件复位 */
    {REG_COM7,    COM7_RESET},

    /* 时钟：XCLK=24MHz，内部PLL，PCLK=12MHz */
    {REG_CLKRC,   0x01},   /* 预分频/1 */
    {REG_DBLV,    0x0A},   /* PLL旁路，PCLK=XCLK/2 */

    /* 输出格式：RGB565，QVGA */
    {REG_COM7,    COM7_FMT_QVGA | COM7_RGB},
    {REG_COM15,   COM15_RGB565 | COM15_R00FF},
    {REG_RGB444,  0x00},   /* 关闭RGB444 */

    /* QVGA水平/垂直帧参数 */
    {REG_HSTART,  0x16},
    {REG_HSTOP,   0x04},
    {REG_HREF,    0x24},
    {REG_VSTART,  0x02},
    {REG_VSTOP,   0x7A},
    {REG_VREF,    0x0A},

    /* 缩放控制（QVGA = VGA/2）*/
    {REG_COM3,    0x04},   /* 使能缩放 */
    {REG_COM14,   0x19},   /* 手动缩放，PCLK分频 */
    {REG_SCALING_XSC,      0x3A},
    {REG_SCALING_YSC,      0x35},
    {REG_SCALING_DCWCTR,   0x11},   /* 水平/垂直各降采样2倍 */
    {REG_SCALING_PC,       0xF1},   /* PCLK分频 */
    {REG_SCALING_PCLK_DIV, 0x02},

    /* AEC/AGC控制 */
    {REG_COM8,    0xE5},   /* 使能AGC, AEC, AWB */
    {REG_GAIN,    0x00},
    {REG_AECH,    0x00},
    {REG_COM9,    0x18},   /* 最大AGC增益4x */
    {REG_AEW,     0x75},   /* AEC上界 */
    {REG_AEB,     0x63},   /* AEC下界 */
    {REG_VPT,     0xD4},   /* 快速AEC操作区 */

    /* AWB控制 */
    {REG_AWBCTR0, 0xAA},
    {REG_AWBCTR1, 0x11},
    {REG_AWBCTR2, 0x01},
    {REG_AWBCTR3, 0x14},

    /* 颜色矩阵（RGB565 from OV7670 官方推荐）*/
    {0x4F,        0x80},
    {0x50,        0x80},
    {0x51,        0x00},
    {0x52,        0x22},
    {0x53,        0x5E},
    {0x54,        0x80},
    {0x58,        0x9E},

    /* Gamma */
    {0x7A,        0x20},
    {0x7B,        0x10},
    {0x7C,        0x1E},
    {0x7D,        0x35},
    {0x7E,        0x5A},
    {0x7F,        0x69},
    {0x80,        0x76},
    {0x81,        0x80},
    {0x82,        0x88},
    {0x83,        0x8F},
    {0x84,        0x96},
    {0x85,        0xA3},
    {0x86,        0xAF},
    {0x87,        0xC4},
    {0x88,        0xD7},
    {0x89,        0xE8},

    /* 饱和度/色调 */
    {0x4E,        0x01},   /* RGB通道矩阵系数符号 */
    {REG_GGAIN,   0x5A},   /* G通道AWB增益 */

    /* 降噪 */
    {REG_COM16,   0x38},   /* AWB增益使能，降噪使能 */
    {REG_EDGE,    0x06},

    /* HSYNC / VSYNC 极性 */
    {REG_COM10,   0x00},   /* VSYNC低有效，HREF高有效，PCLK上升沿有效 */
    {REG_TSLB,    0x04},   /* UYVY格式字节顺序（RGB时不影响）*/

    /* 镜像/翻转（根据实际安装方向调整）*/
    {REG_MVFP,    0x00},   /* 0x00=正常，0x20=水平镜像，0x10=垂直翻转 */

    /* ADC配置 */
    {REG_ADC,     0x02},
    {REG_ACOM,    0x3C},
    {REG_OFON,    0x38},

    /* 参考电压 */
    {REG_ADCCTR1, 0x02},
    {REG_ADCCTR2, 0x91},
    {REG_ADCCTR3, 0x08},

    /* COM11：60Hz灯光频率消除 */
    {REG_COM11,   0x00},

    /* 结束标记 */
    {0xFF,        0xFF}
};

/* ============================================================
 *  公共API
 * ============================================================ */

/**
 * @brief OV7670初始化
 * @param hi2c  I2C句柄（I2C1）
 * @return 0=成功, 1=ID读取失败, 2=寄存器写入失败
 */
uint8_t OV7670_Init(I2C_HandleTypeDef *hi2c)
{
    g_hi2c = hi2c;

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
