#include "kernel/idt.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "drivers/io.h"
#include "kernel/interrupt.h"
#include "fs.h"
#include "kernel/memory.h"
#include "gui/mouse.h"
#include "libc/keyboard_map.h"
struct idt_entry idt[256];
struct idt_ptr idtp;
extern void isr0();
extern void idt_load(unsigned int);
extern void isr32();
extern void isr_ignore();
extern void isr13();
extern void isr14();
extern void isr33();
extern void isr44();
extern void isr128();
volatile int watchdog_counter = 0;
const int WATCHDOG_LIMIT = 5000;
volatile unsigned char key_queue[KEY_QUEUE_SIZE] = {0};
volatile int key_queue_head = 0;
volatile int key_queue_tail = 0;
volatile uint32_t system_ticks = 0;
extern volatile int screen_dirty;
extern uint32_t kernel_esp;
extern uint32_t kernel_eip;
uint32_t exit_recovery_esp;
uint32_t exit_recovery_ebp;
static volatile int left_ctrl_pressed = 0;
static volatile int right_ctrl_pressed = 0;
int syscall_cursor_x = 0;
int syscall_cursor_y = 0;
#define MAX_OPEN_FILES 16
typedef struct {
    int in_use;
    char filename[64];
    uint32_t position;
    uint32_t size;
} file_descriptor_t;

static file_descriptor_t fd_table[MAX_OPEN_FILES];

void init_fd_table() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        fd_table[i].in_use = 0;
    }
}

void int_to_hex(uint32_t n, char *str) {
    char hex_chars[] = "0123456789ABCDEF";
    str[0] = '0'; str[1] = 'x';
    for (int i = 0; i < 8; i++) {
        str[9 - i] = hex_chars[(n >> (i * 4)) & 0x0F];
    }
    str[10] = '\0';
}

void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].base_lo = (base & 0xFFFF);
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_reload() {
    idt_load((unsigned int)&idtp);
}

