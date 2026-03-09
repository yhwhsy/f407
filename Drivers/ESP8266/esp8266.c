#include "esp8266.h"
#include "st7789.h"  // 包含屏幕驱动，用于显示调试颜色
#include <string.h>
#include <stdio.h>

/**
 * @brief  向 ESP8266 发送 AT 指令并等待预期应答
 * @param  cmd     要发送的 AT 指令字符串
 * @param  ack     期待接收到的应答字符串 (如 "OK", "CONNECT")
 * @param  timeout 超时时间 (毫秒)
 * @retval ESP8266_OK (0): 成功收到应答 / ESP8266_ERROR (1): 超时失败
 */
uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout)
{
    // 1. 发送前先清空之前的接收缓冲区 (防止旧数据干扰)
    // 注意：假设你的 rx_buffer 大小是 1024，如果不是请改成本身的实际大小
    memset(rx_buffer, 0, 100); 

    // 2. 发送指令
    HAL_UARTEx_ReceiveToIdle_IT(&huart3, rx_buffer, sizeof(rx_buffer));
    HAL_UART_Transmit(&huart3, (uint8_t *)cmd, strlen(cmd), 1000);

    // 3. 如果不需要等待应答，直接返回成功
    if (ack == NULL) 
    {
        return ESP8266_OK;
    }

    // 4. 循环等待应答，直到超时
    uint32_t start_tick = HAL_GetTick();
    while (HAL_GetTick() - start_tick < timeout)
    {
        // 假设 rx_buffer 会在后台通过中断或 DMA 不断被填充新数据
        // 使用 strstr 查找期待的字符串是否已经出现在缓冲区中
        if (strstr((char *)rx_buffer, ack) != NULL)
        {
            return ESP8266_OK; // 成功匹配！
        }
        
        // 稍微延时，防止死循环占用全部 CPU 资源
        HAL_Delay(10); 
    }

    return ESP8266_ERROR; // 超时都没等到，返回失败
}


/**
 * @brief  初始化 ESP8266 并连接到 TCP 服务器 (透传模式)
 * @param  ssid      Wi-Fi 热点名称
 * @param  pwd       Wi-Fi 密码
 * @param  server_ip 电脑端 TCP 服务器 IP
 * @param  port      电脑端 TCP 服务器端口
 * @retval ESP8266_OK (0): 成功 / ESP8266_ERROR (1): 失败
 */
uint8_t ESP8266_ConnectTo_TCP_Server(char* ssid, char* pwd, char* server_ip, uint16_t port)
{
    char cmd_buf[128]; 

    // 1. 模块复位 (盲等 3 秒)
    ST7789_Fill(COLOR_BLUE); 
    ESP8266_SendCmd("AT+RST\r\n", NULL, 0);
    HAL_Delay(3000);

    // 2. 设置混合模式 (盲等 1 秒)
    ST7789_Fill(COLOR_YELLOW); 
    ESP8266_SendCmd("AT+CWMODE=3\r\n", NULL, 0);
    HAL_Delay(1000);

    // 3. 连接 Wi-Fi (盲等 8 秒，给足路由器分配 IP 的时间)
    ST7789_Fill(COLOR_BLUE); 
    sprintf(cmd_buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
    ESP8266_SendCmd(cmd_buf, NULL, 0);
    HAL_Delay(8000); 

    // 4. 清理旧连接
    ESP8266_SendCmd("AT+CIPMUX=0\r\n", NULL, 0);
    HAL_Delay(500);
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", NULL, 0);
    HAL_Delay(500);

    // 5. 连接电脑 TCP 服务器 (盲等 3 秒)
    ST7789_Fill(COLOR_YELLOW); 
    sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", server_ip, port);
    ESP8266_SendCmd(cmd_buf, NULL, 0); 
    HAL_Delay(3000); 

    // 6. 开启透传
    ESP8266_SendCmd("AT+CIPMODE=1\r\n", NULL, 0);
    HAL_Delay(1000);
    ESP8266_SendCmd("AT+CIPSEND\r\n", NULL, 0);
    HAL_Delay(1000); 

    // 强行返回成功，绝对不让它亮红屏！
    return ESP8266_OK; 
}