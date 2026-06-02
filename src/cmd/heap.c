#include "commands.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include "kernel/memory.h"

void cmd_heap(char* args, int* row) {
    (void)args;
    heap_stats_t stats;
    heap_get_stats(&stats);
    print_line_scroll("=== Heap Statistics ===", 0, row, 0x0B);
    print_line_scroll("", 0, row, 0x0F);
    char line[80];
    strcpy(line, "Total Size:    ");
    char num[16];
    itoa(stats.total_size / 1024, num);
    strcat(line, num);
    strcat(line, " KB");
    print_line_scroll(line, 0, row, 0x0F);
    strcpy(line, "Used:          ");
    itoa(stats.used_size / 1024, num);
    strcat(line, num);
    strcat(line, " KB (");
    itoa((stats.used_size * 100) / stats.total_size, num);
    strcat(line, num);
    strcat(line, "%)");
    print_line_scroll(line, 0, row, 0x0E);
    strcpy(line, "Free:          ");
    itoa(stats.free_size / 1024, num);
    strcat(line, num);
    strcat(line, " KB (");
    itoa((stats.free_size * 100) / stats.total_size, num);
    strcat(line, num);
    strcat(line, "%)");
    print_line_scroll(line, 0, row, 0x0A);
    strcpy(line, "Largest Free:  ");
    itoa(stats.largest_free_block / 1024, num);
    strcat(line, num);
    strcat(line, " KB");
    print_line_scroll(line, 0, row, 0x0F);
    print_line_scroll("", 0, row, 0x0F);
    strcpy(line, "Total Blocks:  ");
    itoa(stats.total_blocks, num);
    strcat(line, num);
    print_line_scroll(line, 0, row, 0x0F);
    strcpy(line, "Used Blocks:   ");
    itoa(stats.used_blocks, num);
    strcat(line, num);
    print_line_scroll(line, 0, row, 0x0E);
    strcpy(line, "Free Blocks:   ");
    itoa(stats.free_blocks, num);
    strcat(line, num);
    print_line_scroll(line, 0, row, 0x0A);
    print_line_scroll("", 0, row, 0x0F);
    if (stats.free_blocks > 10 && stats.largest_free_block < stats.free_size / 2) {
        print_line_scroll("Warning: Memory is fragmented!", 0, row, 0x0C);
    }
}

REGISTER_COMMAND("heap", cmd_heap, 1);
