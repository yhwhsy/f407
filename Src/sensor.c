#include "sensor.h"
#include "adc.h" // 引入 CubeMX 生成的 hadc1

/**
 * @brief  读取光敏电阻的百分比亮度
 * @return 0% (最暗) ~ 100% (最亮)
 */
uint8_t Sensor_GetLightPercent(void)
{
    // 启动 ADC 采集
    HAL_ADC_Start(&hadc1); 
    // 等待转换完成 (超时 10ms)
    HAL_ADC_PollForConversion(&hadc1, 10); 
    // 读取 12 位 ADC 值 (0 ~ 4095)
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1); 
    
    // 将 0-4095 线性映射为 0-100%
    uint8_t percent = (adc_value * 100) / 4095; 
    
    // 💡 注意：如果你的光敏模块是“越亮电压越低(数值越小)”，请把上面那行改为：
    // percent = 100 - ((adc_value * 100) / 4095);
    
    return percent; 
}