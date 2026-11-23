
#ifndef _LORA_RADIO_H_
#define _LORA_RADIO_H_

#include <stdint.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/lora.h>



int lora_radio_init(void);
int lora_radio_send(const uint8_t *data, size_t len);
int lora_radio_receive(uint8_t *data, size_t max_len, int16_t *rssi, int8_t *snr, k_timeout_t timeout);

int lora_radio_receive_async(lora_recv_cb callback, void *user_data);
int lora_radio_stop_async(void);

#endif // _LORA_RADIO_H_