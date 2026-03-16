#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f4xx_hal.h"

#define ESP8266_OK      0
#define ESP8266_ERROR   1

extern UART_HandleTypeDef huart3;
extern uint8_t rx_buffer[512];       // 修改：调大到512字节，防止长句子溢出
extern volatile uint16_t rx_len;     // 新增：记录当前缓冲区的追加长度
extern volatile uint8_t esp_ok_flag; // 新增：供外部调用的强同步 OK 标志

uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout);
uint8_t ESP8266_ConnectTo_TCP_Server(char* ssid, char* pwd, char* server_ip, uint16_t port);

#endif /* __ESP8266_H */