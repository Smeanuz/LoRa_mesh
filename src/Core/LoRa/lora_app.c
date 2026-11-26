#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "lora_app.h"
#include "lora_radio.h"
#include "uart_cmd.h"
#include "protocol_config.h"


/*===========================================================================
 * CONFIGURATION
 *===========================================================================*/

#define LORA_TX_MAX_LEN          255U
#define LORA_TX_QUEUE_SIZE       8U

#define LORA_RX_QUEUE_SIZE       8U

#define LORA_OWNER_STACK_SIZE    3072U
#define LORA_OWNER_PRIORITY      5

#define LORA_INIT_TIMEOUT_MS     5000



/*===========================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/* TX packet */
struct lora_tx_packet
{
    uint8_t data[LORA_TX_MAX_LEN];
    uint16_t len;

    lora_tx_policy_t policy;
    int64_t send_at;
};


/*===========================================================================
 * KERNEL OBJECTS
 *===========================================================================*/

/* TX message queue */
K_MSGQ_DEFINE(lora_tx_msgq,
              sizeof(struct lora_tx_packet),
              LORA_TX_QUEUE_SIZE,
              4);


/* RX message queue */
K_MSGQ_DEFINE(lora_rx_msgq,
              sizeof(struct lora_rx_packet),
              LORA_RX_QUEUE_SIZE,
              4);


/* Owner thread */
K_THREAD_STACK_DEFINE(lora_owner_stack, LORA_OWNER_STACK_SIZE);

static struct k_thread lora_owner_thread_data;

/* Semaphores */
static struct k_sem lora_owner_wake_sem;
static struct k_sem lora_init_sem;


/*===========================================================================
 * GLOBAL VARIABLES
 *===========================================================================*/

static atomic_t lora_rx_enable = ATOMIC_INIT(0);
static atomic_t lora_rx_running = ATOMIC_INIT(0);
static atomic_t lora_rx_count = ATOMIC_INIT(0);

static bool lora_initialized = false;

static int lora_init_result = -EINPROGRESS;


/*===========================================================================
 * UART LOGGING
 *===========================================================================*/

static void lora_log_init(int result)
{
    if (result == 0)
    {
        uart_cmd_SendString("[LoRa] Initialization successful\r\n");
    }
    else
    {
        uart_cmd_SendString("[LoRa] Initialization failed: ");
        uart_cmd_SendInt(result);
        uart_cmd_SendString("\r\n");
    }
}


static void lora_log_tx(int result, uint16_t len)
{
    if (result == 0)
    {
        uart_cmd_SendString("[LoRa TX] Success | Size: ");
        uart_cmd_SendInt(len);
        uart_cmd_SendString(" bytes\r\n");
    }
    else
    {
        uart_cmd_SendString("[LoRa TX] Failed | Error: ");
        uart_cmd_SendInt(result);
        uart_cmd_SendString("\r\n");
    }
}

/*===========================================================================
 * STATIC FUNCTION DECLARATIONS
 *===========================================================================*/

static int lora_tx_find_earliest(const struct lora_tx_packet *packets, size_t count);
static uint32_t lora_tx_get_delay_ms(lora_tx_policy_t policy);


/*===========================================================================
 * RX CALLBACK
 *===========================================================================*/

/*
 * This callback is called by the LoRa driver when a packet is received.
 *
 * Do not perform UART logging here.
 * Only copy packet metadata into the queue and wake up Owner Thread.
 */
static void lora_rx_callback(const struct device *dev,
                             uint8_t *data,
                             uint16_t size,
                             int16_t rssi,
                             int8_t snr,
                             void *user_data)
{
    struct lora_rx_packet packet;

    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    if ((data == NULL) ||
        (size == 0U) ||
        (size > LORA_RX_MAX_LEN))
    {
        return;
    }

    /* Copy payload */
    memcpy(packet.data, data, size);

    /* Copy metadata */
    packet.len = size;
    packet.rssi = rssi;
    packet.snr = snr;

    /* Update RX counter */
    atomic_inc(&lora_rx_count);

    /* Push packet into RX queue */
    if (k_msgq_put(&lora_rx_msgq,
                   &packet,
                   K_NO_WAIT) < 0)
    {
        /* RX queue full: packet is dropped */
        return;
    }
}


