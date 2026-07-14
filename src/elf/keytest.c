#include "kernel/syscall.h"

void _start() {
    sys_print("=================================\n");
    sys_print("    KEYBOARD INPUT TEST\n");
    sys_print("=================================\n");
    sys_print("\n");
    sys_print("Press keys (ESC to exit):\n");
    sys_print("\n");

    while(1) {
        unsigned char key = sys_getkey();

        if (key != 0) {
            if (key == 27) {
                sys_print("\n\nExiting...\n");
                break;
            }
            sys_putchar(key);
        }
    }

    sys_exit();
    while(1);
}
