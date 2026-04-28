#include "screen.h"
#include "io.h"
#include "util.h"

unsigned char current_color = 0x0F;

void clear_screen() {
    char* video_memory = (char*) 0xb8000;
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' '; 
        video_memory[i+1] = 0x0F;
    }
}

void print_at_color(const char* message, int row, int col, unsigned char color) {
    if (row >= 25) return;
    for (int i = 0; message[i] != 0; i++) {
        print_char_at(message[i], row, col + i, color);
    }
}

void print_at(char* message, int row, int col) {
    print_at_color(message, row, col, current_color);
}

void disable_cursor() {
    port_byte_out(0x3D4, 0x0A);
    port_byte_out(0x3D5, 0x20);
}

void enable_cursor(unsigned char cursor_start, unsigned char cursor_end) {
    port_byte_out(0x3D4, 0x0A);
    port_byte_out(0x3D5, (port_byte_in(0x3D5) & 0xC0) | cursor_start);
    port_byte_out(0x3D4, 0x0B);
    port_byte_out(0x3D5, (port_byte_in(0x3D5) & 0xE0) | cursor_end);
}

void update_cursor(int row, int col) {
    if (row < 0) row = 0;
    unsigned short position = row * 80 + col;
    port_byte_out(0x3D4, 14); port_byte_out(0x3D5, (unsigned char)(position >> 8));
    port_byte_out(0x3D4, 15); port_byte_out(0x3D5, (unsigned char)(position & 0xFF));
}


void draw_logo() {
    clear_screen();
    unsigned char color = 0x0B;
    print_at_color("  ______      ______      __     __      ______      ______   ", 7, 12, color);
    print_at_color(" /\\  ___\\    /\\  __ \\    /\\ \\  _ \\ \\    /\\  __ \\    /\\  ___\\  ", 8, 12, color);
    print_at_color(" \\ \\ \\____   \\ \\  __ \\   \\ \\ \\/ \".\\ \\   \\ \\ \\/\\ \\   \\ \\___  \\ ", 9, 12, color);
    print_at_color("  \\ \\_____\\   \\ \\_\\ \\_\\   \\ \\__/\".~\\_\\   \\ \\_____\\   \\/\\_____\\", 10, 12, color);
    print_at_color("   \\/_____/    \\/_/\\/_/    \\/_/   \\/_/    \\/_____/    \\/_____/", 11, 12, color);
    print_at_color("          >> CawOS is loading your dreams... <<", 14, 18, 0x0E);
    sleep_ms(1600); 
}

void scroll() {
    char* vm = (char*) 0xb8000;
    for (int i = 0; i < 80 * 24 * 2; i++) {
        vm[i] = vm[i + 80 * 2];
    }

    for (int i = 80 * 24 * 2; i < 80 * 25 * 2; i += 2) {
        vm[i] = ' ';
        vm[i+1] = 0x0F;
    }
}

void print_line_scroll(const char* msg, int col, int* row, unsigned char color) {
    if (*row >= 24) {
        scroll();
        (*row)--;
    }
    print_at_color(msg, *row, col, color);
    (*row)++;
}

void print_char_at(char c, int row, int col, unsigned char color) {
    if (row >= 25 || col >= 80) return;
    char* video_memory = (char*) 0xb8000;
    int offset = (row * 80 + col) * 2;
    video_memory[offset] = c;
    video_memory[offset + 1] = color;
}