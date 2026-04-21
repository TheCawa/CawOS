#include "keyboard_map.h"
#include "io.h"
#include "util.h"
#include "screen.h"
#include "fs.h"
#include "idt.h"
#include "commands.h"

extern void watchdog_reset();

void __main() {} 

void main() {
    idt_init();
    fs_init(); 
    clear_screen();
    disable_cursor();
    draw_logo();
    beep();           
    watchdog_reset();
    clear_screen();
    print_at_color("CawOS v0.2.3", 0, 0, 0x0B);
    print_at("Type 'help' to see all commands.", 1, 0);
    enable_cursor(13, 15);
    char key_buffer[256];
    int col = 2, row = 2, buffer_idx = 0;

    print_at("> ", row, 0);
    update_cursor(row, col);

    watchdog_reset();
    __asm__ volatile("sti");

    while(1) {
        watchdog_reset();

        if (port_byte_in(0x64) & 0x01) {
            unsigned char scancode = port_byte_in(0x60);
            feed_entropy(scancode);
            if (scancode < 0x80) {
                if (scancode == ENTER) {
                    key_buffer[buffer_idx] = '\0';
                    row++;

                    if (buffer_idx > 0) {
                        execute_command(key_buffer, &row);
                    }


                    if (row >= 24) {
                        scroll();
                        row--;
                    }

                    buffer_idx = 0;
                    col = 2;
                    print_at("> ", row, 0);
                    update_cursor(row, col);
                }
                else if (scancode == BACKSPACE) {
                    if (buffer_idx > 0) {
                        buffer_idx--; 
                        col--;
                        print_at(" ", row, col);
                        update_cursor(row, col);
                    }
                }
                else {
                    char key = ascii_map[scancode];
                    if (key != 0 && buffer_idx < 255) {
                        key_buffer[buffer_idx++] = key;
                        char str[2] = {key, 0};
                        print_at(str, row, col);
                        col++;
                        
                        if (col >= 78) { 
                            col = 2; 
                            row++; 
                            print_at("> ", row, 0); 
                        }
                        update_cursor(row, col);
                    }
                }
            }
        }
    }
}