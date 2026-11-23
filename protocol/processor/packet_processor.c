#include "packet_processor.h"

#include <errno.h>

#include <zephyr/kernel.h>

#include "uart_cmd.h"
#include "lora_app.h"
#include "protocol_packet.h"
#include "packet_dispatcher.h"
#include "ping_handler.h"
#include "mesh_router.h"
#include "mesh_duplicate.h"

#define PACKET_PROCESSOR_STACK_SIZE       2048
#define PACKET_PROCESSOR_PRIORITY         5

#define PACKET_PROCESSOR_RX_TIMEOUT_MS    10U

static struct k_thread packet_processor_thread_data;
static K_THREAD_STACK_DEFINE(packet_processor_stack,
                             PACKET_PROCESSOR_STACK_SIZE);

static void packet_processor_thread(void *arg1,
                                   void *arg2,
                                   void *arg3);

static void packet_processor_thread(void *arg1,
                                   void *arg2,
                                   void *arg3)
{
    struct lora_rx_packet packet;
    protocol_packet_t decoded_packet;
    mesh_packet_result_t mesh_result;
    int ret;

    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    uart_cmd_SendString("[INFO] Packet processor thread started\r\n");

    while (1)
    {
        ret = lora_app_receive(&packet,
                               PACKET_PROCESSOR_RX_TIMEOUT_MS);

        if (ret == 0)
        {
            ret = protocol_packet_decode(packet.data, packet.len, &decoded_packet);
            if (ret == 0)
            {
                uart_cmd_SendString("[PACKET] Decode OK\r\n");

                uart_cmd_SendString("Version: ");
                uart_cmd_SendInt(decoded_packet.version);

                uart_cmd_SendString(", Type: ");
                uart_cmd_SendInt(decoded_packet.type);

                uart_cmd_SendString(", SRC: ");
                uart_cmd_SendInt(decoded_packet.src_id);

                uart_cmd_SendString(", DST: ");
                uart_cmd_SendInt(decoded_packet.dst_id);

                uart_cmd_SendString(", SEQ: ");
                uart_cmd_SendInt(decoded_packet.seq);

                uart_cmd_SendString(", TTL: ");
                uart_cmd_SendInt(decoded_packet.ttl);

                uart_cmd_SendString(", Payload len: ");
                uart_cmd_SendInt(decoded_packet.payload_len);

                uart_cmd_SendString("\r\n");

                mesh_result = mesh_router_process(&decoded_packet);

                switch (mesh_result)
                {
                    case MESH_PACKET_LOCAL:
                        packet_dispatcher_process(&decoded_packet);
                        break;

                    case MESH_PACKET_FORWARD:
                        uart_cmd_SendString("[MESH] Packet forwarded\r\n");
                        break;

                    case MESH_PACKET_DROP:
                    default:
                        uart_cmd_SendString("[MESH] Packet dropped\r\n");
                        break;
                }
            }
            else
            {
                uart_cmd_SendString("[ERROR] Packet decode failed: ");
                uart_cmd_SendInt(ret);
                uart_cmd_SendString("\r\n");
            }
        }
        else if ((ret != -EAGAIN) && (ret != -ENOMSG))
        {
            uart_cmd_SendString("[ERROR] LoRa RX failed: ");
            uart_cmd_SendInt(ret);
            uart_cmd_SendString("\r\n");
        }
    }
}

int packet_processor_init(void)
{
    k_thread_create(&packet_processor_thread_data,
                               packet_processor_stack,
                               K_THREAD_STACK_SIZEOF(packet_processor_stack),
                               packet_processor_thread,
                               NULL,
                               NULL,
                               NULL,
                               PACKET_PROCESSOR_PRIORITY,
                               0,
                               K_NO_WAIT);

    uart_cmd_SendString("[INFO] Packet processor initialized\r\n");

    ping_handler_init();
    mesh_duplicate_init();

    return 0;
}