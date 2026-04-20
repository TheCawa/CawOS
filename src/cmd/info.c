#include "commands.h"
#include "util.h"
#include "screen.h"

extern void get_cpu_info(char* v);
extern unsigned short get_total_memory();

void cmd_info(char* args, int* row) {
    char vendor[13];
    char mem_str[16];
    get_cpu_info(vendor);
    int mem_mb = get_total_memory() + 1;
    itoa(mem_mb, mem_str);

    print_at("CawOS v0.2.1", *row, 0); (*row)++;
    print_at("CPU: ", *row, 0); print_at(vendor, *row, 5); (*row)++;
    print_at("RAM: ", *row, 0); print_at(mem_str, *row, 5); 
    print_at(" MB", *row, 5 + strlen(mem_str)); (*row)++;
    print_at("Display: VGA Text Mode (80x25)", *row, 0);
}
REGISTER_COMMAND("info", cmd_info, 0);