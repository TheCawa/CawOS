#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "kernel/process.h"

#define SCHEDULER_TICKS_PER_SLICE 5

void scheduler_init(void);
void scheduler_add_process(process_t* p);
void scheduler_remove_process(process_t* p);
void scheduler_yield(void);
void scheduler_tick(void);
void schedule(void);
void scheduler_wait_for(process_t* p);
void scheduler_reap_zombies(void);
void process_trampoline(void);
void context_switch(process_t* prev, process_t* next);
extern process_t* current_process;

#endif
