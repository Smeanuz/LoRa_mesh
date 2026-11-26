
#ifndef _UART_CMD_H_
#define _UART_CMD_H_

#include <stdint.h>

int uart_cmd_init(void);
int uart_cmd_ReadChar(uint8_t *data);
void uart_cmd_SendChar(uint8_t data);
void uart_cmd_SendString(const char *str);

void uart_cmd_SendInt(int32_t value);
void uart_cmd_SendUInt(uint32_t value);
void uart_cmd_SendHex(uint32_t value);
void uart_cmd_SendLine(const char *str);


#endif  // _UART_CMD_H_