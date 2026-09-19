#include "commands.h"
#include "fs.h"
#include "libc/util.h"
#include "drivers/screen.h"

extern file_t fs[MAX_FILES];

void cmd_ls(char* args, int* row) {
    int page = 1;
    if (args != NULL && args[0] != '\0') {
        while (*args == ' ') args++;
        if (*args != '\0') page = atoi(args);
    }
    if (page < 1) page = 1;
    int max_rows = screen_get_rows();
    int lines_per_page = max_rows - 4;
    if (lines_per_page < 1) lines_per_page = 1;
    int total_entries = 0;
    int total_lines = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs[i].exists && strcmp(fs[i].dir, current_dir) == 0 &&
            strcmp(fs[i].name, "boot_sound_cawos") != 0) {
            total_entries++;
            total_lines += fs[i].is_dir ? 1 : 2;
        }
    }
    if (total_entries == 0) {
        print_line_scroll("Directory is empty.", 0, row, 0x07);
        return;
    }
    int total_pages = (total_lines + lines_per_page - 1) / lines_per_page;
    if (total_pages < 1) total_pages = 1;
    if (page > total_pages) {
        print_line_scroll("No more content.", 0, row, 0x0C);
        return;
    }
    char header[64];
    snprintf(header, sizeof(header), "--- %s (Page %d/%d) ---", current_dir, page, total_pages);
    print_line_scroll(header, 0, row, 0x0B);
    int skip = (page - 1) * lines_per_page;
    int cur_line = 0;
    int shown = 0;
    for (int i = 0; i < MAX_FILES && shown < lines_per_page; i++) {
        if (!fs[i].exists) continue;
        if (strcmp(fs[i].dir, current_dir) != 0) continue;
        if (strcmp(fs[i].name, "boot_sound_cawos") == 0) continue;
        if (cur_line >= skip + lines_per_page) break;
        if (cur_line >= skip) {
            char line[72];
            memset(line, 0, 72);
            if (fs[i].is_dir) strcpy(line, "DIR:  ");
            else strcpy(line, "FILE: ");
            strcat(line, fs[i].name);
            print_line_scroll(line, 0, row, fs[i].is_dir ? 0x0B : 0x0F);
            shown++;
        }
        cur_line++;
        if (!fs[i].is_dir) {
            if (cur_line >= skip && shown < lines_per_page) {
                char size_line[32];
                char s_buf[16];
                memset(size_line, 0, 32);
                strcpy(size_line, "SIZE: ");
                itoa(fs[i].size_bytes, s_buf);
                strcat(size_line, s_buf);
                strcat(size_line, " bytes");
                print_line_scroll(size_line, 6, row, 0x07);
                shown++;
            }
            cur_line++;
        }
    }
    if (page < total_pages) {
        char tip[48];
        snprintf(tip, sizeof(tip), "Tip: type 'ls %d' for more", page + 1);
        print_line_scroll(tip, 0, row, 0x0E);
    } else {
        char end_line[48];
        char nb[12];
        memset(end_line, 0, 48);
        itoa(total_entries, nb);
        strcpy(end_line, nb);
        strcat(end_line, " entries total.");
        print_line_scroll(end_line, 0, row, 0x07);
    }
    print_line_scroll("--------------------------", 0, row, 0x0B);
}

REGISTER_COMMAND("ls", cmd_ls, 1);