#include "mesh_router.h"

#include <stdint.h>

#include "protocol_config.h"
#include "lora_app.h"
#include "mesh_duplicate.h"

mesh_packet_result_t mesh_router_process(const protocol_packet_t *packet)
{
    protocol_packet_t forward_packet;
    int ret;

    if (packet == NULL)
    {
        return MESH_PACKET_DROP;
    }

    if (mesh_duplicate_check_and_add(packet))
    {
        return MESH_PACKET_DROP;
    }

    if (packet->src_id == protocol_get_node_id())
    {
        return MESH_PACKET_DROP;
    }

    /* Packet dành cho node hiện tại */
    if (packet->dst_id == protocol_get_node_id())
    {
        return MESH_PACKET_LOCAL;
    }

    if (packet->dst_id == PROTOCOL_NODE_ID_BROADCAST)
    {
        return MESH_PACKET_LOCAL;
    }

    /* If TTL is less than or equal to 1, drop the packet */
    if (packet->ttl <= 1U)
    {
        return MESH_PACKET_DROP;
    }

    forward_packet = *packet;

    /* Decrease TTL before forwarding */
    forward_packet.ttl--;

    ret = lora_app_send_packet(&forward_packet, LORA_TX_POLICY_IMMEDIATE);

    if (ret != 0)
    {
        return MESH_PACKET_DROP;
    }

    return MESH_PACKET_FORWARD;
}