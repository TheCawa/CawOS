#include "commands.h"
#include "fs.h"
#include "util.h"
#include "screen.h"
#include "idt.h"

extern volatile unsigned char key_queue[]; 
extern volatile int key_queue_head;
extern volatile int key_queue_tail;

void cmd_rm(char* args, int* row) {
    if (args == 0 || args[0] == '\0') {
        print_line_scroll("Usage: rm <filename>", 0, row, 0x0E);
        return;
    }
    
    if (!fs_exists(args)) {
        print_line_scroll("Error: File not found.", 0, row, 0x0C);
        return;
    }

    char prompt[64];
    memset(prompt, 0, 64);
    strcpy(prompt, "Remove '");
    strcat(prompt, args);
    strcat(prompt, "'? [Y/N]");
    print_line_scroll(prompt, 0, row, 0x0F); 
    int confirmed = -1;
    disable_cursor();
    while (confirmed == -1) {
        __asm__ volatile("hlt");
        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
            if (!(scancode & 0x80)) {
                if (scancode == 0x15) {      // Y
                    confirmed = 1; 
                } else if (scancode == 0x31) { // N
                    confirmed = 0; 
                }
            }
        }
    }

    enable_cursor(13, 15);

    if (confirmed) {
        if (fs_delete(args)) {
            print_line_scroll("File removed.", 0, row, 0x0A);
        } else {
            print_line_scroll("Error: File not found.", 0, row, 0x0C);
        }
    } else {
        print_line_scroll("Aborted.", 0, row, 0x07);
    }
}

REGISTER_COMMAND("rm", cmd_rm, 1);