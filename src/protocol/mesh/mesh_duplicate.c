
#include "mesh_duplicate.h"

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include "uart_cmd.h"

struct mesh_duplicate_entry
{
    uint16_t src_id;
    uint8_t type;
    uint16_t seq;
    int64_t timestamp_ms;
    bool valid;
};

static struct mesh_duplicate_entry
    g_duplicate_cache[MESH_DUPLICATE_CACHE_SIZE];

static uint8_t g_write_index;

void mesh_duplicate_init(void)
{
    for (uint8_t i = 0U; i < MESH_DUPLICATE_CACHE_SIZE; i++)
    {
        g_duplicate_cache[i].src_id = 0U;
        g_duplicate_cache[i].type = 0U;
        g_duplicate_cache[i].seq = 0U;
        g_duplicate_cache[i].timestamp_ms = 0;
        g_duplicate_cache[i].valid = false;
    }

    g_write_index = 0U;
}

bool mesh_duplicate_check_and_add(
    const protocol_packet_t *packet)
{
    int64_t now_ms;

    if (packet == NULL)
    {
        return true;
    }

    now_ms = k_uptime_get();

    /* Check whether this packet has already been received */
    for (uint8_t i = 0U; i < MESH_DUPLICATE_CACHE_SIZE; i++)
    {
        if (g_duplicate_cache[i].valid)
        {
            if ((now_ms - g_duplicate_cache[i].timestamp_ms) >=
                MESH_DUPLICATE_TIMEOUT_MS)
            {
                g_duplicate_cache[i].valid = false;
            }
            else if ((g_duplicate_cache[i].src_id == packet->src_id) &&
                    (g_duplicate_cache[i].type == packet->type) &&
                    (g_duplicate_cache[i].seq == packet->seq))
            {
                    uart_cmd_SendString("[MESH] Duplicate packet dropped: SRC=");
                    uart_cmd_SendInt(packet->src_id);
                    uart_cmd_SendString(", TYPE=");
                    uart_cmd_SendInt(packet->type);
                    uart_cmd_SendString(", SEQ=");
                    uart_cmd_SendInt(packet->seq);
                    uart_cmd_SendString("\r\n");
                return true;
            }
        }
    }

    /* Store new packet in the ring buffer */
    g_duplicate_cache[g_write_index].src_id = packet->src_id;
    g_duplicate_cache[g_write_index].type = packet->type;
    g_duplicate_cache[g_write_index].seq = packet->seq;
    g_duplicate_cache[g_write_index].timestamp_ms = now_ms;
    g_duplicate_cache[g_write_index].valid = true;

    g_write_index++;

    if (g_write_index >= MESH_DUPLICATE_CACHE_SIZE)
    {
        g_write_index = 0U;
    }

    return false;
}