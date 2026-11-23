
#include <stddef.h>
#include <string.h>

#include "cmd_line.h"


int cmdline_process(char *cmd_line)
{
    char *argv[CMDLINE_MAX_ARGS + 1U];
    char *cursor;

    int argc = 0;
    const cmdline_entry_t *cmd_entry = NULL;

    if (NULL == cmd_line) 
    {
        return CMDLINE_NULL_PTR;
    }

    cursor = cmd_line;

    while (*cursor != '\0')
    {
        // Skip leading whitespace
        while (*cursor == ' ')
        {
            *cursor = '\0';
            cursor++;
        }

        // End of command line
        if (*cursor == '\0')
        {
            break;
        }

        // Check maximum number of arguments
        if (argc >= CMDLINE_MAX_ARGS) 
        {
            return CMDLINE_TOO_MANY_ARGS;
        }

        // Store argument pointer
        argv[argc++] = cursor;

        // Find the end of current argument
        while ((*cursor != '\0') && (*cursor != ' '))
        {
            cursor++;
        }
    }

    if (argc == 0)
    {
        return CMDLINE_BAD_CMD;
    }

    argv[argc] = NULL; // Null-terminate the argument list

    for (cmd_entry = g_cmd_table; cmd_entry->cmd != NULL; cmd_entry++)
    {
        if (strcmp(argv[0], cmd_entry->cmd) == 0)
        {
            if (cmd_entry->callback == NULL)
            {
                return CMDLINE_INVALID_ARG;
            }

            return cmd_entry->callback(argc, argv);
        }
    }

    return CMDLINE_BAD_CMD;
}
