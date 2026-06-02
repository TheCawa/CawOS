#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "kernel/interrupt.h"
#ifndef NULL
#define NULL ((void*)0)
#endif

extern command_t __start_cmd;
extern command_t __stop_cmd;

void execute_command(char* input, int* row) {
    if (input[0] == '\0') return;
    clear_interrupt();
    command_t* cmd;
    for (cmd = &__start_cmd; cmd < &__stop_cmd; cmd++) { 
        int name_len = strlen(cmd->name);    
        if (cmd->has_args) {
            if (strncasecmp(input, cmd->name, name_len) == 0 && 
                (input[name_len] == ' ' || input[name_len] == '\0')) {              
                char* args = (input[name_len] == ' ') ? (input + name_len + 1) : "";
                cmd->func(args, row);
                return;
            }
        } else {
            if (strcasecmp(input, cmd->name) == 0) {
                cmd->func(NULL, row);
                return;
            }
        }
    }
    print_line_scroll("Unknown command!", 0, row, 0x0C);
}