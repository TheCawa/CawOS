#include "kernel/syscall.h"

void _start() {
    sys_print("=================================\n");
    sys_print("    GUESS THE NUMBER GAME!\n");
    sys_print("=================================\n");
    sys_print("\n");

    sys_print("Secret number is: 86\n");
    sys_print("\n");

    sys_print("Attempt 1: Guessing 50... Too low!\n");
    sys_print("Attempt 2: Guessing 75... Too low!\n");
    sys_print("Attempt 3: Guessing 88... Too high!\n");

    sys_exit();
    while(1);
}
