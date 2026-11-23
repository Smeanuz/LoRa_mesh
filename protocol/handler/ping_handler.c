

#include "ping_handler.h"

#include <string.h>
#include <zephyr/kernel.h>

#include "protocol_config.h"
#include "lora_app.h"
#include "uart_cmd.h"

#define PING_TIMEOUT_MS 3000U

static uint16_t g_ping_seq = 0U;

static struct k_work_delayable g_ping_timeout_work;

static struct {
    bool active;
    uint16_t dst_id;
    uint16_t seq;
    int64_t timestamp_ms;
} g_ping_pending;

K_MUTEX_DEFINE(g_ping_mutex);

static void ping_timeout_work_handler(struct k_work *work);

int ping_handler_init(void)
{
    k_work_init_delayable(&g_ping_timeout_work,
                          ping_timeout_work_handler);

    return 0;
}

void ping_handler_process(const protocol_packet_t *packet)
{
    protocol_packet_t pong_packet = {0};
    int ret;

    if (packet == NULL)
    {
        return;
    }

    /* Build PONG response */
    pong_packet.version = PROTOCOL_VERSION;
    pong_packet.type = PROTOCOL_TYPE_PONG;

    pong_packet.src_id = protocol_get_node_id();
    pong_packet.dst_id = packet->src_id;

    pong_packet.seq = packet->seq;

    pong_packet.payload_len = packet->payload_len;

    pong_packet.ttl = PROTOCOL_DEFAULT_TTL;

    memcpy(pong_packet.payload,
           packet->payload,
           packet->payload_len);

    /* Send PONG through LoRa application */
    ret = lora_app_send_packet(&pong_packet);

    if (ret == 0)
    {
        uart_cmd_SendString("[PING] PONG response queued\r\n");
    }
    else
    {
        uart_cmd_SendString("[PING] PONG response failed: ");
        uart_cmd_SendInt(ret);
        uart_cmd_SendString("\r\n");
    }
}

/*
 * Send PING request
 */
int ping_handler_send(uint16_t dst_id,
                      const uint8_t *payload,
                      uint8_t payload_len)
{
    protocol_packet_t ping_packet = {0};
    int ret;

    if ((dst_id == PROTOCOL_NODE_ID_RESERVED) ||
        (dst_id == PROTOCOL_NODE_ID_BROADCAST))
    {
        return -EINVAL;
    }

    if ((payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) ||
        ((payload_len > 0U) && (payload == NULL)))
    {
        return -EINVAL;
    }

    ret = k_mutex_lock(&g_ping_mutex, K_FOREVER);
    if (ret != 0)
    {
        return ret;
    }

    /* Only support one outstanding PING for now */
    if (g_ping_pending.active)
    {
        k_mutex_unlock(&g_ping_mutex);
        return -EBUSY;
    }

    /* Generate next sequence number */
    g_ping_seq++;

    ping_packet.version = PROTOCOL_VERSION;
    ping_packet.type = PROTOCOL_TYPE_PING;
    ping_packet.src_id = protocol_get_node_id();
    ping_packet.dst_id = dst_id;
    ping_packet.seq = g_ping_seq;
    ping_packet.payload_len = payload_len;
    ping_packet.ttl = PROTOCOL_DEFAULT_TTL;

    if (payload_len > 0U)
    {
        memcpy(ping_packet.payload, payload, payload_len);
    }

    /* Save pending state before queueing TX */
    g_ping_pending.active = true;
    g_ping_pending.dst_id = dst_id;
    g_ping_pending.seq = g_ping_seq;
    g_ping_pending.timestamp_ms = k_uptime_get();

    ret = lora_app_send_packet(&ping_packet);

    if (ret != 0)
    {
        g_ping_pending.active = false;
    } 
    else
    {
        /* Start timeout work */
        k_work_schedule(&g_ping_timeout_work,
                        K_MSEC(PING_TIMEOUT_MS));
    }

    k_mutex_unlock(&g_ping_mutex);

    if (ret == 0)
    {
        uart_cmd_SendString("[PING] Request queued, DST=");
        uart_cmd_SendInt(dst_id);
        uart_cmd_SendString(", SEQ=");
        uart_cmd_SendInt(g_ping_seq);
        uart_cmd_SendString("\r\n");
    }

    return ret;
}

void ping_handler_handle_pong(const protocol_packet_t *packet)
{
    int64_t timestamp_now;
    int64_t rtt_ms;
    uint16_t expected_dst;
    uint16_t expected_seq;
    int ret;

    if (packet == NULL)
    {
        return;
    }

    ret = k_mutex_lock(&g_ping_mutex, K_FOREVER);
    if (ret != 0)
    {
        return;
    }

    /* No outstanding PING */
    if (!g_ping_pending.active)
    {
        k_mutex_unlock(&g_ping_mutex);

        uart_cmd_SendString("[PING] Unexpected PONG\r\n");
        return;
    }

    /* Validate response */
    if ((packet->src_id != g_ping_pending.dst_id) ||
        (packet->dst_id != protocol_get_node_id()) ||
        (packet->seq != g_ping_pending.seq))
    {
        k_mutex_unlock(&g_ping_mutex);

        uart_cmd_SendString("[PING] PONG mismatch\r\n");
        return;
    }

    /* Calculate RTT */
    timestamp_now = k_uptime_get();
    rtt_ms = timestamp_now - g_ping_pending.timestamp_ms;

    expected_dst = g_ping_pending.dst_id;
    expected_seq = g_ping_pending.seq;

    /* Clear pending state */
    g_ping_pending.active = false;
    k_work_cancel_delayable(&g_ping_timeout_work);

    k_mutex_unlock(&g_ping_mutex);

    /* Print result */
    uart_cmd_SendString("\r\n[PING] PONG received\r\n");

    uart_cmd_SendString("SRC=");
    uart_cmd_SendInt(packet->src_id);

    uart_cmd_SendString(" DST=");
    uart_cmd_SendInt(packet->dst_id);

    uart_cmd_SendString(" SEQ=");
    uart_cmd_SendInt(expected_seq);

    uart_cmd_SendString(" TTL=");
    uart_cmd_SendInt(packet->ttl);

    uart_cmd_SendString(" LEN=");
    uart_cmd_SendInt(packet->payload_len);

    uart_cmd_SendString(" RTT=");
    uart_cmd_SendInt((int32_t)rtt_ms);

    uart_cmd_SendString(" ms\r\n");
}

static void ping_timeout_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (k_mutex_lock(&g_ping_mutex, K_FOREVER) != 0)
    {
        return;
    }

    if (g_ping_pending.active)
    {
        g_ping_pending.active = false;

        k_mutex_unlock(&g_ping_mutex);

        uart_cmd_SendString("[PING] Timeout: no PONG received\r\n");
    }
    else
    {
        k_mutex_unlock(&g_ping_mutex);
    }
}
