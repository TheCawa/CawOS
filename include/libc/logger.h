#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "libc/util.h"

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} log_level_t;

void logger_init(void);
void log_print(log_level_t level, const char* module, const char* fmt, ...);
void logger_enable_screen(bool enable);
void logger_set_min_level(log_level_t level);

typedef struct {
    log_level_t level;
    const char* module;
    char message[128];
    uint32_t timestamp;
} log_entry_t;

const log_entry_t* logger_get_entry(int index);
int logger_get_entry_count(void);

#define LOG_DEBUG(mod, fmt, ...) log_print(LOG_DEBUG, mod, fmt, ##__VA_ARGS__)
#define LOG_INFO(mod, fmt, ...)  log_print(LOG_INFO, mod, fmt, ##__VA_ARGS__)
#define LOG_WARN(mod, fmt, ...)  log_print(LOG_WARN, mod, fmt, ##__VA_ARGS__)
#define LOG_ERROR(mod, fmt, ...) log_print(LOG_ERROR, mod, fmt, ##__VA_ARGS__)
#define LOG_FATAL(mod, fmt, ...) log_print(LOG_FATAL, mod, fmt, ##__VA_ARGS__)

#endif // LOGGER_H