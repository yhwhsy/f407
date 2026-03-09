#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f4xx_hal.h"

/* * 宏定义区 
 */
#define ESP8266_OK      0
#define ESP8266_ERROR   1

/* * 外部变量声明 (引用 main.c 或 usart.c 中的变量)
 * 注意：根据你实际的串口号和接收数组名进行修改！
 */
extern UART_HandleTypeDef huart3;       // 与 ESP8266 通信的串口
extern uint8_t rx_buffer[100];             // 串口接收缓冲区 (你之前调试时用到的那个)

/* * 函数声明 
 */
uint8_t ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout);
uint8_t ESP8266_ConnectTo_TCP_Server(char* ssid, char* pwd, char* server_ip, uint16_t port);

#endif /* __ESP8266_H */