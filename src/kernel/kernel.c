#include "libc/keyboard_map.h"
#include "drivers/io.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "fs.h"
#include "kernel/idt.h"
#include "commands.h"
#include "drivers/audio.h"
#include "acpi.h"
#include "drivers/pci.h"
#include "drivers/ac97.h"
#include "kernel/memory.h"
#include "libc/logger.h"

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

void main() {
    logger_init();
    LOG_INFO("SYS", "CawOS v0.3.0 Bootstrap started");
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
    LOG_INFO("IRQ", "Initializing IDT...");
    idt_init();
    LOG_INFO("ACPI", "Parsing RSDP...");
    acpi_init();
    LOG_INFO("CPU", "Enabling FPU...");
    fpu_init();
    LOG_INFO("MEM", "Heap init...");
    heap_init();
    LOG_INFO("VFS", "Mounting FS...");
    fs_init();
    LOG_INFO("PCI", "Scanning bus 0...");
    pci_init();
    LOG_INFO("SYS", "Core init complete. Ready for shell.");
    logger_enable_screen(false);
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
                free(sound_buffer);
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
    print_line_scroll("CawOS v0.3.0", 0, &row, 0x0B);
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

    while(1) {
        watchdog_reset();
        update_cursor(row, col);
        __asm__ volatile("hlt");
        if (key_queue_head != key_queue_tail) {
            unsigned char scancode = key_queue[key_queue_head];
            key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
            feed_entropy(scancode);

            if (scancode & 0x80) {
                unsigned char released = scancode & 0x7F;
                if (released == LSHIFT || released == RSHIFT) shift_active = 0;
            } else {
                if (scancode == LSHIFT || scancode == RSHIFT) {
                    shift_active = 1;
                } else if (scancode == CAPSLOCK) {
                    caps_active = !caps_active;
                } else if (scancode == 0x48) { 
                    if (history_count > 0 && history_idx < history_count - 1) {
                        history_idx++;
                        for(int k = prompt_len; k < COLS; k++) print_char_at(' ', row, k, 0x0F);
                        strcpy(key_buffer, cmd_history[history_count - 1 - history_idx]);
                        buffer_idx = strlen(key_buffer);
                        print_at_color(key_buffer, row, prompt_len, 0x0F);
                        col = prompt_len + buffer_idx;
                        update_cursor(row, col);
                    }
                } else if (scancode == 0x50) {
                    if (history_idx >= 0) {
                        history_idx--;
                        for(int k = prompt_len; k < COLS; k++) print_char_at(' ', row, k, 0x0F);
                        if (history_idx == -1) {
                            memset(key_buffer, 0, 1024);
                            buffer_idx = 0;
                            col = prompt_len;
                        } else {
                            strcpy(key_buffer, cmd_history[history_count - 1 - history_idx]);
                            buffer_idx = strlen(key_buffer);
                            print_at_color(key_buffer, row, prompt_len, 0x0F);
                            col = prompt_len + buffer_idx;
                        }
                        update_cursor(row, col);
                    }
                } else if (scancode == ENTER) {
                    key_buffer[buffer_idx] = '\0';
                    disable_cursor();
                    if (buffer_idx > 0) {
                        if (history_count == 0 || strcmp(cmd_history[history_count - 1], key_buffer) != 0) {
                            if (history_count < MAX_HISTORY) {
                                strcpy(cmd_history[history_count++], key_buffer);
                            } else {
                                for (int i = 1; i < MAX_HISTORY; i++)
                                    strcpy(cmd_history[i - 1], cmd_history[i]);
                                strcpy(cmd_history[MAX_HISTORY - 1], key_buffer);
                            }
                        }
                        history_idx = -1;
                        row++;
                        g_last_command_row = -1;
                        execute_command(key_buffer, &row);
                        if (g_last_command_row >= 0) {
                            row = g_last_command_row;
                        }
                    } else {
                        row++;
                    }
                    if (row >= ROWS) {
                        scroll();
                        row = ROWS - 1;
                    }
                    buffer_idx = 0;
                    memset(prompt, 0, 36);
                    strcpy(prompt, current_dir);
                    strcat(prompt, "> ");
                    prompt_len = strlen(prompt);
                    for (int c = 0; c < COLS; c++) print_char_at(' ', row, c, 0x0F);
                    print_at_color(prompt, row, 0, 0x0F);
                    col = prompt_len;
                    enable_cursor(13, 15);
                    update_cursor(row, col);
                    continue;
                } else if (scancode == BACKSPACE) {
                    if (buffer_idx > 0) {
                        buffer_idx--; 
                        col--;
                        if (col < prompt_len) col = prompt_len;
                        print_char_at(' ', row, col, 0x0F);
                        update_cursor(row, col);
                    }
                } else if (scancode == 0x0F) {
                    if (buffer_idx == 0) continue; 
                    char typed[128];
                    int len = (buffer_idx < 127) ? buffer_idx : 127;
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
                        for(int k = prompt_len; k < COLS; k++) print_char_at(' ', row, k, 0x0F);
                        int cmd_len = strlen(match->name);
                        memcpy(key_buffer, match->name, cmd_len);
                        buffer_idx = cmd_len;
                        key_buffer[buffer_idx] = '\0';
                        print_at_color(match->name, row, prompt_len, 0x0F);
                        col = prompt_len + buffer_idx; 
                        update_cursor(row, col);
                    }
                } else {
                    char key = shift_active ? shift_map[scancode] : ascii_map[scancode];
                    if (key >= 'a' && key <= 'z') {
                        if (caps_active && !shift_active) key -= 32;
                        if (caps_active && shift_active) key += 32; 
                    } else if (key >= 'A' && key <= 'Z') {
                        if (caps_active && !shift_active) key += 32;
                        if (caps_active && shift_active) key -= 32;
                    }

                    if (key != 0 && buffer_idx < 1023) {
                        key_buffer[buffer_idx++] = key;
                        char str[2] = {key, 0};
                        print_at(str, row, col);
                        col++;
                        if (col >= COLS) {
                            col = 0;
                            row++; 
                            if (row >= ROWS) row = ROWS - 1;
                        }
                        update_cursor(row, col);
                    }
                }
            }
        }
    }
}