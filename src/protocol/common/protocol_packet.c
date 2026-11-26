
#include "protocol_packet.h"

#include <errno.h>
#include <string.h>

int protocol_packet_encode(const protocol_packet_t *packet,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *encoded_len)
{
    size_t total_len;

    if ((packet == NULL) || (buffer == NULL) || (encoded_len == NULL))
    {
        return -EINVAL;
    }

    if ((packet->version != PROTOCOL_VERSION) || 
        (packet->payload_len > PROTOCOL_MAX_PAYLOAD_SIZE))
    {
        return -EINVAL;
    }

    total_len = PROTOCOL_HEADER_SIZE + packet->payload_len;

    if (buffer_size < total_len)
    {
        return -ENOSPC;
    }

    /* Header */
    buffer[0] = packet->version;
    buffer[1] = packet->type;

    /* Big-endian payload */
    buffer[2] = (uint8_t)(packet->src_id >> 8);
    buffer[3] = (uint8_t)(packet->src_id & 0xFFU);

    buffer[4] = (uint8_t)(packet->dst_id >> 8);
    buffer[5] = (uint8_t)(packet->dst_id & 0xFFU);

    buffer[6] = (uint8_t)(packet->seq >> 8);
    buffer[7] = (uint8_t)(packet->seq & 0xFFU);

    buffer[8] = packet->ttl;

    /* Payload */
    if (packet->payload_len > 0U)
    {
        memcpy(&buffer[PROTOCOL_HEADER_SIZE],
               packet->payload,
               packet->payload_len);
    }

    *encoded_len = total_len;

    return 0;
}

int protocol_packet_decode(const uint8_t *buffer,
                           size_t buffer_len,
                           protocol_packet_t *packet)
{
    size_t payload_len;

    if ((buffer == NULL) || (packet == NULL))
    {
        return -EINVAL;
    }

    if (buffer_len < PROTOCOL_HEADER_SIZE)
    {
        return -EINVAL;
    }

    if (buffer[0] != PROTOCOL_VERSION)
    {
        return -EPROTONOSUPPORT;
    }

    payload_len = buffer_len - PROTOCOL_HEADER_SIZE;

    if (payload_len > PROTOCOL_MAX_PAYLOAD_SIZE)
    {
        return -EMSGSIZE;
    }

    memset(packet, 0, sizeof(*packet));

    packet->version = buffer[0];
    packet->type = buffer[1];

    /* Big-edian decoding */
    packet->src_id = ((uint16_t)buffer[2] << 8) | (uint16_t)buffer[3];
    packet->dst_id = ((uint16_t)buffer[4] << 8) | (uint16_t)buffer[5];
    packet->seq = ((uint16_t)buffer[6] << 8) | (uint16_t)buffer[7];
    packet->ttl = buffer[8];

    packet->payload_len = (uint8_t)payload_len;

    if (payload_len > 0U)
    {
        memcpy(packet->payload,
               &buffer[PROTOCOL_HEADER_SIZE],
               payload_len);
    }

    return 0;
}
