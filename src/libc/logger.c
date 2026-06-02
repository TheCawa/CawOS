#include "libc/logger.h"
#include "drivers/screen.h"
#include "libc/util.h"
#include <stdarg.h>
#include <stddef.h>

#define LOG_BUFFER_SIZE 64
#define LOG_MAX_MSG_LEN 128

static const unsigned char LOG_COLORS[] = {
    [LOG_DEBUG] = 0x08,
    [LOG_INFO]  = 0x07,
    [LOG_WARN]  = 0x0E,
    [LOG_ERROR] = 0x0C,
    [LOG_FATAL] = 0x4F
};

static const char* LOG_LEVEL_STR[] = {
    [LOG_DEBUG] = "DEBUG",
    [LOG_INFO]  = "INFO ",
    [LOG_WARN]  = "WARN ",
    [LOG_ERROR] = "ERROR",
    [LOG_FATAL] = "FATAL"
};

static log_entry_t log_buffer[LOG_BUFFER_SIZE];
static int log_write_idx = 0;
static int log_count = 0;
static bool screen_output = true;
static log_level_t min_level = LOG_DEBUG;
static int log_screen_row = 0;
static void safe_strcpy(char* dest, const char* src, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static int log_vsnprintf(char* buf, int size, const char* fmt, va_list args) {
    int i = 0;
    while (*fmt && i < size - 1) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    char* str = va_arg(args, char*);
                    if (str) {
                        while (*str && i < size - 1) buf[i++] = *str++;
                    }
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    char numbuf[12];
                    itoa(num, numbuf);
                    for (int j = 0; numbuf[j] && i < size - 1; j++) buf[i++] = numbuf[j];
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    char hex[] = "0123456789abcdef";
                    char tmp[9] = {0};
                    for (int j = 7; j >= 0; j--) {
                        tmp[j] = hex[num & 0xF];
                        num >>= 4;
                    }
                    int start = 0;
                    while (start < 7 && tmp[start] == '0') start++;
                    for (int j = start; tmp[j] && i < size - 1; j++) buf[i++] = tmp[j];
                    break;
                }
                case 'c': {
                    buf[i++] = (char)va_arg(args, int);
                    break;
                }
                case '%':
                    buf[i++] = '%';
                    break;
                default:
                    buf[i++] = '%';
                    if (*fmt) buf[i++] = *fmt;
                    break;
            }
        } else {
            buf[i++] = *fmt;
        }
        fmt++;
    }
    buf[i] = '\0';
    return i;
}

void logger_init(void) {
    memset(log_buffer, 0, sizeof(log_buffer));
    log_write_idx = 0;
    log_count = 0;
    screen_output = true;
    min_level = LOG_DEBUG;
    log_screen_row = 0;
}

void logger_enable_screen(bool enable) {
    screen_output = enable;
}

void logger_set_min_level(log_level_t level) {
    min_level = level;
}

const log_entry_t* logger_get_entry(int index) {
    if (index < 0 || index >= log_count) return NULL;
    int idx = (log_write_idx - 1 - index + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    return &log_buffer[idx];
}

int logger_get_entry_count(void) {
    return log_count;
}

void log_print(log_level_t level, const char* module, const char* fmt, ...) {
    if (level < min_level) return;
    char buffer[LOG_MAX_MSG_LEN];
    int pos = 0;
    buffer[pos++] = '[';
    const char* lvl = LOG_LEVEL_STR[level];
    for (int i = 0; lvl[i] && pos < LOG_MAX_MSG_LEN - 1; i++) {
        buffer[pos++] = lvl[i];
    }
    buffer[pos++] = ']';
    buffer[pos++] = ' ';
    if (module) {
        for (int i = 0; module[i] && pos < LOG_MAX_MSG_LEN - 2; i++) {
            buffer[pos++] = module[i];
        }
        buffer[pos++] = ':';
        buffer[pos++] = ' ';
    }
    va_list args;
    va_start(args, fmt);
    log_vsnprintf(buffer + pos, LOG_MAX_MSG_LEN - pos, fmt, args);
    va_end(args);
    int idx = log_write_idx % LOG_BUFFER_SIZE;
    log_buffer[idx].level = level;
    log_buffer[idx].module = module;
    safe_strcpy(log_buffer[idx].message, buffer, LOG_MAX_MSG_LEN);
    extern volatile uint32_t system_ticks;
    log_buffer[idx].timestamp = system_ticks;
    log_write_idx++;
    if (log_count < LOG_BUFFER_SIZE) log_count++;
    if (screen_output) {
        unsigned char color = LOG_COLORS[level];
        int max_rows = screen_get_rows();
        if (log_screen_row >= max_rows) {
            log_screen_row = max_rows - 1;
        }
        print_line_scroll(buffer, 0, &log_screen_row, color);
    }
}