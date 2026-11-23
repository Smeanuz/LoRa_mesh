


#ifndef _MESH_DUPLICATE_H_
#define _MESH_DUPLICATE_H_

#include <stdbool.h>
#include "protocol_packet.h"

#define MESH_DUPLICATE_CACHE_SIZE  16U
#define MESH_DUPLICATE_TIMEOUT_MS  10000U

void mesh_duplicate_init(void);

bool mesh_duplicate_check_and_add(
    const protocol_packet_t *packet);

#endif