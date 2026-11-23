#ifndef PROTOCOL_CONFIG_H
#define PROTOCOL_CONFIG_H

#include <stdint.h>

#define PROTOCOL_NODE_ID_MIN       0x0001U
#define PROTOCOL_NODE_ID_MAX       0xFFFEU
#define PROTOCOL_BROADCAST_ID      0xFFFFU

int protocol_set_node_id(uint16_t node_id);

uint16_t protocol_get_node_id(void);

#endif /* PROTOCOL_CONFIG_H */

