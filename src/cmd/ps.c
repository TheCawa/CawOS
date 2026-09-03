#include "commands.h"
#include "kernel/process.h"
#include "drivers/screen.h"
#include "libc/util.h"

static const char* state_name(uint32_t state) {
    switch (state) {
        case PROCESS_RUNNING: return "RUNNING";
        case PROCESS_READY:   return "READY";
        case PROCESS_ZOMBIE:  return "ZOMBIE";
        default:              return "UNKNOWN";
    }
}

void cmd_ps(char* args, int* row) {
    (void)args;
    print_line_scroll("PID  STATE    NAME", 0, row, 0x0F);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* p = &process_table[i];
        if (p->state == 0) continue;

        char line[80];
        char pid_str[12];
        itoa(p->pid, pid_str);

        strcpy(line, pid_str);
        int len = strlen(line);
        while (len < 5) line[len++] = ' ';
        line[len] = '\0';

        strcat(line, state_name(p->state));
        len = strlen(line);
        while (len < 14) line[len++] = ' ';
        line[len] = '\0';

        strcat(line, p->name);
        print_line_scroll(line, 0, row, 0x0F);
    }
}

REGISTER_COMMAND("ps", cmd_ps, 0);
