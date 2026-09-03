#include "commands.h"
#include "fs.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "kernel/memory.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

#define GREP_LINE_BUF_SIZE 512

void cmd_grep(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_line_scroll("Usage: grep <pattern> <filename>", 0, row, 0x0E);
        return;
    }

    char args_buf[256];
    safe_strcpy(args_buf, args, sizeof(args_buf));

    char* pattern = args_buf;
    char* filename = NULL;
    for (int i = 0; args_buf[i] != '\0'; i++) {
        if (args_buf[i] == ' ') {
            args_buf[i] = '\0';
            filename = args_buf + i + 1;
            break;
        }
    }

    if (filename == NULL || filename[0] == '\0') {
        print_line_scroll("Usage: grep <pattern> <filename>", 0, row, 0x0E);
        return;
    }

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

    char line[GREP_LINE_BUF_SIZE];
    int line_idx = 0;
    int found = 0;

    for (uint32_t i = 0; i <= file_size; i++) {
        char c = (i < file_size) ? file_buffer[i] : '\n';

        if (c == '\n' || line_idx >= (int)sizeof(line) - 1) {
            line[line_idx] = '\0';
            if (strstr(line, pattern) != NULL) {
                print_line_scroll(line, 0, row, 0x0F);
                found = 1;
            }
            line_idx = 0;
        } else if (c != '\r') {
            if (line_idx < (int)sizeof(line) - 1) {
                line[line_idx++] = c;
            }
        }
    }

    if (!found) {
        print_line_scroll("No matches found.", 0, row, 0x07);
    }

    free(file_buffer);
}

REGISTER_COMMAND("grep", cmd_grep, 1);
