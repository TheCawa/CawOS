#include "commands.h"
#include "drivers/screen.h"
#include "libc/cpuid.h"
#include "libc/format.h"
#include "libc/string.h"

extern int g_is_graphics;
extern unsigned short get_total_memory(void);
extern int fs_use_ata;

void cmd_info(char* args, int* row) {
    (void)args;
    char cpu_model[CPUID_BRAND_LEN];
    char line[96];
    int brand_status = cpuid_get_brand(cpu_model);
    if (brand_status == CPUID_BRAND_NOT_FOUND) {
        cpuid_get_vendor(cpu_model);

        if (cpu_model[0] == '\0') {
            safe_strcpy(cpu_model, "Unknown", sizeof(cpu_model));
        }
    }
    print_line_scroll("CawOS v0.3.6", 0, row, 0x0B);
    snprintf(line, sizeof(line), "CPU: %s", cpu_model);
    print_line_scroll(line, 0, row, 0x0F);
    int mem_mb = (int)get_total_memory();
    snprintf(line, sizeof(line), "RAM: %d MB", mem_mb);
    print_line_scroll(line, 0, row, 0x0F);
    if (fs_use_ata) {
        snprintf(line, sizeof(line), "Storage: ATA Driver (PIO Mode)");
    } else {
        snprintf(line, sizeof(line), "Storage: BIOS Thunk (INT 13h)");
    }
    print_line_scroll(line, 0, row, 0x0F);
    if (g_is_graphics && g_width > 0) {
        snprintf(
            line,
            sizeof(line),
            "Display: VESA Graphics (%ux%u @ %ubpp)",
            (unsigned int)g_width,
            (unsigned int)g_height,
            (unsigned int)g_bpp
        );
        print_line_scroll(line, 0, row, 0x0F);
        snprintf(
            line,
            sizeof(line),
            "         Grid: %ux%u chars",
            (unsigned int)g_cols,
            (unsigned int)g_rows
        );
        print_line_scroll(line, 0, row, 0x0F);
    } else {
        snprintf(line, sizeof(line), "Display: VGA Text Mode (80x25)");
        print_line_scroll(line, 0, row, 0x0F);
    }
}

REGISTER_COMMAND("info", cmd_info, 0);