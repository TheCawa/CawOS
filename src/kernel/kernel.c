#include "libc/keyboard_map.h"
#include "drivers/io.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "drivers/serial.h"
#include "fs.h"
#include "kernel/idt.h"
#include "kernel/gdt.h"
#include "commands.h"
#include "drivers/audio.h"
#include "acpi.h"
#include "drivers/pci.h"
#include "drivers/ac97.h"
#include "kernel/memory.h"
#include "kernel/scheduler.h"
#include "libc/logger.h"
#include "kernel/config.h"

#define NULL ((void*)0)
#define MAX_HISTORY 10
static char cmd_history[MAX_HISTORY][256];
static int history_count = 0;
static int history_idx = -1;
extern void watchdog_reset();
extern void fpu_init();
static int shift_active = 0;
static int caps_active = 0;
int COLS;
int ROWS;
int g_last_command_row = -1;
extern volatile uint32_t system_ticks;
extern void cmd_bigclock(char* args, int* row);
uint32_t g_idle_timeout_ticks = 60 * 100;  /* 60 сек при таймере 100 Гц */
int g_idle_enabled = 1;
static uint32_t last_activity = 0;

static void load_serial_config(void) {
    int dummy_row = 0;
    char saved_dir[32];
    strcpy(saved_dir, current_dir);
    fs_cd("/", &dummy_row);
    if (!fs_cd("core", &dummy_row) || !fs_cd("config", &dummy_row)) {
        fs_cd_abs(saved_dir);
        return;
    }
    uint32_t size = fs_get_size("serial");
    if (size == 0) {
        fs_cd_abs(saved_dir);
        return;
    }
    uint32_t sectors = (size + 511) / 512;
    uint32_t buffer_size = sectors * 512;
    uint8_t* buf = (uint8_t*)malloc(buffer_size + 1);
    if (!buf) {
        fs_cd_abs(saved_dir);
        return;
    }
    if (fs_load_to_memory("serial", buf)) {
        buf[size] = '\0';
        if (strstr((char*)buf, "serial_console=1") != NULL) {
            g_serial_mirror_enabled = 1;
            LOG_INFO("SERIAL", "Command output mirroring enabled");
        }
    }
    free(buf);
    fs_cd_abs(saved_dir);
}

static void draw_prompt(int row, int current_max_cols) {
    extern char current_dir[32];
    char prompt[36];
    memset(prompt, 0, 36);
    strcpy(prompt, current_dir);
    strcat(prompt, "> ");
    for (int c = 0; c < current_max_cols; c++) print_char_at(' ', row, c, 0x0F);
    print_at_color(prompt, row, 0, 0x0F);
}

static void insert_char(char key, char* key_buffer, int* buffer_idx, int* row, int* col,
                        int prompt_len, int current_max_rows, int current_max_cols) {
    if (*buffer_idx >= 1023) return;
    key_buffer[(*buffer_idx)++] = key;
    char str[2] = {key, 0};
    print_at(str, *row, *col);
    (*col)++;
    if (*col >= current_max_cols) {
        *col = 0;
        (*row)++;
        while (*row >= current_max_rows) {
            scroll();
            *row = current_max_rows - 1;
        }
    }
    update_cursor(*row, *col);
}

static void erase_char(char* key_buffer, int* buffer_idx, int* row, int* col, int prompt_len,
                       int current_max_cols) {
    if (*buffer_idx <= 0) return;
    (*buffer_idx)--;
    (*col)--;
    if (*col < 0) {
        if (*row > 0) {
            (*row)--;
            *col = current_max_cols - 1;
        } else {
            *col = prompt_len;
        }
    }
    print_char_at(' ', *row, *col, 0x0F);
    update_cursor(*row, *col);
}

static void input_pos(int idx, int prompt_len, int cols, int* line, int* col) {
    int cap = cols - prompt_len;
    if (cap < 1) cap = 1;
    if (idx < cap) { *line = 0; *col = prompt_len + idx; return; }
    int rem = idx - cap;
    *line = 1 + rem / cols;
    *col = rem % cols;
}

