#include "commands.h"
#include "libc/logger.h"
#include "drivers/screen.h"
#include "libc/util.h"

void cmd_dmesg(char* args, int* row) {
    int count = logger_get_entry_count();
    int show = count;
    
    if (args && args[0] != '\0') {
        int n = atoi(args);
        if (n > 0 && n < count) show = n;
    }
    
    for (int i = show - 1; i >= 0; i--) {
        const log_entry_t* entry = logger_get_entry(i);
        if (!entry) break;
        
        unsigned char color = 0x07;
        switch (entry->level) {
            case LOG_DEBUG: color = 0x08; break;
            case LOG_INFO:  color = 0x07; break;
            case LOG_WARN:  color = 0x0E; break;
            case LOG_ERROR: color = 0x0C; break;
            case LOG_FATAL: color = 0x4F; break;
        }
        
        print_line_scroll(entry->message, 0, row, color);
    }
}
REGISTER_COMMAND("dmesg", cmd_dmesg, 1);