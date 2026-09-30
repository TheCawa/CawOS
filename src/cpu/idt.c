#include "kernel/idt.h"
#include "libc/util.h"
#include "drivers/screen.h"
#include "drivers/serial.h"
#include "drivers/io.h"
#include "kernel/interrupt.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
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
const int WATCHDOG_LIMIT = 500;
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

#define MAX_STACK_FRAMES 8

static int is_valid_frame_ptr(uint32_t addr) {
    if (addr < 0x100000 || (addr & 3)) return 0;
    uint32_t max_ram = ((uint32_t)get_total_memory()) * 1024 * 1024;
    if (addr + 8 > max_ram) return 0;
    return 1;
}

static void bsod_serial_dump(const char* error_name, struct registers *r) {
    serial_puts(SERIAL_COM1, "[BSOD] ");
    serial_puts(SERIAL_COM1, error_name);
    serial_puts(SERIAL_COM1, "\r\n");

    char hex_buf[12];
    serial_puts(SERIAL_COM1, "EIP: "); int_to_hex(r->eip, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "CS:  "); int_to_hex(r->cs, hex_buf);  serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "EAX: "); int_to_hex(r->eax, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "EBX: "); int_to_hex(r->ebx, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "ECX: "); int_to_hex(r->ecx, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "EDX: "); int_to_hex(r->edx, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "ESP: "); int_to_hex(r->kernel_esp, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    serial_puts(SERIAL_COM1, "EBP: "); int_to_hex(r->ebp, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");

    if (r->int_no == 14) {
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_puts(SERIAL_COM1, "CR2: "); int_to_hex(cr2, hex_buf); serial_puts(SERIAL_COM1, hex_buf); serial_puts(SERIAL_COM1, "\r\n");
    }

    serial_puts(SERIAL_COM1, "Stack trace:\r\n");
    uint32_t ebp = r->ebp;
    for (int i = 0; i < MAX_STACK_FRAMES && is_valid_frame_ptr(ebp); i++) {
        uint32_t* frame = (uint32_t*)ebp;
        serial_puts(SERIAL_COM1, "Frame ");
        char num_buf[8];
        itoa(i, num_buf);
        serial_puts(SERIAL_COM1, num_buf);
        serial_puts(SERIAL_COM1, ": ");
        int_to_hex(frame[1], hex_buf);
        serial_puts(SERIAL_COM1, hex_buf);
        serial_puts(SERIAL_COM1, "\r\n");
        ebp = frame[0];
    }
}

static void print_stack_frame_to_screen(int row, int idx, uint32_t eip) {
    char line[64];
    char hex_buf[12];
    char num_buf[8];
    strcpy(line, "Frame ");
    itoa(idx, num_buf);
    strcat(line, num_buf);
    strcat(line, ": ");
    int_to_hex(eip, hex_buf);
    strcat(line, hex_buf);
    print_at_color(line, row, 3, 0x1F);
}

static const char* bsod_exception_name(int no) {
    switch (no) {
        case 0:  return "DIVIDE BY ZERO (#DE)";
        case 1:  return "DEBUG (#DB)";
        case 2:  return "NON-MASKABLE INTERRUPT";
        case 3:  return "BREAKPOINT (#BP)";
        case 4:  return "OVERFLOW (#OF)";
        case 5:  return "BOUND RANGE EXCEEDED (#BR)";
        case 6:  return "INVALID OPCODE (#UD)";
        case 7:  return "DEVICE NOT AVAILABLE (#NM)";
        case 8:  return "DOUBLE FAULT (#DF)";
        case 9:  return "COPROCESSOR SEGMENT OVERRUN";
        case 10: return "INVALID TSS (#TS)";
        case 11: return "SEGMENT NOT PRESENT (#NP)";
        case 12: return "STACK-SEGMENT FAULT (#SS)";
        case 13: return "GENERAL PROTECTION FAULT (#GP)";
        case 14: return "PAGE FAULT (#PF)";
        case 15: return "RESERVED EXCEPTION (15)";
        case 16: return "x87 FPU ERROR (#MF)";
        case 17: return "ALIGNMENT CHECK (#AC)";
        case 18: return "MACHINE CHECK (#MC)";
        case 19: return "SIMD FLOAT-POINT EXCEPTION (#XM)";
        case 20: return "VIRTUALIZATION EXCEPTION (#VE)";
        case 21: return "CONTROL PROTECTION EXCEPTION (#CP)";
        case 30: return "SECURITY EXCEPTION (#SX)";
        default: return "UNKNOWN EXCEPTION";
    }
}

static int bsod_has_error_code(int no) {
    return no == 8 || no == 10 || no == 11 || no == 12 || no == 13 ||
           no == 14 || no == 17 || no == 21 || no == 30;
}

void draw_bsod(const char* error_name, struct registers *r) {
    char hex_buf[11];
    int is_gfx = g_is_graphics;
    int cols = is_gfx ? screen_get_cols() : 80;
    int rows = is_gfx ? screen_get_rows() : 25;
    unsigned char speaker_state = port_byte_in(0x61);
    port_byte_out(0x61, speaker_state & 0xFC);
    disable_cursor();
    if (!is_gfx) {
        char* vm = (char*)0xb8000;
        for (int i = 0; i < 80 * 25 * 2; i += 2) {
            vm[i] = ' '; vm[i + 1] = 0x1F;
        }
    } else {
        screen_set_font_scale(3, 2);
        cols = screen_get_cols();
        rows = screen_get_rows();
        uint32_t blue = 0x000000AA;
        for (uint32_t y = 0; y < g_height; y++) {
            uint8_t* rowpx = g_shadow + y * g_pitch;
            for (uint32_t x = 0; x < g_width; x++) {
                uint8_t* pixel = rowpx + x * (g_bpp / 8);
                uint8_t rr = (blue >> 16) & 0xFF;
                uint8_t gg = (blue >> 8)  & 0xFF;
                uint8_t bb = (blue)       & 0xFF;
                if (g_bpp == 32) {
                    pixel[0] = bb; pixel[1] = gg; pixel[2] = rr; pixel[3] = 0;
                } else if (g_bpp == 24) {
                    pixel[0] = bb; pixel[1] = gg; pixel[2] = rr;
                } else if (g_bpp == 16) {
                    uint16_t c16 = ((rr >> 3) << 11) | ((gg >> 2) << 5) | (bb >> 3);
                    pixel[0] = c16 & 0xFF;
                    pixel[1] = c16 >> 8;
                }
            }
        }
        memcpy(g_framebuffer, g_shadow, g_height * g_pitch);
    }
    int row = 1;
    int mid = cols / 2 - 12;
    if (mid < 0) mid = 0;
    #define BPRINT(text, col, color) do { \
        if (row < rows) print_at_color((text), row, (col), (color)); \
    } while (0)
    #define BNEXT() do { row++; } while (0)
    BPRINT(" [ CawOS System Error ] ", mid, 0x1F); BNEXT(); BNEXT();
    BPRINT("A fatal exception has occurred. The system has been halted", 3, 0x1F); BNEXT();
    BPRINT("to prevent damage to your computer.", 3, 0x1F); BNEXT(); BNEXT();
    BPRINT("Error Type:", 3, 0x1F);
    BPRINT(error_name, 15, 0x1E); BNEXT();
    BPRINT("-----------------------------------------------", 3, 0x1F); BNEXT();
    BPRINT("REGS DUMP:", 3, 0x1F); BNEXT();
    BPRINT("EIP:", 3, 0x1F); int_to_hex(r->eip, hex_buf); BPRINT(hex_buf, 8, 0x1F);
    BPRINT("CS:", 22, 0x1F); int_to_hex(r->cs, hex_buf);  BPRINT(hex_buf, 26, 0x1F); BNEXT();
    BPRINT("EAX:", 3, 0x1F); int_to_hex(r->eax, hex_buf); BPRINT(hex_buf, 8, 0x1F);
    BPRINT("EBX:", 22, 0x1F); int_to_hex(r->ebx, hex_buf); BPRINT(hex_buf, 26, 0x1F); BNEXT();
    BPRINT("ECX:", 3, 0x1F); int_to_hex(r->ecx, hex_buf); BPRINT(hex_buf, 8, 0x1F);
    BPRINT("EDX:", 22, 0x1F); int_to_hex(r->edx, hex_buf); BPRINT(hex_buf, 26, 0x1F); BNEXT();
    BPRINT("ESP:", 3, 0x1F); int_to_hex(r->kernel_esp, hex_buf); BPRINT(hex_buf, 8, 0x1F);
    BPRINT("EBP:", 22, 0x1F); int_to_hex(r->ebp, hex_buf); BPRINT(hex_buf, 26, 0x1F); BNEXT();
    if (bsod_has_error_code(r->int_no) || r->int_no == 14) {
        if (bsod_has_error_code(r->int_no)) {
            BPRINT("ERR CODE:", 3, 0x1F);
            int_to_hex(r->err_code, hex_buf); BPRINT(hex_buf, 13, 0x1F);
        }
        if (r->int_no == 14) {
            uint32_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            BPRINT("CR2:", 22, 0x1F);
            int_to_hex(cr2, hex_buf); BPRINT(hex_buf, 27, 0x1F);
        }
        BNEXT();
    }
    BPRINT("-----------------------------------------------", 3, 0x1F); BNEXT();
    BPRINT("Stack trace:", 3, 0x1F); BNEXT();
    int trace_start = row;
    int reserve = 3;
    int max_frames = rows - trace_start - reserve;
    if (max_frames > MAX_STACK_FRAMES) max_frames = MAX_STACK_FRAMES;
    if (max_frames < 0) max_frames = 0;
    uint32_t ebp = r->ebp;
    int shown = 0;
    for (int i = 0; i < max_frames && is_valid_frame_ptr(ebp); i++) {
        uint32_t* frame = (uint32_t*)ebp;
        print_stack_frame_to_screen(trace_start + i, i, frame[1]);
        ebp = frame[0];
        shown++;
    }
    row = trace_start + shown;
    BPRINT("-----------------------------------------------", 3, 0x1F); BNEXT(); BNEXT();
    if (row > rows - 1) row = rows - 1;
    BPRINT("Please restart your computer.", 3, 0x1F);
    #undef BPRINT
    #undef BNEXT
    bsod_serial_dump(error_name, r);
}

__attribute__((force_align_arg_pointer))
void isr_handler(struct registers *r) {
    if (r->int_no == 32) {
        system_ticks++;
        watchdog_counter++;
        if (watchdog_counter >= WATCHDOG_LIMIT) {
            draw_bsod("WATCHDOG TIMEOUT", r);
            __asm__ volatile("cli; hlt");
        }
        port_byte_out(0x20, 0x20);
        scheduler_tick();
        return;
    }
    if (r->int_no == 44) {
        for (int i = 0; i < 256; i++) {
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
        for (int i = 0; i < 256; i++) {
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
                if (current_process) {
                    current_process->state = PROCESS_ZOMBIE;
                    schedule();
                }
                break;

            case 11: // sys_yield
                scheduler_yield();
                break;

            case 12: // sys_getpid
                {
                    process_t* p = process_get_current();
                    r->eax = p ? p->pid : 0;
                }
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
        port_byte_out(0xA0, 0x20);
        port_byte_out(0x20, 0x20);
        return;
    }
    draw_bsod(bsod_exception_name(r->int_no), r);
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
    idt_set_gate(0x80, (unsigned int)isr128, 0x08, 0xEE);

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
