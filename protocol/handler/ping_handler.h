

#ifndef PING_HANDLER_H
#define PING_HANDLER_H

#include "protocol_packet.h"

int ping_handler_init(void);

void ping_handler_process(const protocol_packet_t *packet);

int ping_handler_send(uint16_t dst_id, const uint8_t *payload, uint8_t payload_len);

void ping_handler_handle_pong(const protocol_packet_t *packet);

#endif /* PING_HANDLER_H */