void draw_bsod(const char* error_name, struct registers *r) {
    char hex_buf[11];
    int is_gfx = g_is_graphics;
    int rows = is_gfx ? screen_get_rows() : 25;
    int cols = is_gfx ? screen_get_cols() : 80;
    unsigned char speaker_state = port_byte_in(0x61);
    port_byte_out(0x61, speaker_state & 0xFC);
    disable_cursor();
    if (!is_gfx) {
        char* vm = (char*)0xb8000;
        for (int i = 0; i < 80 * 25 * 2; i += 2) {
            vm[i] = ' '; vm[i+1] = 0x1F;
        }
    } else {
        uint32_t blue = 0x000000AA;
        for (uint32_t y = 0; y < g_height; y++) {
            uint8_t* row = g_shadow + y * g_pitch;
            for (uint32_t x = 0; x < g_width; x++) {
                uint8_t* pixel = row + x * (g_bpp / 8);
                uint8_t r = (blue >> 16) & 0xFF;
                uint8_t g = (blue >> 8)  & 0xFF;
                uint8_t b = (blue)       & 0xFF;
                if (g_bpp == 32) {
                    pixel[0] = b; pixel[1] = g; pixel[2] = r; pixel[3] = 0;
                } else if (g_bpp == 24) {
                    pixel[0] = b; pixel[1] = g; pixel[2] = r;
                } else if (g_bpp == 16) {
                    uint16_t c16 = ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
                    pixel[0] = c16 & 0xFF;
                    pixel[1] = c16 >> 8;
                }
            }
        }
        memcpy(g_framebuffer, g_shadow, g_height * g_pitch);
    }
    int title_row = is_gfx ? 2 : 1;
    int msg1_row = is_gfx ? 6 : 4;
    int msg2_row = is_gfx ? 8 : 5;
    int err_label_row = is_gfx ? 11 : 7;
    int err_name_row = is_gfx ? 11 : 7;
    int line1_row = is_gfx ? 14 : 9;
    int regs_label_row = is_gfx ? 16 : 10;
    int eip_label_row = is_gfx ? 19 : 12;
    int eip_val_row = is_gfx ? 19 : 12;
    int cs_label_row = is_gfx ? 19 : 12;
    int cs_val_row = is_gfx ? 19 : 12;
    int eax_label_row = is_gfx ? 22 : 14;
    int eax_val_row = is_gfx ? 22 : 14;
    int ebx_label_row = is_gfx ? 22 : 14;
    int ebx_val_row = is_gfx ? 22 : 14;
    int ecx_label_row = is_gfx ? 24 : 15;
    int ecx_val_row = is_gfx ? 24 : 15;
    int edx_label_row = is_gfx ? 24 : 15;
    int edx_val_row = is_gfx ? 24 : 15;
    int esp_label_row = is_gfx ? 27 : 17;
    int esp_val_row = is_gfx ? 27 : 17;
    int ebp_label_row = is_gfx ? 27 : 17;
    int ebp_val_row = is_gfx ? 27 : 17;
    int line2_row = is_gfx ? 30 : 19;
    int restart_row = is_gfx ? 34 : 21;
    int mid = cols / 2 - 12;
    print_at_color(" [ CawOS System Error ] ", title_row, mid, 0x1F);
    print_at_color("A fatal exception has occurred. The system has been halted", msg1_row, 3, 0x1F);
    print_at_color("to prevent damage to your computer.", msg2_row, 3, 0x1F);
    print_at_color("Error Type:", err_label_row, 3, 0x1F);
    print_at_color(error_name, err_name_row, 15, 0x1E);
    print_at_color("-----------------------------------------------", line1_row, 3, 0x1F);
    print_at_color("REGS DUMP:", regs_label_row, 3, 0x1F);
    print_at_color("EIP:", eip_label_row, 3, 0x1F);
    int_to_hex(r->eip, hex_buf);
    print_at_color(hex_buf, eip_val_row, 8, 0x1F);
    print_at_color("CS:", cs_label_row, 22, 0x1F);
    int_to_hex(r->cs, hex_buf);
    print_at_color(hex_buf, cs_val_row, 26, 0x1F);
    print_at_color("EAX:", eax_label_row, 3, 0x1F);
    int_to_hex(r->eax, hex_buf);
    print_at_color(hex_buf, eax_val_row, 8, 0x1F);
    print_at_color("EBX:", ebx_label_row, 22, 0x1F);
    int_to_hex(r->ebx, hex_buf);
    print_at_color(hex_buf, ebx_val_row, 26, 0x1F);
    print_at_color("ECX:", ecx_label_row, 3, 0x1F);
    int_to_hex(r->ecx, hex_buf);
    print_at_color(hex_buf, ecx_val_row, 8, 0x1F);
    print_at_color("EDX:", edx_label_row, 22, 0x1F);
    int_to_hex(r->edx, hex_buf);
    print_at_color(hex_buf, edx_val_row, 26, 0x1F);
    print_at_color("ESP:", esp_label_row, 3, 0x1F);
    int_to_hex(r->kernel_esp, hex_buf);
    print_at_color(hex_buf, esp_val_row, 8, 0x1F);
    print_at_color("EBP:", ebp_label_row, 22, 0x1F);
    int_to_hex(r->ebp, hex_buf);
    print_at_color(hex_buf, ebp_val_row, 26, 0x1F);
    print_at_color("-----------------------------------------------", line2_row, 3, 0x1F);
    print_at_color("Please restart your computer.", restart_row, 3, 0x1F);
}