/*===========================================================================
 * OWNER THREAD
 *===========================================================================*/


static void lora_owner_thread(void *arg1, void *arg2, void *arg3)
{
    struct lora_tx_packet tx_packet;

    struct lora_tx_packet pending_packets[LORA_TX_QUEUE_SIZE];
    size_t pending_count = 0U;

    int ret;

    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    /*-----------------------------------------------------------------------
     * Initialize radio
     *-----------------------------------------------------------------------*/

    ret = lora_radio_init();

    lora_init_result = ret;

    lora_log_init(ret);

    k_sem_give(&lora_init_sem);

    if (ret < 0)
    {
        return;
    }

    /*-----------------------------------------------------------------------
     * Main Owner Thread loop
     *-----------------------------------------------------------------------*/

    while (1)
    {
        int earliest_index;
        int64_t now;
        int64_t wait_ms;
        bool tx_due = false;

        /*-------------------------------------------------------------------
         * 1. Collect TX requests from message queue
         *-------------------------------------------------------------------*/

        while (pending_count < LORA_TX_QUEUE_SIZE)
        {
            ret = k_msgq_get(&lora_tx_msgq,
                             &tx_packet,
                             K_NO_WAIT);

            if (ret != 0)
            {
                break;
            }

            pending_packets[pending_count] = tx_packet;
            pending_count++;
        }

        /*-------------------------------------------------------------------
         * 2. Select earliest TX packet
         *-------------------------------------------------------------------*/

        if (pending_count > 0U)
        {
            earliest_index = lora_tx_find_earliest(
                pending_packets,
                pending_count);

            if (earliest_index >= 0)
            {
                now = k_uptime_get();

                tx_packet = pending_packets[earliest_index];

                wait_ms = tx_packet.send_at - now;

                if (wait_ms <= 0)
                {
                    tx_due = true;
                }
            }
        }

        /*-------------------------------------------------------------------
         * 3. Transmit packet when it is ready
         *-------------------------------------------------------------------*/

        if (tx_due)
        {
            /*
             * Stop asynchronous RX before transmitting.
             */
            if (atomic_get(&lora_rx_running))
            {
                ret = lora_radio_stop_async();

                if (ret == 0)
                {
                    atomic_set(&lora_rx_running, 0);

                    uart_cmd_SendString(
                        "[LoRa] Async RX stopped for TX\r\n");
                }
                else
                {
                    uart_cmd_SendString(
                        "[LoRa] Failed to stop RX: ");

                    uart_cmd_SendInt(ret);
                    uart_cmd_SendString("\r\n");

                    /*
                     * Keep packet in pending queue.
                     * Retry after the next scheduler wake-up.
                     */
                    tx_due = false;
                }
            }

            /*
             * Transmit packet.
             */
            if (tx_due)
            {
                ret = lora_radio_send(tx_packet.data,
                                      tx_packet.len);

                lora_log_tx(ret, tx_packet.len);

                /*
                 * Remove transmitted packet from pending queue.
                 */
                pending_count--;

                pending_packets[earliest_index] =
                    pending_packets[pending_count];
            }
        }

        /*-------------------------------------------------------------------
         * 4. Reconcile RX state
         *-------------------------------------------------------------------*/

        if (atomic_get(&lora_rx_enable) &&
            !atomic_get(&lora_rx_running))
        {
            ret = lora_radio_receive_async(lora_rx_callback, NULL);

            if (ret == 0)
            {
                atomic_set(&lora_rx_running, 1);

                uart_cmd_SendString(
                    "[LoRa] Async RX started\r\n");
            }
            else
            {
                uart_cmd_SendString(
                    "[LoRa] Failed to start RX: ");

                uart_cmd_SendInt(ret);
                uart_cmd_SendString("\r\n");
            }
        }
        else if (!atomic_get(&lora_rx_enable) &&
                 atomic_get(&lora_rx_running))
        {
            ret = lora_radio_stop_async();

            if (ret == 0)
            {
                atomic_set(&lora_rx_running, 0);

                uart_cmd_SendString(
                    "[LoRa] Async RX stopped\r\n");
            }
            else
            {
                uart_cmd_SendString(
                    "[LoRa] Failed to stop RX: ");

                uart_cmd_SendInt(ret);
                uart_cmd_SendString("\r\n");
            }
        }

        /*-------------------------------------------------------------------
         * 5. Wait for next TX event or scheduled deadline
         *-------------------------------------------------------------------*/

        if (!tx_due && (pending_count > 0U))
        {
            /*
            * TX is ready but RX could not be stopped.
            * Keep packet and retry after 100 ms.
            */
            k_sem_take(&lora_owner_wake_sem, K_MSEC(100));
        }
        else if (pending_count > 0U)
        {
            earliest_index = lora_tx_find_earliest(
                pending_packets,
                pending_count);

            now = k_uptime_get();

            wait_ms = pending_packets[earliest_index].send_at - now;

            if (wait_ms > 0)
            {
                k_sem_take(&lora_owner_wake_sem, K_MSEC(wait_ms));
            }
            else
            {
                continue;
            }
        }
        else
        {
            /*
            * No pending packet.
            */
            k_sem_take(&lora_owner_wake_sem, K_FOREVER);
        }
    }
}


