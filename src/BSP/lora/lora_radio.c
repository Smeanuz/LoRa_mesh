
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/lora.h>

#include "lora_radio.h"
#include "uart_cmd.h"


#define RADIO_NODE DT_ALIAS(lora0)

static const struct device *lora_dev = DEVICE_DT_GET(RADIO_NODE);
static struct lora_modem_config config = 
{
    .frequency =  920500000,        // 865100000
    .bandwidth =  BW_125_KHZ,
    .datarate =   SF_8,
    .coding_rate = CR_4_5,
    .preamble_len = 8,
    .tx_power =   4,
    .tx = false,
    .public_network = false,
    .iq_inverted = false,
};

static bool radio_is_tx = false;
static bool radio_configured = false;

static int lora_radio_set_mode(bool tx_mode);

int lora_radio_init(void)
{
    if (!device_is_ready(lora_dev)) {
        return -ENODEV;
    }

    // radio_configured = false;

    return lora_radio_set_mode(false);
}

int lora_radio_send(const uint8_t *data, size_t len)
{
    int ret;

    if ((NULL == data) || (0U == len)) 
    {
        return -EINVAL;
    }

    ret = lora_radio_set_mode(true);
    if (ret < 0) 
    {
        return ret;
    }

    return lora_send(lora_dev, (uint8_t *)data, (uint32_t)len);
}

int lora_radio_receive(uint8_t *data, size_t max_len, int16_t *rssi, int8_t *snr, k_timeout_t timeout)
{
    int ret;

    if ((data == NULL) ||
        (max_len == 0U) ||
        (rssi == NULL) ||
        (snr == NULL))
    {
        return -EINVAL;
    }

    if (max_len > UINT8_MAX)
    {
        max_len = UINT8_MAX;
    }

    ret = lora_radio_set_mode(false);

    if (ret < 0)
    {
        return ret;
    }

    return lora_recv(lora_dev,
                     data,
                     (uint8_t)max_len,
                     timeout,
                     rssi,
                     snr);
}

int lora_radio_receive_async(lora_recv_cb callback, void *user_data)
{
    int ret;

    if (NULL == callback)
    {
        return -EINVAL;
    }

    ret = lora_radio_set_mode(false);
    if (ret < 0)
    {
        return ret;
    }

    return lora_recv_async(lora_dev, callback, user_data);
}

int lora_radio_stop_async(void)
{
    return lora_recv_async(lora_dev, NULL, NULL);
}

static int lora_radio_set_mode(bool tx_mode)
{
    int ret;

    if (radio_configured && (radio_is_tx == tx_mode)) 
    {
        return 0;
    }

    config.tx = tx_mode;
    ret = lora_config(lora_dev, &config);
    if (ret < 0) 
    {
        uart_cmd_SendString("[LoRa] Config error: ");
        uart_cmd_SendInt(ret);
        uart_cmd_SendString("\r\n");
        return ret;
    }

    radio_is_tx = tx_mode;
    radio_configured = true;

    uart_cmd_SendString(tx_mode ?
                    "[LoRa] Mode: TX\r\n" :
                    "[LoRa] Mode: RX\r\n");

    return 0;
}
