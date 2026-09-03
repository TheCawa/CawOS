#include "kernel/syscall.h"

void _start() {
    sys_print("[spin] started\n");
    while (1) {
        /* Busy loop; preemptive timer will share the CPU. */
    }
}
