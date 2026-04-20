#include "commands.h"
#include "screen.h"
#include "util.h"
#include "idt.h"

extern command_t __start_cmd;
extern command_t __stop_cmd;

void cmd_help(char* args, int* row) {
    print_at_color("--- CawOS Command Help ---", (*row)++, 0, 0x0B);
    
    command_t* cmd;
    for (cmd = &__start_cmd; cmd < &__stop_cmd; cmd++) {
        print_at("> ", *row, 0);
        print_at((char*)cmd->name, *row, 2);
        (*row)++;
        watchdog_reset();

        if (*row >= 24) {
             clear_screen();
             *row = 0;
        }
    }
    
    print_at_color("--------------------------", (*row)++, 0, 0x0B);
}

REGISTER_COMMAND("help", cmd_help, 0);