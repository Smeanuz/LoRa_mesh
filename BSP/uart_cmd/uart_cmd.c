

// #include <cstdio>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "uart_cmd.h"
#include "zephyr/sys/printk.h"



#define UART_CMD                    DT_NODELABEL(usart1)

#define UART_CMD_BUFFER_SIZE        128U

static const struct device *uart_cmd_dev = DEVICE_DT_GET(UART_CMD);

K_MSGQ_DEFINE(uart_cmd_queue, sizeof(uint8_t), UART_CMD_BUFFER_SIZE, 1);

static void uart_cmd_isr(const struct device *dev, void *user_data) 
{
    uint8_t data;
    ARG_UNUSED(user_data);

    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) 
    {
        while (uart_fifo_read(dev, &data, 1) > 0) 
        {
            k_msgq_put(&uart_cmd_queue, &data, K_NO_WAIT);
        }
    }
}

int uart_cmd_init(void)
{
    if (!device_is_ready(uart_cmd_dev)) 
    {
        return -ENODEV;
    }

    int ret = uart_irq_callback_user_data_set(uart_cmd_dev, uart_cmd_isr, NULL);
    if (ret != 0) 
    {
        return ret;
    }

    uart_irq_rx_enable(uart_cmd_dev);

    return 0;
}

int uart_cmd_ReadChar(uint8_t *data) 
{
    if (NULL == data)
    {
        return -EINVAL;
    }

    return k_msgq_get(&uart_cmd_queue, data, K_FOREVER);
}

void uart_cmd_SendChar(uint8_t data) 
{
    uart_poll_out(uart_cmd_dev, data);
}

void uart_cmd_SendString(const char *str) 
{
    if (NULL == str) 
    {
        return;
    }

    while (*str != '\0') 
    {
        uart_poll_out(uart_cmd_dev, (uint8_t)*str++);
    }
}

void uart_cmd_SendInt(int32_t value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%" PRId32, value);
    uart_cmd_SendString(buffer);
}

void uart_cmd_SendUInt(uint32_t value)
{
    char buffer[16];

    snprintf(buffer, sizeof(buffer), "%" PRIu32, value);
    uart_cmd_SendString(buffer);
}

void uart_cmd_SendHex(uint32_t value)
{
    char buffer[16];

    snprintf(buffer, sizeof(buffer), "0x%08" PRIX32, value);
    uart_cmd_SendString(buffer);
}

void uart_cmd_SendLine(const char *str)
{
    uart_cmd_SendString(str);
    uart_cmd_SendString("\r\n");
}
