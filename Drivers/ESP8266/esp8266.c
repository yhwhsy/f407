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
    char cmd_buf[128]; // 用于存放动态拼接的 AT 指令

    // 1. 模块复位
    ST7789_Fill(COLOR_BLUE); // 蓝屏：正在复位
    ESP8266_SendCmd("AT+RST\r\n", "ready", 3000);
    HAL_Delay(1000);

    // 2. 设置为 STA+AP 混合模式 (最稳定的模式)
    ST7789_Fill(COLOR_YELLOW); 
    if(ESP8266_SendCmd("AT+CWMODE=3\r\n", "OK", 2000) != ESP8266_OK) return ESP8266_ERROR; 
    HAL_Delay(500);

    // 3. 连接 Wi-Fi 热点 (动态拼接字符串)
    ST7789_Fill(COLOR_BLUE); // 蓝屏：正在连 Wi-Fi
    if(ESP8266_SendCmd("AT+CWJAP=\"yhwhsy\",\"13616338678\"\r\n", "WIFI GOT IP", 20000) != ESP8266_OK) return ESP8266_ERROR;
    HAL_Delay(2000); // 必须等待路由器彻底分配好 IP

    // 4. 清理旧连接并设置为单连接模式
    ESP8266_SendCmd("AT+CIPMUX=0\r\n", "OK", 2000);
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
    HAL_Delay(500);

    // 5. 连接电脑 TCP 服务器 (动态拼接 IP 和端口)
    ST7789_Fill(COLOR_YELLOW); // 黄屏：正在连 TCP Server
    sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", server_ip, port);
    // 注意：1.5.4.1 固件连接成功返回的是 "CONNECT"
    ESP8266_SendCmd(cmd_buf, NULL, 0); 
    HAL_Delay(2000); // 强行等它连上

    // 6. 开启透传模式
    ESP8266_SendCmd("AT+CIPMODE=1\r\n", "OK", 2000);
    ESP8266_SendCmd("AT+CIPSEND\r\n", ">", 2000);
    HAL_Delay(500); // 极其关键的延时，等待模块彻底进入透传状态

    return ESP8266_OK; // 全部通关！
}