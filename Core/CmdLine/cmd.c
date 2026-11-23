
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr/kernel.h>

#include "cmd.h"
#include "cmd_line.h"

#include "../../BSP/uart_cmd/uart_cmd.h"
#include "../../Core/LED/led_sts.h"
#include "lora_app.h"
#include "ping_handler.h"
#include "../../protocol/common/protocol_config.h"




#define CMD_BUFFER_SIZE     64U
#define CMD_HISTORY_SIZE    5U

#define NAME_SHELL          "SMEANUZ"

#define KEY_ENTER           '\r'                    // [ENTER] key
#define KEY_BACKSPACE       '\x7f'                  // [BACKSPACE] key
#define KEY_ESCAPE          '\x1b'                  // [ESCAPE] key
#define KEY_HISTORY_UP      'A'                     // [-] key
#define KEY_HISTORY_DOWN    'B'                     // [=] key


typedef struct 
{
    char buffer[CMD_BUFFER_SIZE];                        // Command input buffer
    uint16_t length;

    char history[CMD_HISTORY_SIZE][CMD_BUFFER_SIZE];    // Command history buffer
    uint8_t history_count;                                          // Number of commands in history
    uint8_t history_index;                                          // Current index in history navigation

    uint8_t escape_state;

    bool ignore_lf;
} cmd_context_t;

// Command 
static int Cmd_help(int argc, char *argv[]);
static int Cmd_alive_check(int argc, char *argv[]);

static int Cmd_lora_send(int argc, char *argv[]);
static int Cmd_lora_rx(int argc, char *argv[]);
static int Cmd_lora_read(int argc, char *argv[]);
static int Cmd_lora_id(int argc, char *argv[]);
static int Cmd_lora_send_packet(int argc, char *argv[]);
static int Cmd_lora_ping(int argc, char *argv[]);
// static int Cmd_lora_status(int argc, char *argv[]);

static cmd_context_t cmd_context;

// Command table
const cmdline_entry_t g_cmd_table[] =
{
    {"help",        Cmd_help,        "Display list of commands | help"},
    {"alive_check", Cmd_alive_check, "Check system status | alive_check"},
    
    {"lora_send",   Cmd_lora_send,   "Send LoRa raw msg | lora_send <msg>"},
    {"lora_send_p", Cmd_lora_send_packet, "Send LoRa packet | <dst_id> <type> <seq> <payload>"},
    {"lora_rx",     Cmd_lora_rx,     "Control LoRa receiver | lora_rx <start|stop>"},
    {"lora_read",   Cmd_lora_read,   "Read LoRa message | lora_read"},
    {"lora_id",     Cmd_lora_id,     "Set LoRa node ID | lora_id <node_id>"},
    {"lora_ping",   Cmd_lora_ping,   "Ping another LoRa node | lora_ping <dst_id> [payload]"},
    // {"lora_sts",    Cmd_lora_status, "Display LoRa status | lora_sts"},

    {NULL, NULL, NULL}
};

static void cmd_print_prompt(void);
static void cmd_clear_line(void);
static void cmd_reset_buffer(void);

static void cmd_process_char(uint8_t data);
static void cmd_handle_enter(void);
static void cmd_handle_backspace(void);
static void cmd_handle_printable(uint8_t data);

static void cmd_handle_history_up(void);
static void cmd_handle_history_down(void);
static void cmd_save_history(const char *command);

static void cmd_execute(void);
static void cmd_print_result(int status);





void cmdline_init(void)
{
    memset(&cmd_context, 0, sizeof(cmd_context));
    cmd_context.history_index = 0U;

    uart_cmd_SendString("\r\nWelcome to " NAME_SHELL
                    " Command Line Interface!\r\n");

    cmd_print_prompt();
}

void task_cmdline_update(void)
{
    uint8_t data;
    if (uart_cmd_ReadChar(&data) == 0U)
    {
        cmd_process_char(data);
    }
}



static void cmd_print_prompt(void)
{
    char prompt[32];

    snprintf(prompt, sizeof(prompt), "%s$ ", NAME_SHELL);
    uart_cmd_SendString(prompt);
}

