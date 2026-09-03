#include "kernel/syscall.h"

void _start() {
    __asm__ volatile("cli");
    sys_exit();
    while(1);
}
