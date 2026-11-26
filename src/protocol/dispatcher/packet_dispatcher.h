


#ifndef _PACKET_DISPATCHER_H_
#define _PACKET_DISPATCHER_H_

#include "protocol_packet.h"

int packet_dispatcher_init(void);

void packet_dispatcher_process(const protocol_packet_t *packet);

#endif //  _PACKET_DISPATCHER_H_
