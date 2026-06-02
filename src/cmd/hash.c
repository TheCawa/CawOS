#include "commands.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "fs.h"
#include "kernel/memory.h"

void itoa_hex(uint32_t n, char* str) {
    char* hex_chars = "0123456789ABCDEF";
    str[0] = '0'; str[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        str[i + 2] = hex_chars[n & 0xF];
        n >>= 4;
    }
    str[10] = '\0';
}

void cmd_hash(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: hash <string> | hash -f <filename>", 0, row, 0x0E);
        return;
    }
    if (args[0] == '-' && (args[1] == 'f' || args[1] == 'F')) {
        char* filename = args + 2;
        while (*filename == ' ') filename++;
        if (*filename == '\0') {
            print_line_scroll("Error: No filename specified!", 0, row, 0x0C);
            return;
        }

        uint32_t size = fs_get_size(filename);
        if (size == 0) {
            print_line_scroll("Error: File not found or empty.", 0, row, 0x0C);
            return;
        }

        uint32_t sectors = (size / 512) + 1;
        uint32_t buffer_size = sectors * 512;
        uint8_t* file_buf = (uint8_t*)malloc(buffer_size);
        if (!file_buf) {
            print_line_scroll("Error: Out of memory.", 0, row, 0x0C);
            return;
        }

        if (fs_load_to_memory(filename, file_buf)) {
            file_buf[size] = '\0';
            unsigned int h = hash((char*)file_buf);
            char res_buf[32];
            char hex_buf[12];
            itoa_hex(h, hex_buf);
            strcpy(res_buf, "Hash: ");
            strcat(res_buf, hex_buf);
            print_line_scroll(res_buf, 0, row, 0x0B);
        } else {
            print_line_scroll("Error: File not found!", 0, row, 0x0C);
        }

        free(file_buf);
    } else {
        unsigned int h = hash(args);
        char res_buf[32];
        char hex_buf[12];
        itoa_hex(h, hex_buf);
        strcpy(res_buf, "Hash: ");
        strcat(res_buf, hex_buf);
        print_line_scroll(res_buf, 0, row, 0x0B);
    }
}

REGISTER_COMMAND("hash", cmd_hash, 1);