static void cmd_clear_line(void)
{
    uart_cmd_SendString("\r\033[2K"); // Clear line
    uart_cmd_SendString(NAME_SHELL "$ ");
}

static void cmd_reset_buffer(void)
{
    memset(cmd_context.buffer, 0, sizeof(cmd_context.buffer));
    cmd_context.length = 0U;
}

static void cmd_process_char(uint8_t data)
{
    if (cmd_context.escape_state != 0U)
    {
        if (cmd_context.escape_state == 1U)
        {
            cmd_context.escape_state = (data == '[') ? 2U : 0U;
            return;
        }

        if (cmd_context.escape_state == 2U) 
        {
            cmd_context.escape_state = 0U;
            switch (data)
            {
                case KEY_HISTORY_UP:
                    cmd_handle_history_up();
                    break;

                case KEY_HISTORY_DOWN:
                    cmd_handle_history_down();
                    break;

                default:
                    break;
            }

            return;
        }
    }

    if (data == KEY_ESCAPE)
    {
        cmd_context.escape_state = 1U;
        return;
    }

    if ((data == '\n') || (data == '\r'))
    {
        if (cmd_context.ignore_lf && (data == '\n'))
        {
            cmd_context.ignore_lf = false;
            return;
        }
        
        cmd_context.ignore_lf = (data == '\r');
        cmd_handle_enter();
        return;
    }


    cmd_context.ignore_lf = false;

    switch (data)
    {
        // case KEY_ENTER:
        //     cmd_context.ignore_lf = true;
        //     cmd_handle_enter();
        //     break;
        
        case KEY_BACKSPACE:
            cmd_handle_backspace();
            break;

        default:
            if ((data >= 32U) && (data <= 126U)) // Printable ASCII range
            {
                cmd_handle_printable(data);
            }
            break;
    }
}

// Key handling functions

static void cmd_handle_printable(uint8_t data)
{
    if (cmd_context.length >= (CMD_BUFFER_SIZE - 1U))
    {
        uart_cmd_SendString("\r\nError: Command buffer is full!\r\n");
        
        cmd_reset_buffer();
        cmd_print_prompt();

        return;
    }

    cmd_context.buffer[cmd_context.length++] = (char)data;
    cmd_context.buffer[cmd_context.length] = '\0';

    uart_cmd_SendChar(data);
}

static void cmd_handle_backspace(void)
{
    if (cmd_context.length > 0U)
    {
        cmd_context.length--;
        cmd_context.buffer[cmd_context.length] = '\0';
        uart_cmd_SendString("\b \b");
    }
}

static void cmd_handle_enter(void)
{
    uart_cmd_SendString("\r\n");

    if (cmd_context.length == 0U)
    {
        cmd_print_prompt();
        return;
    }

    cmd_context.buffer[cmd_context.length] = '\0';

    cmd_save_history(cmd_context.buffer);

    cmd_execute();

    cmd_reset_buffer();
    cmd_context.history_index = cmd_context.history_count;

    cmd_print_prompt();
}

static void cmd_execute(void)
{
    char command[CMD_BUFFER_SIZE];
    int status;

    memcpy(command, cmd_context.buffer, sizeof(command));
    command[sizeof(command) - 1U] = '\0';

    status = cmdline_process(command);

    cmd_print_result(status);
}

static void cmd_print_result(int status)
{
    static const char *const status_messages[] =
    {
        "OK",
        "Unknown command",
        "Too many arguments",
        "Too few arguments",
        "Invalid argument",
        "Pending...",
        "Null pointer",
    };

    if ((status < 0U) || ((size_t)status >= sizeof(status_messages) / sizeof(status_messages[0])))
    {
        uart_cmd_SendString("\r\nCommand execution failed");
        return;
    }

    if (status != CMDLINE_OK)
    {
        uart_cmd_SendString("Error: ");
        uart_cmd_SendString(status_messages[status]);
    }

    uart_cmd_SendString("\r\n");
}

