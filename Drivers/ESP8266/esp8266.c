#include "esp8266.h"
#include "st7789.h"  
#include <string.h>
#include <stdio.h>
#include "ui.h"
uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout)
{
    HAL_UART_AbortReceive_IT(&huart3); // 先强行停止之前的接收
    memset(rx_buffer, 0, 512);         // 清空缓冲区
    rx_len = 0;                        // 长度归零
    esp_ok_flag = 0;                   // 标志归零

    // 从头开始全新接收
    HAL_UARTEx_ReceiveToIdle_IT(&huart3, rx_buffer, 512); 
    HAL_UART_Transmit(&huart3, (uint8_t *)cmd, strlen(cmd), 1000);

    if (ack == NULL) return ESP8266_OK;

    uint32_t start_tick = HAL_GetTick();
    while (HAL_GetTick() - start_tick < timeout)
    {
        // 双重保险：对于 OK 利用中断里的精确 flag 判定，对于其他利用全量缓冲区判定
        if ((strstr(ack, "OK") != NULL && esp_ok_flag == 1) || strstr((char *)rx_buffer, ack) != NULL)
        {
            return ESP8266_OK; 
        }
        HAL_Delay(10); 
    }
    return ESP8266_ERROR; 
}

uint8_t ESP8266_ConnectTo_TCP_Server(char* ssid, char* pwd, char* server_ip, uint16_t port)
{
    char cmd_buf[128]; 
    HAL_UART_Transmit(&huart3, (uint8_t *)"+++", 3, 1000);
    HAL_Delay(1000); // 发完 +++ 之后必须强制等待至少 1 秒，模块才会退回到指令模式
    // 1. 复位与基础设置
    ST7789_Fill(COLOR_BLUE); 
    ESP8266_SendCmd("AT+RST\r\n", "ready", 3000);
    HAL_Delay(1000);
    ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 2000); 

    // 2. 秒连优化：检查是否已经连上了正确的 Wi-Fi
    ST7789_Fill(COLOR_YELLOW); 
    ESP8266_SendCmd("AT+CWJAP?\r\n", "OK", 2000);
    
    // 如果当前没连上，或者连的不是我们要的 WiFi，才执行耗时的连接指令
    if (strstr((char*)rx_buffer, ssid) == NULL) 
    {
        sprintf(cmd_buf, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
        while(ESP8266_SendCmd(cmd_buf, "OK", 15000) != ESP8266_OK) 
        {
            ST7789_Fill(COLOR_RED);  
            // 1. 把 ESP8266 返回的换行符替换为空格，防止字体库无法显示
            for(int i = 0; i < 512; i++) {
                if(rx_buffer[i] == '\r' || rx_buffer[i] == '\n') rx_buffer[i] = ' ';
            }
            
            // 2. 把报错原话直接打在红屏上！
            UI_DrawString(5, 40, "ESP LOG:", COLOR_WHITE, COLOR_RED);
            UI_DrawString(5, 60, (char*)rx_buffer, COLOR_WHITE, COLOR_RED);
            // ================================================

            HAL_Delay(6000); // 停顿 6 秒，给你充足的时间看清屏幕写了什么
            ST7789_Fill(COLOR_YELLOW);
        }
    }
    HAL_Delay(500);

    // 3. 断开旧连接，开启 TCP
    ESP8266_SendCmd("AT+CIPMUX=0\r\n", "OK", 1500);
    ST7789_Fill(COLOR_WHITE); 
    
    sprintf(cmd_buf, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", server_ip, port);
    while(ESP8266_SendCmd(cmd_buf, "OK", 8000) != ESP8266_OK) 
    {
        // 试探是否已经处于透传状态（终结死锁）
        if(ESP8266_SendCmd("AT+CIPMODE=1\r\n", "OK", 1000) == ESP8266_OK) {
            break; 
        }
        ST7789_Fill(COLOR_RED);  
        HAL_Delay(500);
        ST7789_Fill(COLOR_WHITE);
    }

    // 4. 开启透传并准备发送
    ST7789_Fill(COLOR_BLUE);
    while(ESP8266_SendCmd("AT+CIPMODE=1\r\n", "OK", 2000) != ESP8266_OK) {
        HAL_Delay(500);
    }

    while(ESP8266_SendCmd("AT+CIPSEND\r\n", ">", 2000) != ESP8266_OK) {
        HAL_Delay(500);
    }

    ST7789_Fill(COLOR_GREEN);
    HAL_Delay(500);

    return ESP8266_OK; 
}