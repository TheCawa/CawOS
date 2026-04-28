#include "keyboard_map.h"
#include "io.h"
#include "util.h"
#include "screen.h"
#include "fs.h"
#include "idt.h"
#include "commands.h"
#include "audio.h"
#include "acpi.h"

extern void watchdog_reset();
extern void fpu_init();
static int shift_active = 0;
static int caps_active = 0;

void main() {
    idt_init();
    acpi_init();
    fpu_init();
    fs_init();
    clear_screen();
    disable_cursor();
    draw_logo();
    beep();    
    watchdog_reset();
    clear_screen();
    int row = 0; 
    int col = 2;
    int buffer_idx = 0;
    static char key_buffer[1024];
    print_line_scroll("CawOS v0.2.5", 0, &row, 0x0B);
    print_line_scroll("Type 'help' to see all commands.", 0, &row, 0x0F);
    row++; 
    enable_cursor(13, 15);
    extern char current_dir[32];
    char prompt[36];
    memset(prompt, 0, 36);
    strcpy(prompt, current_dir);
    strcat(prompt, "> ");
    print_at_color(prompt, row, 0, 0x0F);
    col = strlen(prompt);
    update_cursor(row, col);

    watchdog_reset();
    __asm__ volatile("sti");

    while(1) {
        watchdog_reset();
        __asm__ volatile("hlt");
        if (key_queue_head != key_queue_tail) {
        unsigned char scancode = key_queue[key_queue_head];
        key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        feed_entropy(scancode);
            if (scancode & 0x80) {
                unsigned char released = scancode & 0x7F;
                if (released == LSHIFT || released == RSHIFT) {
                    shift_active = 0;
                }
            } 
            else {
                if (scancode == LSHIFT || scancode == RSHIFT) {
                    shift_active = 1;
                }
                else if (scancode == CAPSLOCK) {
                    caps_active = !caps_active;
                }
                else if (scancode == ENTER) {
                    key_buffer[buffer_idx] = '\0';
                    row++;
                    if (buffer_idx > 0) {
                        execute_command(key_buffer, &row);
                    }
                    if (row >= 24) { scroll(); row = 23; }
                    buffer_idx = 0;
                    col = 2;
                    extern char current_dir[32];
                    char prompt[36];
                    memset(prompt, 0, 36);
                    strcpy(prompt, current_dir);
                    strcat(prompt, "> ");
                    print_at_color(prompt, row, 0, 0x0F);
                    col = strlen(prompt);
                    update_cursor(row, col);
                }
                else if (scancode == BACKSPACE) {
                    if (buffer_idx > 0) {
                        buffer_idx--; 
                        col--;
                        if (col < 0) { col = 79; row--; }
                        print_at(" ", row, col);
                        update_cursor(row, col);
                    }
                }
                else {
                    char key = shift_active ? shift_map[scancode] : ascii_map[scancode];
                    if (key >= 'a' && key <= 'z' && caps_active) {
                        key -= 32;
                    } else if (key >= 'A' && key <= 'Z' && caps_active && shift_active) {
                        key += 32;
                    }

                    if (key != 0 && buffer_idx < 1023) {
                        key_buffer[buffer_idx++] = key;
                        char str[2] = {key, 0};
                        print_at(str, row, col);
                        col++;
                        if (col >= 80) {
                            col = 0;
                            row++; 
                            if (row >= 24) { scroll(); row = 23; }
                        }
                        update_cursor(row, col);
                    }
                }
            }
        }
    }
}