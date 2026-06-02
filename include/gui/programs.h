#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "gui/window.h"

typedef struct {
    const char* name;
    void (*launch)(const char* args);
} program_t;

#define REGISTER_PROGRAM(prog_name, launch_func) \
    __attribute__((used, section(".programs"))) \
    const program_t __prog_##launch_func = { \
        .name = prog_name, \
        .launch = launch_func \
    }

void execute_program(const char* input);

#endif