static void cmd_save_history(const char *command)
{
    if ((command == NULL) || (command[0] == '\0'))
    {
        return;
    }

    /* Avoid saving duplicated consecutive commands */
    if ((cmd_context.history_count > 0U) &&
        (strcmp(cmd_context.history[cmd_context.history_count - 1U],
                command) == 0))
    {
        return;
    }

    if (cmd_context.history_count < CMD_HISTORY_SIZE)
    {
        strcpy(cmd_context.history[cmd_context.history_count], command);
        cmd_context.history_count++;
    }
    else
    {
        for (uint8_t i = 1U; i < CMD_HISTORY_SIZE; i++)
        {
            strcpy(cmd_context.history[i - 1U], cmd_context.history[i]);
        }

        strcpy(cmd_context.history[CMD_HISTORY_SIZE - 1U], command);
    }
}

static void cmd_handle_history_up(void)
{
    if (cmd_context.history_count == 0U)
    {
        return;
    }

    if (cmd_context.history_index > 0U)
    {
        cmd_context.history_index--;
    }

    strcpy(cmd_context.buffer, cmd_context.history[cmd_context.history_index]);

    cmd_context.length = strlen(cmd_context.buffer);

    cmd_clear_line();
    uart_cmd_SendString(cmd_context.buffer);
}

static void cmd_handle_history_down(void)
{
    if (cmd_context.history_index < cmd_context.history_count)
    {
        cmd_context.history_index++;
    }

    if (cmd_context.history_index < cmd_context.history_count)
    {
        strcpy(cmd_context.buffer, cmd_context.history[cmd_context.history_index]);
        cmd_context.length = strlen(cmd_context.buffer);
    }
    else
    {
        cmd_reset_buffer();
    }

    cmd_clear_line();
    uart_cmd_SendString(cmd_context.buffer);
}


// Command implementations

static int Cmd_help(int argc, char *argv[])
{
    size_t max_length = 0U;
    const cmdline_entry_t *entry;

    (void)argv;

    if (argc != 1)
    {
        return CMDLINE_TOO_MANY_ARGS;
    }

    /* Find the longest command name */
    for (entry = g_cmd_table; entry->cmd != NULL; entry++)
    {
        size_t length = strlen(entry->cmd);

        if (length > max_length)
        {
            max_length = length;
        }
    }

    uart_cmd_SendString("\r\nAvailable commands:");

    /* Display command list */
    for (entry = g_cmd_table; entry->cmd != NULL; entry++)
    {
        char line[128];

        snprintf(line, sizeof(line),
                 "\r\n  %-*s : %s",
                 (int)max_length,
                 entry->cmd,
                 entry->help);

        uart_cmd_SendString(line);
    }

    uart_cmd_SendString("\r\n");

    return CMDLINE_OK;
}

static int Cmd_alive_check(int argc, char *argv[])
{
    (void)argv;

    if (argc != 1)
    {
        return CMDLINE_TOO_MANY_ARGS;
    }

    uart_cmd_SendString("\r\nSystem is alive and running!");

    uart_cmd_SendString("\r\n");

    return CMDLINE_OK;
}

static int Cmd_lora_send(int argc, char *argv[])
{
    int ret;
    char message[CMD_BUFFER_SIZE];

    if (argc < 2)
    {
        return CMDLINE_TOO_FEW_ARGS;
    }

    message[0] = '\0';

    for (int i = 1; i < argc; i++)
    {
        size_t current_len = strlen(message);
        size_t arg_len = strlen(argv[i]);

        if ((current_len + arg_len + 2U) > sizeof(message))
        {
            return CMDLINE_INVALID_ARG;
        }

        if (i > 1)
        {
            strcat(message, " ");
        }

        strcat(message, argv[i]);
    }

    ret = lora_app_send_string(message);

    if (ret == 0)
    {
        uart_cmd_SendString("[CLI] LoRa message sent\r\n");
        return CMDLINE_OK;
    }

    uart_cmd_SendString("[CLI] LoRa TX failed: ");
    uart_cmd_SendInt(ret);
    uart_cmd_SendString("\r\n");

    return CMDLINE_INVALID_ARG;
}

static int Cmd_lora_rx(int argc, char *argv[])
{
    int ret;

    if (argc != 2)
    {
        return argc < 2 ? CMDLINE_TOO_FEW_ARGS : CMDLINE_TOO_MANY_ARGS;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        ret = lora_app_rx_start();
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        ret = lora_app_rx_stop();
    }
    else
    {
        return CMDLINE_INVALID_ARG;
    }

    if (ret != 0)
    {
        uart_cmd_SendString("[CLI] LoRa RX failed: ");
        uart_cmd_SendInt(ret);
        uart_cmd_SendString("\r\n");

        return CMDLINE_INVALID_ARG;
    }

    return CMDLINE_OK;
}

