#include "commands.h"
#include "util.h"
#include "screen.h"

extern void get_cpu_info(char* v);
extern unsigned short get_total_memory();

void cmd_info(char* args, int* row) {
    char cpu_model[48];
    char mem_str[16];
    get_cpu_info(cpu_model);
    int mem_mb = get_total_memory() + 1;
    itoa(mem_mb, mem_str);
    print_line_scroll("CawOS v0.2.5", 0, row, 0x0B);
    char cpu_line[64];
    memset(cpu_line, 0, 64);
    strcpy(cpu_line, "CPU: ");
    strcat(cpu_line, cpu_model);
    print_line_scroll(cpu_line, 0, row, 0x0F);
    char ram_line[32];
    memset(ram_line, 0, 32);
    strcpy(ram_line, "RAM: ");
    strcat(ram_line, mem_str);
    strcat(ram_line, " MB");
    print_line_scroll(ram_line, 0, row, 0x0F);
    print_line_scroll("Display: VGA Text Mode (80x25)", 0, row, 0x0F);
}
REGISTER_COMMAND("info", cmd_info, 0);