static void reset_input_area(char* key_buffer, int* buffer_idx, int* row, int* col,
                             int prompt_len, int current_max_rows, int current_max_cols) {
    int line, c;
    input_pos(*buffer_idx, prompt_len, current_max_cols, &line, &c);
    int start = *row - line;
    for (int i = 0; i <= line; i++) {
        int r = start + i;
        if (r < 0 || r >= current_max_rows) continue;
        for (int k = 0; k < current_max_cols; k++) print_char_at(' ', r, k, 0x0F);
    }
    *buffer_idx = 0;
    *row = start;
    draw_prompt(*row, current_max_cols);
    *col = prompt_len;
}

static void replay_into_buffer(const char* src, int len, char* key_buffer, int* buffer_idx,
                               int* row, int* col, int prompt_len,
                               int current_max_rows, int current_max_cols) {
    for (int i = 0; i < len; i++) {
        insert_char(src[i], key_buffer, buffer_idx, row, col,
                    prompt_len, current_max_rows, current_max_cols);
    }
    update_cursor(*row, *col);
}

static void autocomplete(char* key_buffer, int* buffer_idx, int* row, int* col,
                         int prompt_len, int current_max_rows, int current_max_cols) {
    if (*buffer_idx == 0) return;
    char typed[128];
    int len = (*buffer_idx < 127) ? *buffer_idx : 127;
    memcpy(typed, key_buffer, len);
    typed[len] = '\0';
    extern command_t __start_cmd;
    extern command_t __stop_cmd;
    command_t* cmd;
    command_t* match = NULL;
    int matches = 0;
    for (cmd = &__start_cmd; cmd < &__stop_cmd; cmd++) {
        if (strncasecmp(cmd->name, typed, len) == 0) {
            matches++;
            match = cmd;
        }
    }
    if (matches == 1 && match) {
        char tmp[128];
        safe_strcpy(tmp, match->name, 128);
        int cmd_len = strlen(tmp);
        reset_input_area(key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
        replay_into_buffer(tmp, cmd_len, key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
    }
}

static void history_prev(char* key_buffer, int* buffer_idx, int* row, int* col,
                         int prompt_len, int current_max_rows, int current_max_cols) {
    if (history_count == 0 || history_idx >= history_count - 1) return;
    history_idx++;
    char tmp[1024];
    safe_strcpy(tmp, cmd_history[history_count - 1 - history_idx], 1024);
    int len = strlen(tmp);
    reset_input_area(key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
    replay_into_buffer(tmp, len, key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
}

static void history_next(char* key_buffer, int* buffer_idx, int* row, int* col,
                         int prompt_len, int current_max_rows, int current_max_cols) {
    if (history_idx < 0) return;
    history_idx--;
    char tmp[1024];
    if (history_idx == -1) tmp[0] = '\0';
    else safe_strcpy(tmp, cmd_history[history_count - 1 - history_idx], 1024);
    int len = strlen(tmp);
    reset_input_area(key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
    replay_into_buffer(tmp, len, key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
}

static void submit_command(char* key_buffer, int* buffer_idx, int* row, int* col,
                           int* prompt_len, int current_max_rows, int current_max_cols) {
    key_buffer[*buffer_idx] = '\0';
    disable_cursor();
    if (*buffer_idx > 0) {
        if (history_count == 0 || strcmp(cmd_history[history_count - 1], key_buffer) != 0) {
            if (history_count < MAX_HISTORY) {
                safe_strcpy(cmd_history[history_count++], key_buffer, 256);
            } else {
                for (int i = 1; i < MAX_HISTORY; i++)
                    safe_strcpy(cmd_history[i - 1], cmd_history[i], 256);
                safe_strcpy(cmd_history[MAX_HISTORY - 1], key_buffer, 256);
            }
        }
        history_idx = -1;
        (*row)++;
        g_last_command_row = -1;
        execute_command(key_buffer, row);
        if (g_last_command_row >= 0) {
            *row = g_last_command_row;
        }
    } else {
        (*row)++;
    }
    while (*row >= current_max_rows) {
        scroll();
        *row = current_max_rows - 1;
    }
    *buffer_idx = 0;
    draw_prompt(*row, current_max_cols);
    *prompt_len = strlen(current_dir) + 2;
    *col = *prompt_len;
    enable_cursor(13, 15);
    update_cursor(*row, *col);
    last_activity = (uint32_t)system_ticks;
}

static void handle_serial_byte(char byte, char* key_buffer, int* buffer_idx, int* row, int* col,
                               int* prompt_len, int current_max_rows, int current_max_cols) {
    last_activity = (uint32_t)system_ticks;
    if (byte == '\r' || byte == '\n') {
        serial_putc(SERIAL_COM1, '\r');
        serial_putc(SERIAL_COM1, '\n');
        submit_command(key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
    } else if (byte == '\b' || byte == 0x7F) {
        serial_putc(SERIAL_COM1, '\b');
        serial_putc(SERIAL_COM1, ' ');
        serial_putc(SERIAL_COM1, '\b');
        erase_char(key_buffer, buffer_idx, row, col, *prompt_len, current_max_cols);
    } else if (byte == '\t') {
        autocomplete(key_buffer, buffer_idx, row, col, *prompt_len, current_max_rows, current_max_cols);
    } else if (byte == 0x03) {
        // Ctrl+C - clear current input
        memset(key_buffer, 0, 1024);
        *buffer_idx = 0;
        *col = *prompt_len;
        draw_prompt(*row, current_max_cols);
        update_cursor(*row, *col);
    } else if (byte == 0x0C) {
        // Ctrl+L - clear screen
        clear_screen();
        *row = 0;
        draw_prompt(*row, current_max_cols);
        *col = *prompt_len;
        update_cursor(*row, *col);
    } else if (byte >= 0x20 && byte <= 0x7E) {
        serial_putc(SERIAL_COM1, byte);
        insert_char(byte, key_buffer, buffer_idx, row, col, *prompt_len, current_max_rows, current_max_cols);
    }
}

static void poll_serial_input(char* key_buffer, int* buffer_idx, int* row, int* col,
                              int* prompt_len, int current_max_rows, int current_max_cols) {
    /* Real hardware without a COM1 receiver can return status 0xFF, making
       serial_has_data() appear always true. Cap the loop and pet the
       watchdog so a stuck UART cannot hang the system. */
    for (int i = 0; i < 256 && serial_has_data(SERIAL_COM1); i++) {
        watchdog_reset();
        char byte = serial_read(SERIAL_COM1);
        handle_serial_byte(byte, key_buffer, buffer_idx, row, col, prompt_len, current_max_rows, current_max_cols);
    }
}

void main() {
    heap_init();
    logger_init();
    LOG_INFO("SYS", "CawOS v0.3.4 Bootstrap started");
    LOG_INFO("MEM", "Heap initialized");
    uint32_t vbe_fb     = *((volatile uint32_t*)0x0520);
    uint32_t vbe_pitch  = *((volatile uint32_t*)0x0524);
    uint32_t vbe_width  = *((volatile uint32_t*)0x0528);
    uint32_t vbe_height = *((volatile uint32_t*)0x052C);
    if (vbe_fb != 0) {
        screen_init_graphics(vbe_fb, vbe_width, vbe_height, vbe_pitch);
        COLS = screen_get_cols();
        ROWS = screen_get_rows();
        LOG_INFO("VIDEO", "Graphics mode: %dx%d, pitch=%d", vbe_width, vbe_height, vbe_pitch);
    } else {
        LOG_ERROR("VIDEO", "No VBE Framebuffer found at 0x0520");
    }
    LOG_INFO("IRQ", "Initializing PIC...");
    pic_init();
    LOG_INFO("GDT", "Initializing GDT and TSS...");
    gdt_init();
    LOG_INFO("IRQ", "Initializing IDT...");
    idt_init();
    LOG_INFO("ACPI", "Parsing RSDP...");
    acpi_init();
    LOG_INFO("CPU", "Enabling FPU...");
    fpu_init();
    LOG_INFO("VFS", "Mounting FS...");
    fs_init();
    config_init();
    LOG_INFO("PCI", "Scanning bus 0...");
    pci_init();
    LOG_INFO("SCHED", "Initializing scheduler...");
    scheduler_init();
    LOG_INFO("SYS", "Core init complete. Ready for shell.");
    logger_enable_screen(false);
    load_serial_config();
    int idle_en = 1, idle_min = 1;
    config_get_idle(&idle_en, &idle_min);
    g_idle_enabled = idle_en;
    g_idle_timeout_ticks = (uint32_t)idle_min * 60 * 100;
    screen_set_font_scale(3, 2, 7, 4);
    clear_screen();
    disable_cursor();
    draw_logo();

    if (ac97_init() == 0) {
        uint32_t size = fs_get_size("boot_sound_cawos");
        if (size > 0) {
            uint32_t sectors = (size / 512) + 1;
            uint32_t buffer_size = sectors * 512;
            uint8_t* sound_buffer = (uint8_t*)malloc(buffer_size);
            if (sound_buffer && fs_load_to_memory("boot_sound_cawos", sound_buffer)) {
                ac97_play_pcm(sound_buffer, size);
            }
        }
    }

    beep(200, 1000);
    watchdog_reset();
    clear_screen();
    int row = 0; 
    int col = 2;
    int buffer_idx = 0;
    int prompt_len = 0;
    static char key_buffer[1024];
    // Shutdown clean warning temporarily disabled.
    // if (!shutdown_clean) {
    //     print_line_scroll("WARNING: System was not shut down properly.", 0, &row, 0x0C);
    // }
    print_line_scroll("CawOS v0.3.4", 0, &row, 0x0B);
    print_line_scroll("Type 'help' to see all commands.", 0, &row, 0x0F);
    row++; 
    enable_cursor(13, 15);
    extern char current_dir[32];
    char prompt[36];
    memset(prompt, 0, 36);
    strcpy(prompt, current_dir);
    strcat(prompt, "> ");
    prompt_len = strlen(prompt);
    print_at_color(prompt, row, 0, 0x0F);
    col = prompt_len;
    update_cursor(row, col);

    watchdog_reset();
    __asm__ volatile("sti");
    last_activity = (uint32_t)system_ticks;

    while(1) {
        schedule();
        scheduler_reap_zombies();

        watchdog_reset();
        int current_max_rows = screen_get_rows();
        int current_max_cols = screen_get_cols();
        if (g_idle_enabled &&
            ((uint32_t)system_ticks - last_activity) >= g_idle_timeout_ticks) {
            int dummy_row = 0;
            cmd_bigclock("--idle", &dummy_row);
            last_activity = (uint32_t)system_ticks;
            row = 0;
            draw_prompt(row, current_max_cols);
            char tmp[1024];
            int n = buffer_idx;
            memcpy(tmp, key_buffer, n);
            buffer_idx = 0;
            col = prompt_len;
            for (int i = 0; i < n; i++) {
                insert_char(tmp[i], key_buffer, &buffer_idx, &row, &col,
                            prompt_len, current_max_rows, current_max_cols);
            }
            update_cursor(row, col);
        }
        update_cursor(row, col);
        __asm__ volatile("hlt");

        poll_serial_input(key_buffer, &buffer_idx, &row, &col, &prompt_len, current_max_rows, current_max_cols);

        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
            feed_entropy(scancode);
            last_activity = (uint32_t)system_ticks;

            if (scancode & 0x80) {
                unsigned char released = scancode & 0x7F;
                if (released == LSHIFT || released == RSHIFT) shift_active = 0;
            } else {
                if (scancode == LSHIFT || scancode == RSHIFT) {
                    shift_active = 1;
                } else if (scancode == CAPSLOCK) {
                    caps_active = !caps_active;
                } else if (scancode == 0x48) {
                    history_prev(key_buffer, &buffer_idx, &row, &col, prompt_len, current_max_rows, current_max_cols);
                } else if (scancode == 0x50) {
                    history_next(key_buffer, &buffer_idx, &row, &col, prompt_len, current_max_rows, current_max_cols);
                } else if (scancode == ESC) {
                    continue;
                } else if (scancode == ENTER) {
                    submit_command(key_buffer, &buffer_idx, &row, &col, &prompt_len, current_max_rows, current_max_cols);
                } else if (scancode == BACKSPACE) {
                    erase_char(key_buffer, &buffer_idx, &row, &col, prompt_len, current_max_cols);
                } else if (scancode == 0x0F) {
                    autocomplete(key_buffer, &buffer_idx, &row, &col, prompt_len, current_max_rows, current_max_cols);
                } else {
                    char key = shift_active ? shift_map[scancode] : ascii_map[scancode];
                    if (key >= 'a' && key <= 'z') {
                        if (caps_active && !shift_active) key -= 32;
                        if (caps_active && shift_active) key += 32;
                    } else if (key >= 'A' && key <= 'Z') {
                        if (caps_active && !shift_active) key += 32;
                        if (caps_active && shift_active) key -= 32;
                    }

                    if (key != 0) {
                        insert_char(key, key_buffer, &buffer_idx, &row, &col, prompt_len, current_max_rows, current_max_cols);
                    }
                }
            }
        }
    }
}