
#ifndef _PROTOCOL_PACKET_H_
#define _PROTOCOL_PACKET_H_

#include <stdint.h>
#include <stddef.h>

#define PROTOCOL_VERSION            0x01U

#define PROTOCOL_HEADER_SIZE        9U
#define PROTOCOL_MAX_PACKET_SIZE    255U
#define PROTOCOL_MAX_PAYLOAD_SIZE   (PROTOCOL_MAX_PACKET_SIZE - PROTOCOL_HEADER_SIZE)

// Node ID definitions
#define PROTOCOL_NODE_ID_RESERVED  0x0000U
#define PROTOCOL_NODE_ID_BROADCAST 0xFFFFU

// TTL definitions
#define PROTOCOL_DEFAULT_TTL      3U
#define PROTOCOL_MAX_TTL          15U

typedef enum
{
    PROTOCOL_TYPE_PING  = 0x01U,
    PROTOCOL_TYPE_PONG  = 0x02U,

} protocol_packet_type_t;

typedef struct
{
    uint8_t version;
    uint8_t type;

    uint16_t src_id;
    uint16_t dst_id;
    uint16_t seq;

    uint8_t ttl;

    uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE];
    uint8_t payload_len;
    
} protocol_packet_t;

/* Encode logical packet into byte stream */
int protocol_packet_encode(const protocol_packet_t *packet,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *encoded_len);

/* Decode byte stream into logical packet */
int protocol_packet_decode(const uint8_t *buffer,
                           size_t buffer_len,
                           protocol_packet_t *packet);

#endif // _PROTOCOL_PACKET_H_