#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/elf.h"
#include "kernel/memory.h"
#include "libc/util.h"

#define PROCESS_STACK_SIZE       65536
#define PROCESS_KERNEL_STACK_SIZE 16384

#define LOAD_BASE_START   0x1000000
#define LOAD_SLOT_SIZE    0x0100000

process_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static uint8_t  load_slot_used[MAX_PROCESSES] = {0};

extern process_t* current_process;

static uint32_t allocate_load_base(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!load_slot_used[i]) {
            load_slot_used[i] = 1;
            return LOAD_BASE_START + (uint32_t)i * LOAD_SLOT_SIZE;
        }
    }
    return 0;
}

static void free_load_base(uint32_t base) {
    if (base < LOAD_BASE_START) return;
    int i = (int)((base - LOAD_BASE_START) / LOAD_SLOT_SIZE);
    if (i >= 0 && i < MAX_PROCESSES) {
        load_slot_used[i] = 0;
    }
}

process_t* process_create(const char* path, const char* name, uint32_t load_base) {
    if (!path || !name) return NULL;

    process_t* p = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == 0) {
            p = &process_table[i];
            break;
        }
    }
    if (!p) return NULL;

    uint32_t real_load_base = load_base;
    if (real_load_base == 0) {
        real_load_base = allocate_load_base();
        if (real_load_base == 0) return NULL;
    }

    uint8_t* user_stack = (uint8_t*)malloc(PROCESS_STACK_SIZE);
    if (!user_stack) {
        if (load_base == 0) free_load_base(real_load_base);
        return NULL;
    }

    uint8_t* kernel_stack = (uint8_t*)malloc(PROCESS_KERNEL_STACK_SIZE);
    if (!kernel_stack) {
        free(user_stack);
        if (load_base == 0) free_load_base(real_load_base);
        return NULL;
    }

    elf_result_t res = elf_load(path, real_load_base);
    if (!res.success) {
        free(user_stack);
        free(kernel_stack);
        if (load_base == 0) free_load_base(real_load_base);
        return NULL;
    }

    memset(p, 0, sizeof(process_t));
    p->pid = next_pid++;
    p->state = PROCESS_READY;
    p->entry = res.entry;
    p->stack_base = (uint32_t)user_stack;
    p->stack_top = ((uint32_t)user_stack + PROCESS_STACK_SIZE) & ~0xF;
    p->kernel_stack_base = (uint32_t)kernel_stack;
    p->kernel_stack_top = ((uint32_t)kernel_stack + PROCESS_KERNEL_STACK_SIZE) & ~0xF;
    p->load_base = real_load_base;
    p->ebp = 0;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    uint32_t* frame = (uint32_t*)(p->kernel_stack_top - 1024);
    frame[0] = 0x200;                     /* eflags with IF=1 */
    frame[1] = 0;                         /* edi */
    frame[2] = 0;                         /* esi */
    frame[3] = 0;                         /* ebx */
    frame[4] = 0;                         /* ebp */
    frame[5] = (uint32_t)process_trampoline; /* return address */
    p->saved_esp = (uint32_t)&frame[0];
    p->saved_eflags = 0x200;

    return p;
}

int process_destroy(process_t* p) {
    if (!p) return 0;
    if (p < &process_table[0] || p >= &process_table[MAX_PROCESSES]) return 0;

    if (p->stack_base) {
        free((void*)p->stack_base);
    }
    if (p->kernel_stack_base) {
        free((void*)p->kernel_stack_base);
    }
    free_load_base(p->load_base);
    memset(p, 0, sizeof(process_t));
    return 1;
}

process_t* process_get_current(void) {
    return current_process;
}

process_t* process_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != 0 && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

void process_set_current(process_t* p) {
    current_process = p;
}
