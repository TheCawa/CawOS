#include "commands.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "drivers/screen.h"
#include "libc/util.h"

void cmd_kill(char* args, int* row) {
    if (!args || args[0] == '\0') {
        print_line_scroll("Usage: kill <pid>", 0, row, 0x0C);
        return;
    }

    int pid = atoi(args);
    process_t* p = process_get_by_pid((uint32_t)pid);
    if (!p) {
        print_line_scroll("Error: process not found", 0, row, 0x0C);
        return;
    }

    if (p == current_process) {
        print_line_scroll("Error: cannot kill the current process", 0, row, 0x0C);
        return;
    }

    p->state = PROCESS_ZOMBIE;
    scheduler_remove_process(p);

    char msg[64];
    strcpy(msg, "Killed process PID: ");
    itoa(pid, msg + strlen(msg));
    print_line_scroll(msg, 0, row, 0x0A);
}

REGISTER_COMMAND("kill", cmd_kill, 1);