/*===========================================================================
 * PUBLIC API
 *===========================================================================*/

/*
 * Initialize LoRa application.
 */
int lora_app_init(void)
{
    int ret;

    if (lora_initialized)
    {
        return 0;
    }

    k_sem_init(&lora_owner_wake_sem, 0, 1);
    k_sem_init(&lora_init_sem, 0, 1);

    atomic_set(&lora_rx_enable, 0);
    atomic_set(&lora_rx_running, 0);
    atomic_set(&lora_rx_count, 0);

    lora_init_result = -EINPROGRESS;

    /*
     * Start Owner Thread.
     */
    k_thread_create(&lora_owner_thread_data,
                    lora_owner_stack,
                    K_THREAD_STACK_SIZEOF(lora_owner_stack),
                    lora_owner_thread,
                    NULL,
                    NULL,
                    NULL,
                    LORA_OWNER_PRIORITY,
                    0,
                    K_NO_WAIT);

    /*
     * Wait for radio initialization.
     */
    ret = k_sem_take(&lora_init_sem,
                     K_MSEC(LORA_INIT_TIMEOUT_MS));

    if (ret < 0)
    {
        return -ETIMEDOUT;
    }

    if (lora_init_result < 0)
    {
        return lora_init_result;
    }

    lora_initialized = true;

    ret = lora_app_rx_start();
    if (ret != 0)
    {
        uart_cmd_SendString("[LoRa] Failed to start RX: ");
        uart_cmd_SendInt(ret);
        uart_cmd_SendString("\r\n");
    }

    return 0;
}


/*
 * Send raw data through LoRa.
 */
int lora_app_send(const uint8_t *data, size_t len, lora_tx_policy_t policy)
{
    struct lora_tx_packet packet;
    int ret;

    if (!lora_initialized)
    {
        return -EACCES;
    }

    if ((data == NULL) ||
        (len == 0U) ||
        (len > LORA_TX_MAX_LEN))
    {
        return -EINVAL;
    }

    if (policy > LORA_TX_POLICY_DISCOVERY)
    {
        return -EINVAL;
    }

    packet.len = (uint16_t)len;
    packet.policy = policy;
    packet.send_at = k_uptime_get() + lora_tx_get_delay_ms(policy);

    memcpy(packet.data, data, len);

    /*
     * Non-blocking queue operation.
     */
    ret = k_msgq_put(&lora_tx_msgq,
                     &packet,
                     K_NO_WAIT);

    if (ret != 0)
    {
        return -ENOSPC;
    }

    k_sem_give(&lora_owner_wake_sem);

    return 0;
}

