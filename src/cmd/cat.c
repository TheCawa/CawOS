#include "fs.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "commands.h"
#include "kernel/memory.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

#define CAT_LINE_BUF_SIZE 512

static int cat_count_lines(const char* buf, uint32_t size) {
    int count = 0;
    int had_content = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (buf[i] == '\n') {
            count++;
            had_content = 0;
        } else if (buf[i] != '\r') {
            had_content = 1;
        }
    }
    if (had_content) count++;
    return count;
}

void cmd_cat(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_line_scroll("Usage: cat <filename> [page]", 0, row, 0x0E);
        return;
    }
    if (strcmp(args, "boot_sound_cawos") == 0) {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
        return;
    }

    char args_buf[256];
    safe_strcpy(args_buf, args, sizeof(args_buf));

    char* filename = args_buf;
    char* page_arg = NULL;
    for (int i = 0; args_buf[i] != '\0'; i++) {
        if (args_buf[i] == ' ') {
            args_buf[i] = '\0';
            page_arg = args_buf + i + 1;
            break;
        }
    }

    int page = 1;
    if (page_arg != NULL && page_arg[0] != '\0') {
        page = atoi(page_arg);
    }
    if (page < 1) page = 1;

    uint32_t file_size = fs_get_size(filename);
    if (file_size == 0) {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
        return;
    }

    uint32_t sectors = (file_size / 512) + 1;
    uint32_t buffer_size = sectors * 512;
    char* file_buffer = (char*)malloc(buffer_size);
    if (!file_buffer) {
        print_line_scroll("Error: Out of memory.", 0, row, 0x0C);
        return;
    }

    memset(file_buffer, 0, buffer_size);

    if (!fs_load_to_memory(filename, (uint8_t*)file_buffer)) {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
        free(file_buffer);
        return;
    }

    int max_rows = screen_get_rows();
    int lines_per_page = max_rows - 20;
    if (lines_per_page < 1) lines_per_page = 1;

    int total_lines = cat_count_lines(file_buffer, file_size);
    int total_pages = (total_lines + lines_per_page - 1) / lines_per_page;
    if (total_pages < 1) total_pages = 1;

    if (page > total_pages) {
        print_line_scroll("No more content.", 0, row, 0x0C);
        free(file_buffer);
        return;
    }

    int skip = (page - 1) * lines_per_page;

    char header[64];
    snprintf(header, sizeof(header), "--- %s (Page %d/%d) ---", filename, page, total_pages);
    print_line_scroll(header, 0, row, 0x0B);

    char line[CAT_LINE_BUF_SIZE];
    int line_idx = 0;
    int current_line = 0;
    int shown = 0;

    for (uint32_t i = 0; i <= file_size && shown < lines_per_page; i++) {
        char c = (i < file_size) ? file_buffer[i] : '\n';

        if (c == '\n' || line_idx >= (int)sizeof(line) - 1) {
            line[line_idx] = '\0';
            int len = line_idx;
            int max_cols = screen_get_cols();
            if (max_cols < 1) max_cols = 1;

            if (len == 0) {
                if (current_line >= skip && shown < lines_per_page) {
                    print_line_scroll("", 0, row, 0x0E);
                    shown++;
                }
                current_line++;
            } else {
                for (int off = 0; off < len; off += max_cols) {
                    int chunk_len = len - off;
                    if (chunk_len > max_cols) chunk_len = max_cols;
                    char save = line[off + chunk_len];
                    line[off + chunk_len] = '\0';
                    if (current_line >= skip && shown < lines_per_page) {
                        print_line_scroll(line + off, 0, row, 0x0E);
                        shown++;
                    }
                    current_line++;
                    line[off + chunk_len] = save;
                    if (shown >= lines_per_page) break;
                }
            }
            line_idx = 0;
        } else if (c != '\r') {
            if (line_idx < (int)sizeof(line) - 1) {
                line[line_idx++] = c;
            }
        }
    }

    if (page < total_pages) {
        char tip[80];
        snprintf(tip, sizeof(tip), "Tip: type 'cat %s %d' for more", filename, page + 1);
        print_line_scroll(tip, 0, row, 0x0E);
    } else {
        print_line_scroll("End of file.", 0, row, 0x07);
    }
    print_line_scroll("--------------------------", 0, row, 0x0B);

    free(file_buffer);
}

REGISTER_COMMAND("cat", cmd_cat, 1);
