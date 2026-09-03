#include "kernel/scheduler.h"
#include "kernel/gdt.h"
#include "kernel/memory.h"
#include "drivers/serial.h"
#include "libc/util.h"

#define IDLE_PID 0

process_t* current_process = NULL;

static process_t kernel_process;
static process_t* ready_head = NULL;
static process_t* ready_tail = NULL;
static uint32_t ticks_since_switch = 0;
static int scheduler_ready = 0;

extern uint32_t kernel_stack_top;
extern void enter_user_mode(uint32_t entry, uint32_t stack_top);

void scheduler_init(void) {
    memset(&kernel_process, 0, sizeof(kernel_process));
    kernel_process.pid = IDLE_PID;
    kernel_process.state = PROCESS_READY;
    kernel_process.saved_eflags = 0x200;
    kernel_process.kernel_stack_top = (uint32_t)&kernel_stack_top;
    strncpy(kernel_process.name, "kernel", sizeof(kernel_process.name) - 1);
    kernel_process.name[sizeof(kernel_process.name) - 1] = '\0';

    current_process = &kernel_process;
    ready_head = &kernel_process;
    ready_tail = &kernel_process;
    kernel_process.next = NULL;
    ticks_since_switch = 0;
    scheduler_ready = 1;
}

void scheduler_add_process(process_t* p) {
    if (!p) return;
    for (process_t* cur = ready_head; cur; cur = cur->next) {
        if (cur == p) return;
    }
    p->state = PROCESS_READY;
    p->next = NULL;
    if (ready_tail) {
        ready_tail->next = p;
    } else {
        ready_head = p;
    }
    ready_tail = p;
}

void scheduler_remove_process(process_t* p) {
    if (!p || !ready_head) return;
    if (ready_head == p) {
        ready_head = p->next;
        if (ready_tail == p) ready_tail = NULL;
    } else {
        process_t* cur = ready_head;
        while (cur->next) {
            if (cur->next == p) {
                cur->next = p->next;
                if (ready_tail == p) ready_tail = cur;
                break;
            }
            cur = cur->next;
        }
    }
    p->next = NULL;
}

static process_t* pick_next(void) {
    process_t* p = ready_head;
    if (p) {
        ready_head = p->next;
        if (!ready_head) ready_tail = NULL;
        p->next = NULL;
    }
    return p;
}

void schedule(void) {
    if (!scheduler_ready || !current_process) return;
    extern void watchdog_reset(void);
    watchdog_reset();
    process_t* prev = current_process;
    process_t* next = pick_next();

    if (prev && prev->state == PROCESS_RUNNING) {
        scheduler_add_process(prev);
    }

    if (!next) {
        next = &kernel_process;
    }

    if (next == prev) {
        next->state = PROCESS_RUNNING;
        return;
    }

    if (next->state == PROCESS_READY) {
        next->state = PROCESS_RUNNING;
    }

    ticks_since_switch = 0;
    current_process = next;
    tss_set_esp0(next->kernel_stack_top);
    context_switch(prev, next);
}

void scheduler_tick(void) {
    if (!scheduler_ready || !current_process) return;
    ticks_since_switch++;
    if (ticks_since_switch >= SCHEDULER_TICKS_PER_SLICE) {
        ticks_since_switch = 0;
        schedule();
    }
}

void scheduler_yield(void) {
    schedule();
}

void scheduler_wait_for(process_t* p) {
    if (!p) return;
    while (p->state != PROCESS_ZOMBIE && p->state != 0) {
        schedule();
    }
}

void scheduler_reap_zombies(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_ZOMBIE) {
            process_destroy(&process_table[i]);
        }
    }
}

void process_trampoline(void) {
    process_t* p = current_process;
    if (!p) {
        while (1) { __asm__ volatile("cli; hlt"); }
    }

    tss_set_esp0(p->kernel_stack_top);
    enter_user_mode(p->entry, p->stack_top);
    p->state = PROCESS_ZOMBIE;
    schedule();
    while (1) { __asm__ volatile("cli; hlt"); }
}