static int Cmd_lora_read(int argc, char *argv[])
{
    struct lora_rx_packet packet;
    int ret;

    if (argc != 1)
    {
        return CMDLINE_TOO_MANY_ARGS;
    }

    ret = lora_app_receive(&packet, 5000U);

    if (ret == 0)
    {
        uart_cmd_SendString("[CLI] LoRa RX packet received\r\n");
        uart_cmd_SendString("Data: ");
        uart_cmd_SendString((const char *)packet.data);
        uart_cmd_SendString("\r\n");
        uart_cmd_SendString("Length: ");
        uart_cmd_SendInt(packet.len);
        uart_cmd_SendString("\r\n");
        uart_cmd_SendString("RSSI: ");
        uart_cmd_SendInt(packet.rssi);
        uart_cmd_SendString(" dBm\r\n");
        uart_cmd_SendString("SNR: ");
        uart_cmd_SendInt(packet.snr);
        uart_cmd_SendString(" dB\r\n");

        return CMDLINE_OK;
    }
    else if (ret == -EAGAIN)
    {
        uart_cmd_SendString("[CLI] No LoRa RX packet available\r\n");
        return CMDLINE_OK;
    }
    else
    {
        uart_cmd_SendString("[CLI] LoRa RX failed: ");
        uart_cmd_SendInt(ret);
        uart_cmd_SendString("\r\n");
        return CMDLINE_INVALID_ARG;
    }
}

static int Cmd_lora_id(int argc, char *argv[])
{
    // uint16_t node_id;
    char *endptr;

    if (argc != 2)
    {
        uart_cmd_SendLine("Usage: lora_id <node_id>");
        return argc < 2 ? CMDLINE_TOO_FEW_ARGS : CMDLINE_TOO_MANY_ARGS;
    }

    unsigned long value = strtoul(argv[1], &endptr, 0);

    if ((*endptr != '\0') || (value > 0xFFFFUL))
    {
        uart_cmd_SendLine("[ERROR] Invalid Node ID");
        return CMDLINE_INVALID_ARG;
    }

    int ret = protocol_set_node_id((uint16_t)value);

    if (ret < 0)
    {
        uart_cmd_SendLine("[ERROR] Node ID out of range");
        return CMDLINE_INVALID_ARG;
    }

    uart_cmd_SendString("Node ID updated: ");
    uart_cmd_SendInt(protocol_get_node_id());
    uart_cmd_SendString("\r\n");

    return CMDLINE_OK;
}

