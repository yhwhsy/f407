#include "mpu6050.h"

#define MPU6050_ADDR 0xD0 // 默认 I2C 地址 (AD0接GND)

// 初始化 MPU6050
uint8_t MPU6050_Init(void)
{
    uint8_t check, data;

    // 1. 检查传感器是否在线 (读取 WHO_AM_I 寄存器 0x75)
    HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, 0x75, 1, &check, 1, 1000);
    if (check != 0x68) return 1; // 找不到传感器，返回错误

    // 2. 唤醒传感器 (解除休眠模式)
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, 0x6B, 1, &data, 1, 1000);

    // 3. 配置加速度计量程为 ±4g (足够检测电动车跌倒或撞击)
    data = 0x08;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, 0x1C, 1, &data, 1, 1000);

    return 0; // 初始化成功
}

// 碰撞与跌倒检测核心逻辑
uint8_t MPU6050_CheckCollision(void)
{
    uint8_t buf[6];
    int16_t accel_x, accel_y, accel_z;
    
    // 一次性连续读取 X, Y, Z 的加速度高低字节 (寄存器 0x3B 开始)
    // 👇 这里也是 &hi2c2
    HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, 0x3B, 1, buf, 6, 1000);
    
    accel_x = (buf[0] << 8) | buf[1];
    accel_y = (buf[2] << 8) | buf[3];
    accel_z = (buf[4] << 8) | buf[5];

    // 将原始数据(±4g量程下，8192代表1g)转换为大致的 g 值
    float ax = (float)accel_x / 8192.0f;
    float ay = (float)accel_y / 8192.0f;
    float az = (float)accel_z / 8192.0f;

    // 💡 碰撞判定算法：
    // 正常骑行时，三轴合力应该在 1.0g 左右（重力）。
    // 如果任意一个轴的加速度绝对值超过了 2.0g，说明发生了剧烈的急刹车、撞击或重摔！
    if (ax > 2.0f || ax < -2.0f || 
        ay > 2.0f || ay < -2.0f || 
        az > 2.0f || az < -2.0f) 
    {
        return 1; // 发生碰撞！
    }
    
    return 0; // 平安无事
}