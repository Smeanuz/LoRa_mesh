

#ifndef _MESH_ROUTER_H_
#define _MESH_ROUTER_H_

#include "protocol_packet.h"

typedef enum
{
    MESH_PACKET_LOCAL = 0,
    MESH_PACKET_FORWARD,
    MESH_PACKET_DROP,
} mesh_packet_result_t;

mesh_packet_result_t mesh_router_process(const protocol_packet_t *packet);

#endif // _MESH_ROUTER_H_