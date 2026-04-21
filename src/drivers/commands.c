#include "commands.h"
#include "util.h"
#include "screen.h"
#ifndef NULL
#define NULL ((void*)0)
#endif

extern command_t __start_cmd;
extern command_t __stop_cmd;

void execute_command(char* input, int* row) {
    if (input[0] == '\0') return;

    command_t* cmd;
    for (cmd = &__start_cmd; cmd < &__stop_cmd; cmd++) {
        int name_len = strlen(cmd->name);
        if (cmd->has_args) {
            if (strncmp(input, cmd->name, name_len) == 0 && 
                (input[name_len] == ' ' || input[name_len] == '\0')) {
                char* args = (input[name_len] == ' ') ? (input + name_len + 1) : "";
                cmd->func(args, row);
                return;
            }
        } else {
            if (strcmp(input, cmd->name) == 0) {
                cmd->func(NULL, row);
                return;
            }
        }
    }
    print_line_scroll("Unknown command!", 0, row, 0x0C);
}