__attribute__((force_align_arg_pointer))
void isr_handler(struct registers *r) {
    if (r->int_no == 32) {
        system_ticks++;
        port_byte_out(0x20, 0x20);
        return;
    }
    if (r->int_no == 44) {
        while (1) {
            uint8_t status = port_byte_in(0x64);
            if (!(status & 0x01)) break;
            uint8_t byte = port_byte_in(0x60);
            mouse_process_byte(byte);
        }
        port_byte_out(0xA0, 0x20);
        port_byte_out(0x20, 0x20);
        return;
    }
    if (r->int_no == 33) {
        while (1) {
            uint8_t status = port_byte_in(0x64);
            if (!(status & 0x01)) break;
            uint8_t byte = port_byte_in(0x60);
            if (byte == 0x1D) left_ctrl_pressed = 1;
            else if (byte == 0x9D) left_ctrl_pressed = 0;
            if ((left_ctrl_pressed || right_ctrl_pressed) && byte == 0x2E) {
                request_interrupt();
            }
            int next = (key_queue_tail + 1) % KEY_QUEUE_SIZE;
            if (next != key_queue_head) {
                key_queue[key_queue_tail] = byte;
                key_queue_tail = next;
            }
        }
        port_byte_out(0x20, 0x20);
        return;
    }
    if (r->int_no == 0x80) {
        extern int screen_get_cols();
        extern int screen_get_rows();
        extern void clear_screen();
        uint32_t syscall_num = r->eax;
        uint32_t param1 = r->ebx;
        uint32_t param2 = r->ecx;
        uint32_t param3 = r->edx;
        switch (syscall_num) {
            case 1: // sys_exit
                __asm__ volatile(
                    "mov %0, %%esp\n"
                    "sti\n"
                    "pop %%ebp\n"
                    "ret\n"
                    : : "m"(exit_recovery_esp) : "memory"
                );
                __builtin_unreachable();
                break;

            case 2: // sys_putchar
                {
                    unsigned char c = (unsigned char)(param1 & 0xFF);
                    int max_cols = g_is_graphics ? screen_get_cols() : 80;
                    int max_rows = g_is_graphics ? screen_get_rows() : 25;

                        if (c == '\n') {
                            syscall_cursor_x = 0;
                            syscall_cursor_y++;
                            if (syscall_cursor_y >= max_rows) {
                                scroll();
                                syscall_cursor_y = max_rows - 1;
                            }
                        } else {
                            if (syscall_cursor_y >= max_rows) {
                                scroll();
                                syscall_cursor_y = max_rows - 1;
                            }
                            print_char_at(c, syscall_cursor_y, syscall_cursor_x, 0x0F);
                        syscall_cursor_x++;
                        if (syscall_cursor_x >= max_cols) {
                            syscall_cursor_x = 0;
                            syscall_cursor_y++;
                        }
                    }
                }
                break;

            case 3: // sys_print
                {
                    const char* str = (const char*)param1;
                    if (!str) break;
                    int max_cols = g_is_graphics ? screen_get_cols() : 80;
                    int max_rows = g_is_graphics ? screen_get_rows() : 25;

                    for (int i = 0; i < 256 && str[i] != '\0'; i++) {
                        unsigned char c = str[i];

                        if (c == '\n') {
                            syscall_cursor_x = 0;
                            syscall_cursor_y++;
                            if (syscall_cursor_y >= max_rows) {
                                scroll();
                                syscall_cursor_y = max_rows - 1;
                            }
                        } else {
                            if (syscall_cursor_y >= max_rows) {
                                scroll();
                                syscall_cursor_y = max_rows - 1;
                            }
                            print_char_at(c, syscall_cursor_y, syscall_cursor_x, 0x0F);
                            syscall_cursor_x++;
                            if (syscall_cursor_x >= max_cols) {
                                syscall_cursor_x = 0;
                                syscall_cursor_y++;
                            }
                        }
                    }
                }
                break;
                
            case 4: // sys_clear
                clear_screen();
                syscall_cursor_x = 0;
                syscall_cursor_y = 0;
                break;
                
            case 5: // sys_setcursor
                syscall_cursor_y = (int)param1;
                syscall_cursor_x = (int)param2;
                break;

            case 6: // sys_getkey
                {
                    extern volatile unsigned char key_queue[KEY_QUEUE_SIZE];
                    extern volatile int key_queue_head;
                    extern volatile int key_queue_tail;

                    if (key_queue_head != key_queue_tail) {
                        unsigned char scancode = key_queue[key_queue_head];
                        key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
                        if (scancode & 0x80) {
                            r->eax = 0;
                            break;
                        }
                        if (scancode == 0x2A || scancode == 0x36 || scancode == 0x1D || scancode == 0x3A) {
                            r->eax = 0;
                            break;
                        }
                        unsigned char key = ascii_map[scancode];
                        r->eax = key;
                    } else {
                        r->eax = 0;
                    }
                }
                break;

            case 7: // sys_open
                {
                    const char* filename = (const char*)param1;

                    if (!filename) {
                        r->eax = -1;
                        break;
                    }
                    int fd = -1;
                    for (int i = 0; i < MAX_OPEN_FILES; i++) {
                        if (!fd_table[i].in_use) {
                            fd = i;
                            break;
                        }
                    }

                    if (fd == -1) {
                        r->eax = -1;
                        break;
                    }
                    if (!fs_exists((char*)filename)) {
                        r->eax = -1;
                        break;
                    }
                    fd_table[fd].in_use = 1;
                    strcpy(fd_table[fd].filename, filename);
                    fd_table[fd].position = 0;
                    fd_table[fd].size = fs_get_size((char*)filename);
                    r->eax = fd;
                }
                break;

            case 8: // sys_read
                {
                    int fd = (int)param1;
                    void* buffer = (void*)param2;
                    uint32_t size = param3;

                    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use || !buffer) {
                        r->eax = -1;
                        break;
                    }
                    uint32_t remaining = fd_table[fd].size - fd_table[fd].position;
                    if (size > remaining) size = remaining;

                    if (size == 0) {
                        r->eax = 0;
                        break;
                    }
                    uint32_t file_size = fd_table[fd].size;
                    uint32_t sectors = (file_size / 512) + 1;
                    uint32_t buffer_size = sectors * 512;
                    uint8_t* temp_buffer = (uint8_t*)malloc(buffer_size);
                    if (!temp_buffer) {
                        r->eax = -1;
                        break;
                    }
                    if (fs_load_to_memory(fd_table[fd].filename, temp_buffer)) {
                        memcpy(buffer, temp_buffer + fd_table[fd].position, size);
                        fd_table[fd].position += size;
                        r->eax = size;
                    } else {
                        r->eax = -1;
                    }

                    free(temp_buffer);
                }
                break;

            case 9: // sys_write
                {
                    int fd = (int)param1;
                    const void* buffer = (const void*)param2;
                    uint32_t size = param3;
                    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use || !buffer) {
                        r->eax = -1;
                        break;
                    }
                    uint32_t new_size = fd_table[fd].position + size;
                    if (new_size > 65536) {
                        r->eax = -1;
                        break;
                    }
                    uint32_t sectors = (new_size / 512) + 1;
                    uint32_t buffer_size = sectors * 512;
                    uint8_t* temp_buffer = (uint8_t*)malloc(buffer_size);
                    if (!temp_buffer) {
                        r->eax = -1;
                        break;
                    }
                    uint32_t existing_size = fd_table[fd].size;
                    if (existing_size > 0) {
                        fs_load_to_memory(fd_table[fd].filename, temp_buffer);
                    }
                    memcpy(temp_buffer + fd_table[fd].position, buffer, size);
                    if (fs_write_file(fd_table[fd].filename, temp_buffer, new_size)) {
                        fd_table[fd].position += size;
                        fd_table[fd].size = new_size;
                        r->eax = size;
                    } else {
                        r->eax = -1;
                    }

                    free(temp_buffer);
                }
                break;

            case 10: // sys_close
                {
                    int fd = (int)param1;

                    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
                        r->eax = -1;
                        break;
                    }

                    fd_table[fd].in_use = 0;
                    r->eax = 0;
                }
                break;
        }
        return;
    }
    if (r->int_no == 255) {
        return;
    }
    char* err_desc;
    switch (r->int_no) {
        case 0:  err_desc = "DIVIDE BY ZERO"; break;
        case 13: err_desc = "GENERAL PROTECTION FAULT"; break;
        case 14: err_desc = "PAGE FAULT"; break;
        default: err_desc = "UNKNOWN EXCEPTION"; break;
    }
    draw_bsod(err_desc, r);
    __asm__ volatile("cli; hlt");
}