static int Cmd_lora_send_packet(int argc, char *argv[])
{
    static protocol_packet_t packet;

    char *endptr;
    unsigned long value;

    int ret;

    /* Check argument count */
    if (argc < 5)
    {
        uart_cmd_SendString(
            "Usage: lora_send <dst_id> <type> <seq> <payload>\r\n");
        return CMDLINE_TOO_FEW_ARGS;
    }

    /* Parse destination ID */
    value = strtoul(argv[1], &endptr, 0);

    if ((*endptr != '\0') ||
        (value == PROTOCOL_NODE_ID_RESERVED) ||
        (value > PROTOCOL_BROADCAST_ID))
    {
        uart_cmd_SendString("[CLI] Invalid destination ID\r\n");
        return CMDLINE_INVALID_ARG;
    }

    packet.dst_id = (uint16_t)value;

    /* Parse packet type */
    value = strtoul(argv[2], &endptr, 0);

    if ((*endptr != '\0') ||
        ((value != PROTOCOL_TYPE_PING) &&
         (value != PROTOCOL_TYPE_PONG)))
    {
        uart_cmd_SendString("[CLI] Invalid packet type\r\n");
        return CMDLINE_INVALID_ARG;
    }

    packet.type = (uint8_t)value;

    /* Parse sequence number */
    value = strtoul(argv[3], &endptr, 0);

    if ((*endptr != '\0') || (value > UINT16_MAX))
    {
        uart_cmd_SendString("[CLI] Invalid sequence number\r\n");
        return CMDLINE_INVALID_ARG;
    }

    packet.seq = (uint16_t)value;

    /* Configure packet header */
    packet.version = PROTOCOL_VERSION;
    packet.src_id = protocol_get_node_id();

    /* Build payload from remaining arguments */
    packet.payload_len = 0U;

    for (int i = 4; i < argc; i++)
    {
        size_t arg_len = strlen(argv[i]);
        size_t separator_len = (i > 4) ? 1U : 0U;

        if ((packet.payload_len + separator_len + arg_len) >
            PROTOCOL_MAX_PAYLOAD_SIZE)
        {
            uart_cmd_SendString("[CLI] Payload too long\r\n");
            return CMDLINE_INVALID_ARG;
        }

        if (separator_len > 0U)
        {
            packet.payload[packet.payload_len++] = ' ';
        }

        memcpy(&packet.payload[packet.payload_len],
               argv[i],
               arg_len);

        packet.payload_len += (uint8_t)arg_len;
    }

    /* Encode and enqueue packet */
    ret = lora_app_send_packet(&packet, LORA_TX_POLICY_IMMEDIATE);

    if (ret == 0)
    {
        uart_cmd_SendString("[CLI] Protocol packet queued\r\n");

        uart_cmd_SendString("SRC=");
        uart_cmd_SendInt(packet.src_id);

        uart_cmd_SendString(" DST=");
        uart_cmd_SendInt(packet.dst_id);

        uart_cmd_SendString(" TYPE=");
        uart_cmd_SendInt(packet.type);

        uart_cmd_SendString(" SEQ=");
        uart_cmd_SendInt(packet.seq);

        uart_cmd_SendString(" LEN=");
        uart_cmd_SendInt(packet.payload_len);

        uart_cmd_SendString("\r\n");

        return CMDLINE_OK;
    }

    uart_cmd_SendString("[CLI] LoRa TX failed: ");
    uart_cmd_SendInt(ret);
    uart_cmd_SendString("\r\n");

    return CMDLINE_INVALID_ARG;
}


static int Cmd_lora_ping(int argc, char *argv[])
{
    char *endptr;
    unsigned long value;

    uint16_t dst_id;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE];
    uint8_t payload_len = 0U;

    int ret;

    /* Check argument count */
    if (argc < 2)
    {
        uart_cmd_SendString("Usage: lora_ping <dst_id> [payload]\r\n");
        return CMDLINE_TOO_FEW_ARGS;
    }

    /* Parse destination ID */
    value = strtoul(argv[1], &endptr, 0);

    if ((*endptr != '\0') ||
        (value < PROTOCOL_NODE_ID_MIN) ||
        (value > PROTOCOL_NODE_ID_MAX))
    {
        uart_cmd_SendString("[CLI] Invalid destination ID\r\n");
        return CMDLINE_INVALID_ARG;
    }

    dst_id = (uint16_t)value;

    /* Build payload from remaining arguments */
    for (int i = 2; i < argc; i++)
    {
        size_t arg_len = strlen(argv[i]);
        size_t separator_len = (i > 2) ? 1U : 0U;

        if ((payload_len + separator_len + arg_len) >
            PROTOCOL_MAX_PAYLOAD_SIZE)
        {
            uart_cmd_SendString("[CLI] Payload too long\r\n");
            return CMDLINE_INVALID_ARG;
        }

        if (separator_len > 0U)
        {
            payload[payload_len++] = ' ';
        }

        memcpy(&payload[payload_len], argv[i], arg_len);
        payload_len += (uint8_t)arg_len;
    }

    /* Send PING request */
    ret = ping_handler_send(dst_id,
                           (payload_len > 0U) ? payload : NULL,
                           payload_len);

    if (ret == 0)
    {
        uart_cmd_SendString("[CLI] PING request queued\r\n");
        return CMDLINE_OK;
    }

    uart_cmd_SendString("[CLI] PING request failed: ");
    uart_cmd_SendInt(ret);
    uart_cmd_SendString("\r\n");

    return CMDLINE_INVALID_ARG;
}
