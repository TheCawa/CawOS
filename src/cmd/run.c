#include "commands.h"
#include "kernel/elf.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/memory.h"
#include "drivers/screen.h"
#include "libc/util.h"

extern int syscall_cursor_x;
extern int syscall_cursor_y;

void cmd_run(char* args, int* row) {
    if (!args || args[0] == '\0') {
        print_line_scroll("Usage: run <file.elf>", 0, row, 0x0C);
        return;
    }
    print_line_scroll("Loading ELF...", 0, row, 0x0B);
    process_t* p = process_create(args, args, 0);
    if (!p) {
        print_line_scroll("Error: failed to create process", 0, row, 0x0C);
        return;
    }
    print_line_scroll("Starting...", 0, row, 0x0A);

    char pid_msg[48];
    strcpy(pid_msg, "Started process PID: ");
    itoa(p->pid, pid_msg + strlen(pid_msg));
    print_line_scroll(pid_msg, 0, row, 0x0A);

    int max_rows = g_is_graphics ? screen_get_rows() : 25;
    int saved_row = *row;
    syscall_cursor_x = 0;
    syscall_cursor_y = saved_row;
    disable_cursor();

    scheduler_add_process(p);
    scheduler_wait_for(p);
    scheduler_reap_zombies();

    extern int g_last_command_row;
    g_last_command_row = syscall_cursor_y;
    if (g_last_command_row >= max_rows) g_last_command_row = max_rows - 1;
    *row = syscall_cursor_y;
    if (*row >= max_rows) *row = max_rows - 1;
    enable_cursor(13, 15);
    print_line_scroll("Program exited.", 0, row, 0x0F);
}

REGISTER_COMMAND("run", cmd_run, 1);
