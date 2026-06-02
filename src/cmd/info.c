#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"

extern int g_is_graphics;
extern uint32_t g_width, g_height, g_pitch, g_bpp;
extern uint32_t g_cols, g_rows;
extern void get_cpu_info(char* v);
extern unsigned short get_total_memory();

void cmd_info(char* args, int* row) {
    char cpu_model[48];
    char mem_str[16];
    get_cpu_info(cpu_model);
    int mem_mb = get_total_memory() + 1;
    itoa(mem_mb, mem_str);
    print_line_scroll("CawOS v0.3.0", 0, row, 0x0B);
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
    char display_line[64];
    memset(display_line, 0, 64);
    strcpy(display_line, "Display: ");
    if (g_is_graphics && g_width > 0) {
        char res_str[32], bpp_str[8];
        itoa(g_width, res_str);
        strcat(res_str, "x");
        itoa(g_height, mem_str);
        strcat(res_str, mem_str);
        itoa(g_bpp, bpp_str);
        strcpy(display_line, "Display: VESA Graphics (");
        strcat(display_line, res_str);
        strcat(display_line, " @ ");
        strcat(display_line, bpp_str);
        strcat(display_line, "bpp)");
        char grid_line[32];
        memset(grid_line, 0, 32);
        strcpy(grid_line, "         Grid: ");
        itoa(g_cols, grid_line + strlen(grid_line));
        strcat(grid_line, "x");
        itoa(g_rows, mem_str);
        strcat(grid_line, mem_str);
        strcat(grid_line, " chars");
        print_line_scroll(display_line, 0, row, 0x0F);
        print_line_scroll(grid_line, 0, row, 0x0F);
    } else {
        strcat(display_line, "VGA Text Mode (80x25)");
        print_line_scroll(display_line, 0, row, 0x0F);
    }
}

REGISTER_COMMAND("info", cmd_info, 0);