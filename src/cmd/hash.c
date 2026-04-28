#include "commands.h"
#include "util.h"
#include "screen.h"

void itoa_hex(unsigned int n, char* str) {
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
        print_line_scroll("Usage: hash <string>", 0, row, 0x0E);
        return;
    }
    unsigned int h = hash(args);
    char res_buf[32];
    char hex_buf[12];
    itoa_hex(h, hex_buf);
    strcpy(res_buf, "Hash: ");
    strcat(res_buf, hex_buf);
    print_line_scroll(res_buf, 0, row, 0x0B);
}

REGISTER_COMMAND("hash", cmd_hash, 1);