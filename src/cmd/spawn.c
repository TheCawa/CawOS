#include "commands.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/memory.h"
#include "drivers/screen.h"
#include "libc/util.h"

void cmd_spawn(char* args, int* row) {
    if (!args || args[0] == '\0') {
        print_line_scroll("Usage: spawn <file.elf>", 0, row, 0x0C);
        return;
    }

    process_t* p = process_create(args, args, 0);
    if (!p) {
        print_line_scroll("Error: failed to create process", 0, row, 0x0C);
        return;
    }

    scheduler_add_process(p);

    char msg[64];
    strcpy(msg, "Spawned background process PID: ");
    itoa(p->pid, msg + strlen(msg));
    print_line_scroll(msg, 0, row, 0x0A);
}

REGISTER_COMMAND("spawn", cmd_spawn, 1);