void init_timer(int frequency) {
    if (frequency <= 0) frequency = 100;
    int divisor = 1193180 / frequency;
    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, divisor & 0xFF);
    port_byte_out(0x40, (divisor >> 8) & 0xFF);
}

void idt_init() {
    init_fd_table();
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    port_byte_out(0x20, 0x11);
    port_byte_out(0xA0, 0x11);
    port_byte_out(0x21, 0x20); 
    port_byte_out(0xA1, 0x28);
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    port_byte_out(0x21, 0xF8);
    port_byte_out(0xA1, 0xEF);

    memset(&idt, 0, sizeof(struct idt_entry) * 256);

    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned int)isr_ignore, 0x08, 0x8E);
    }

    idt_set_gate(0, (unsigned int)isr0, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned int)isr14, 0x08, 0x8E);
    idt_set_gate(32, (unsigned int)isr32, 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)isr33, 0x08, 0x8E);
    idt_set_gate(44, (unsigned int)isr44, 0x08, 0x8E);
    idt_set_gate(0x80, (unsigned int)isr128, 0x08, 0x8E);

    idt_load((unsigned int)&idtp);
    init_timer(100);
    __asm__ volatile("sti");
}

void pic_init() {
    port_byte_out(0x20, 0x11);
    port_byte_out(0xA0, 0x11);
    port_byte_out(0x21, 0x20);
    port_byte_out(0xA1, 0x28);
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    port_byte_out(0x21, 0xF8);
    port_byte_out(0xA1, 0xEF);
}

void watchdog_reset() {
    watchdog_counter = 0;
}
