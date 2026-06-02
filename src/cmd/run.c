#include "commands.h"
#include "kernel/elf.h"
#include "kernel/memory.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/idt.h"

#define LOAD_BASE 0x1000000
#define STACK_SIZE 65536

extern uint32_t exit_recovery_esp;
extern uint32_t exit_recovery_ebp;

extern void run_program_asm(uint32_t entry, uint32_t stack_top);

void cmd_run(char* args, int* row) {
    if (!args || args[0] == '\0') {
        print_line_scroll("Usage: run <file.elf>", 0, row, 0x0C);
        return;
    }
    print_line_scroll("Loading ELF...", 0, row, 0x0B);
    elf_result_t res = elf_load(args, LOAD_BASE);
    if (!res.success) {
        char msg[128];
        strcpy(msg, "Error: ");
        strcat(msg, res.error);
        print_line_scroll(msg, 0, row, 0x0C);
        return;
    }
    uint8_t* prog_stack = (uint8_t*)malloc(STACK_SIZE);
    if (!prog_stack) {
        print_line_scroll("Error: out of memory", 0, row, 0x0C);
        return;
    }
    uint32_t stack_top = ((uint32_t)prog_stack + STACK_SIZE) & ~0xF;
    print_line_scroll("Starting...", 0, row, 0x0A);
    extern int syscall_cursor_x;
    extern int syscall_cursor_y;
    int max_rows = g_is_graphics ? screen_get_rows() : 25;
    int saved_row = *row;
    syscall_cursor_x = 0;
    syscall_cursor_y = saved_row;
    disable_cursor();
    run_program_asm(res.entry, stack_top);
    free(prog_stack);
    idt_reload();
    extern int g_last_command_row;
    g_last_command_row = syscall_cursor_y;
    if (g_last_command_row >= max_rows) g_last_command_row = max_rows - 1;
    *row = syscall_cursor_y;
    if (*row >= max_rows) *row = max_rows - 1;
    print_line_scroll("Program exited.", 0, row, 0x0F);
}

REGISTER_COMMAND("run", cmd_run, 1);
