
#ifndef _CMD_LINE_H_
#define _CMD_LINE_H_

#include <stdint.h>

#define CMDLINE_MAX_ARGS        8U

typedef enum cmdline_status {
    CMDLINE_OK = 0,
    CMDLINE_BAD_CMD,
    CMDLINE_TOO_MANY_ARGS,
    CMDLINE_TOO_FEW_ARGS,
    CMDLINE_INVALID_ARG,
    CMDLINE_PENDING,
    CMDLINE_NULL_PTR,
    CMDLINE_NONE_RETURNED,
} cmdline_status_t;

typedef int (*cmdline_callback_t)(int argc, char *argv[]);

typedef struct cmdline_entry {
    const char *cmd;                  // Pointer to command string
    cmdline_callback_t callback;      // Pointer to command function 
    const char *help;                 // Pointer to help string  
} cmdline_entry_t;

extern const cmdline_entry_t g_cmd_table[];

int cmdline_process(char *cmd_line);

#endif  // _CMD_LINE_H_