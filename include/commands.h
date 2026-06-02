#ifndef COMMANDS_H
#define COMMANDS_H

typedef struct {
    const char* name;
    void (*func)(char* args, int* row);
    int has_args;
} command_t;

void execute_command(char* input, int* row);
extern int g_last_command_row;

#define REGISTER_COMMAND(c_name, func_ptr, args_flag) \
    __attribute__((section(".cmd"))) \
    command_t _cmd_obj_##func_ptr = { c_name, func_ptr, args_flag }

#endif