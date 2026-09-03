#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 16

#define PROCESS_RUNNING 1
#define PROCESS_READY   2
#define PROCESS_ZOMBIE  3

typedef struct process {
    uint32_t saved_esp;         /* offset 0: used by context_switch */
    uint32_t saved_eflags;      /* offset 4: EFLAGS snapshot */
    uint32_t kernel_stack_top;  /* offset 8: TSS ESP0 for this process */
    uint32_t pid;
    uint32_t state;
    uint32_t entry;
    uint32_t stack_base;
    uint32_t stack_top;
    uint32_t kernel_stack_base;
    uint32_t load_base;
    uint32_t ebp;
    char name[32];
    struct process* next;
} process_t;

extern process_t process_table[MAX_PROCESSES];

process_t* process_create(const char* path, const char* name, uint32_t load_base);
int process_destroy(process_t* p);
process_t* process_get_current(void);
process_t* process_get_by_pid(uint32_t pid);
void process_set_current(process_t* p);

#endif
