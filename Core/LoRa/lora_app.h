#ifndef LORA_APP_H
#define LORA_APP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "protocol_packet.h"

/* Configuration */
#define LORA_RX_MAX_LEN       255U

/* RX packet structure */
struct lora_rx_packet
{
    uint8_t data[LORA_RX_MAX_LEN];
    uint16_t len;
    int16_t rssi;
    int8_t snr;
};

typedef enum
{
    LORA_TX_POLICY_IMMEDIATE = 0,
    LORA_TX_POLICY_RESPONSE,
    LORA_TX_POLICY_FORWARD,
    LORA_TX_POLICY_DISCOVERY,
} lora_tx_policy_t;

/* Initialize LoRa application */
int lora_app_init(void);

/* Transmit API */
int lora_app_send(const uint8_t *data, size_t len, lora_tx_policy_t policy);
int lora_app_send_string(const char *msg);

int lora_app_send_packet(const protocol_packet_t *packet, lora_tx_policy_t policy);

/* Receive API */
int lora_app_receive(struct lora_rx_packet *packet,
                     int32_t timeout_ms);

/* Asynchronous RX control */
int lora_app_rx_start(void);
int lora_app_rx_stop(void);

/* RX status */
bool lora_app_is_rx_enabled(void);
bool lora_app_is_rx_running(void);
uint32_t lora_app_get_rx_count(void);

#endif /* LORA_APP_H */