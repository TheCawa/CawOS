#include "libc/logger.h"
#include "drivers/screen.h"
#include "drivers/serial.h"
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

void logger_init(void) {
    serial_init(SERIAL_COM1);
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
    vsnprintf(buffer + pos, LOG_MAX_MSG_LEN - pos, fmt, args);
    va_end(args);
    int idx = log_write_idx % LOG_BUFFER_SIZE;
    log_buffer[idx].level = level;
    log_buffer[idx].module = module;
    safe_strcpy(log_buffer[idx].message, buffer, LOG_MAX_MSG_LEN);
    extern volatile uint32_t system_ticks;
    log_buffer[idx].timestamp = system_ticks;
    log_write_idx++;
    if (log_count < LOG_BUFFER_SIZE) log_count++;
    serial_puts(SERIAL_COM1, buffer);
    serial_putc(SERIAL_COM1, '\n');
    if (screen_output) {
        unsigned char color = LOG_COLORS[level];
        int max_rows = screen_get_rows();
        if (log_screen_row >= max_rows) {
            log_screen_row = max_rows - 1;
        }
        print_line_scroll(buffer, 0, &log_screen_row, color);
    }
}

void log_dump_hex(log_level_t level, const char* module, const void* data, uint32_t len) {
    if (level < min_level) return;
    const uint8_t* bytes = (const uint8_t*)data;
    char line[80];
    for (uint32_t off = 0; off < len; off += 16) {
        int pos = 0;
        snprintf(line + pos, sizeof(line) - pos, "%08x: ", off);
        pos = strlen(line);
        for (int j = 0; j < 16 && (off + j) < len; j++) {
            snprintf(line + pos, sizeof(line) - pos, "%02x ", bytes[off + j]);
            pos += 3;
        }
        for (int j = (len - off < 16) ? (int)(len - off) : 16; j < 16; j++) {
            snprintf(line + pos, sizeof(line) - pos, "   ");
            pos += 3;
        }
        snprintf(line + pos, sizeof(line) - pos, " |");
        pos = strlen(line);
        for (int j = 0; j < 16 && (off + j) < len; j++) {
            uint8_t c = bytes[off + j];
            line[pos++] = (c >= 0x20 && c <= 0x7E) ? c : '.';
        }
        line[pos++] = '|';
        line[pos] = '\0';
        log_print(level, module, "%s", line);
    }
}