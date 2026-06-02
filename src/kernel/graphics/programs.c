#include "gui/programs.h"
#include "libc/util.h"

extern program_t __start_prog;
extern program_t __stop_prog;

void execute_program(const char* input) {
    if (!input || input[0] == '\0') return;
    char buf[128];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* args = "";
    for (int i = 0; buf[i] != '\0'; i++) {
        if (buf[i] == ' ') {
            buf[i] = '\0';
            args = buf + i + 1;
            break;
        }
    }
    const program_t* prog;
    for (prog = &__start_prog; prog < &__stop_prog; prog++) {
        if (strcasecmp(buf, prog->name) == 0) {
            if (prog->launch) {
                prog->launch(args);
            }
            return;
        }
    }
    
}