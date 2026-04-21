#include "commands.h"
#include "screen.h"
#include "util.h"
#include "idt.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

extern command_t __start_cmd;
extern command_t __stop_cmd;

void cmd_help(char* args, int* row) {
    int page = 1;
    if (args != NULL && args[0] != '\0') page = atoi(args);
    if (page <= 0) page = 1;

    int cmds_per_page = 15;
    int skip = (page - 1) * cmds_per_page;
    int shown = 0;
    int current_idx = 0;
    char page_str[10];
    itoa(page, page_str);

    char header[40];
    memset(header, 0, 40);
    strcpy(header, "--- CawOS Help (Page ");
    strcat(header, page_str);
    strcat(header, ") ---");
    print_line_scroll(header, 0, row, 0x0B);

    command_t* cmd;
    for (cmd = &__start_cmd; cmd < &__stop_cmd; cmd++) {
        if (current_idx >= skip && shown < cmds_per_page) {
            if (*row >= 24) { scroll(); (*row)--; }
            print_at("> ", *row, 0);
            print_at((char*)cmd->name, *row, 2);
            (*row)++;
            shown++;
        }
        current_idx++;
        watchdog_reset();
    }

    if (current_idx > (page * cmds_per_page)) {
        print_line_scroll("Tip: type 'help 2' for more", 0, row, 0x0E);
    } else if (shown == 0 && page > 1) {
        print_line_scroll("No more commands here.", 0, row, 0x0C);
    }
    print_line_scroll("--------------------------", 0, row, 0x0B);
}

REGISTER_COMMAND("help", cmd_help, 1);