/*
 * Encode and send protocol packet through LoRa.
 */
int lora_app_send_packet(const protocol_packet_t *packet, lora_tx_policy_t policy)
{
    uint8_t buffer[PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len;
    int ret;

    if (packet == NULL)
    {
        return -EINVAL;
    }

    ret = protocol_packet_encode(packet,
                                 buffer,
                                 sizeof(buffer),
                                 &encoded_len);

    if (ret != 0)
    {
        return ret;
    }

    return lora_app_send(buffer, encoded_len, policy);
}

/*
 * Receive packet from RX queue.
 *
 * timeout_ms:
 *   0  : Non-blocking
 *   >0 : Wait for specified milliseconds
 *  -1  : Wait forever
 */
int lora_app_receive(struct lora_rx_packet *packet,
                     int32_t timeout_ms)
{
    k_timeout_t timeout;

    if (!lora_initialized)
    {
        return -EACCES;
    }

    if (packet == NULL)
    {
        return -EINVAL;
    }

    if (timeout_ms < -1)
    {
        return -EINVAL;
    }

    /* Convert timeout */
    if (timeout_ms == -1)
    {
        timeout = K_FOREVER;
    }
    else if (timeout_ms == 0)
    {
        timeout = K_NO_WAIT;
    }
    else
    {
        timeout = K_MSEC(timeout_ms);
    }

    /*
     * Get packet from RX queue.
     */
    return k_msgq_get(&lora_rx_msgq,
                      packet,
                      timeout);
}

/*
 * Send string through LoRa.
 */
int lora_app_send_string(const char *msg)
{
    if (msg == NULL)
    {
        return -EINVAL;
    }

    return lora_app_send((const uint8_t *)msg,
                         strlen(msg), LORA_TX_POLICY_IMMEDIATE);
}


/*
 * Enable asynchronous RX.
 */
int lora_app_rx_start(void)
{
    if (!lora_initialized)
    {
        return -EACCES;
    }

    atomic_set(&lora_rx_enable, 1);

    k_sem_give(&lora_owner_wake_sem);

    return 0;
}


/*
 * Disable asynchronous RX.
 */
int lora_app_rx_stop(void)
{
    if (!lora_initialized)
    {
        return -EACCES;
    }

    atomic_set(&lora_rx_enable, 0);

    k_sem_give(&lora_owner_wake_sem);

    return 0;
}


/*
 * Check whether RX is requested.
 */
bool lora_app_is_rx_enabled(void)
{
    return (atomic_get(&lora_rx_enable) != 0);
}


/*
 * Check whether RX is currently running.
 */
bool lora_app_is_rx_running(void)
{
    return (atomic_get(&lora_rx_running) != 0);
}


/*
 * Get total received packet count.
 */
uint32_t lora_app_get_rx_count(void)
{
    return (uint32_t)atomic_get(&lora_rx_count);
}

static int lora_tx_find_earliest(const struct lora_tx_packet *packets, size_t count)
{
    size_t i;
    size_t earliest = 0U;

    if ((packets == NULL) || (count == 0U))
    {
        return -1;
    }

    for (i = 1U; i < count; i++)
    {
        if (packets[i].send_at < packets[earliest].send_at)
        {
            earliest = i;
        }
    }

    return (int)earliest;
}

static uint32_t lora_tx_get_delay_ms(lora_tx_policy_t policy)
{
    switch (policy)
    {
        case LORA_TX_POLICY_IMMEDIATE:
            return 0U;

        case LORA_TX_POLICY_RESPONSE:
            return 50U;

        case LORA_TX_POLICY_FORWARD:
            return 100U;

        case LORA_TX_POLICY_DISCOVERY:
            return 400U;

        default:
            return 0U;
    }
}

