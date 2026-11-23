/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>


#include "led_sts.h"
#include "uart_cmd.h"
#include "cmd.h"
#include "lora_app.h"
#include "protocol_packet.h"
#include "packet_processor.h"


#define SLEEP_TIME_MS   10



int main(void)
{
	int ret;

    /* Initialize UART */
    ret = uart_cmd_init();
    if (ret < 0)
    {
        return ret;
    }

    /* Initialize CLI */
    cmdline_init();

    /* Initialize LED */
    ret = led_sts_init();
    if (ret < 0)
    {
        uart_cmd_SendLine("[ERROR] LED init failed");
    }

    /* Initialize LoRa */
    ret = lora_app_init();
    if (ret < 0)
    {
        uart_cmd_SendString("[ERROR] LoRa init failed: ");
        uart_cmd_SendInt(ret);
        uart_cmd_SendString("\r\n");
    }

    packet_processor_init();

    uart_cmd_SendLine("System initialized");


    while (1)
    {
        task_cmdline_update();

        k_msleep(SLEEP_TIME_MS);
    }
	return 0;
}
