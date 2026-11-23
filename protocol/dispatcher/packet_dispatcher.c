#include "packet_dispatcher.h"

#include "protocol_config.h"
#include "ping_handler.h"
#include "uart_cmd.h"

#include <zephyr/kernel.h>
#include <errno.h>

void packet_dispatcher_process(const protocol_packet_t *packet)
{
    uint16_t local_node_id;

    if (packet == NULL)
    {
        return;
    }

    local_node_id = protocol_get_node_id();

    /*
     * Check destination.
     */
    if ((packet->dst_id != local_node_id) &&
        (packet->dst_id != PROTOCOL_NODE_ID_BROADCAST))
    {
        uart_cmd_SendString("[DISPATCH] Packet not for this node\r\n");
        return;
    }

    /*
     * Dispatch by packet type.
     */
    switch (packet->type)
    {
        case PROTOCOL_TYPE_PING:

            uart_cmd_SendString("[DISPATCH] PING packet received\r\n");
            ping_handler_process(packet);
            break;

        case PROTOCOL_TYPE_PONG:

            uart_cmd_SendString("[DISPATCH] PONG packet received\r\n");
            ping_handler_handle_pong(packet);
            break;

        default:

            uart_cmd_SendString("[DISPATCH] Unknown packet type\r\n");
            break;
    }
}

int packet_dispatcher_init(void)
{
    uart_cmd_SendString("[INFO] Packet dispatcher initialized\r\n");

    return 0;
}