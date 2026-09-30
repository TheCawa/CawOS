#include "commands.h"
#include "libc/logger.h"
#include "drivers/screen.h"
#include "libc/util.h"

void cmd_loglevel(char* args, int* row) {
    if (args == NULL || args[0] == '\0') {
        print_line_scroll("Usage: loglevel <debug|info|warn|error|fatal>", 0, row, 0x0E);
        return;
    }
    
    log_level_t level = LOG_DEBUG;
    if (strcasecmp(args, "debug") == 0) level = LOG_DEBUG;
    else if (strcasecmp(args, "info") == 0) level = LOG_INFO;
    else if (strcasecmp(args, "warn") == 0) level = LOG_WARN;
    else if (strcasecmp(args, "error") == 0) level = LOG_ERROR;
    else if (strcasecmp(args, "fatal") == 0) level = LOG_FATAL;
    else {
        print_line_scroll("Invalid level. Use: debug, info, warn, error, fatal", 0, row, 0x0C);
        return;
    }
    
    logger_set_min_level(level);
    char msg[64];
    snprintf(msg, sizeof(msg), "Log level set to %s", args);
    print_line_scroll(msg, 0, row, 0x0A);
}
REGISTER_COMMAND("loglevel", cmd_loglevel, 1);