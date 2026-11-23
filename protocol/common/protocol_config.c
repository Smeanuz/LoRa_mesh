#include "protocol_config.h"

#include <errno.h>

/* Default Node ID */
static uint16_t g_node_id = 0x0001U;

int protocol_set_node_id(uint16_t node_id)
{
    if ((node_id < PROTOCOL_NODE_ID_MIN) ||
        (node_id > PROTOCOL_NODE_ID_MAX))
    {
        return -EINVAL;
    }

    g_node_id = node_id;

    return 0;
}

uint16_t protocol_get_node_id(void)
{
    return g_node_id;
}