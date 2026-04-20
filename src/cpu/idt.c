#include "idt.h"
#include "util.h"
#include "screen.h"
#include "io.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void isr0();
extern void idt_load(unsigned int);
extern void isr32();
extern void isr_ignore();
extern void isr13();

volatile int watchdog_counter = 0;
const int WATCHDOG_LIMIT = 5000; 

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
    unsigned char speaker_state = port_byte_in(0x61);
    port_byte_out(0x61, speaker_state & 0xFC);
    disable_cursor();

    char* vm = (char*)0xb8000;
    char hex_buf[11];

    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        vm[i] = ' ';
        vm[i+1] = 0x1F; 
    }

    print_at_color(" [ CawOS System Error ] ", 1, 28, 0x1F);
    print_at_color("A fatal exception has occurred. The system has been halted", 4, 3, 0x1F);
    print_at_color("to prevent damage to your computer.", 5, 3, 0x1F);
    print_at_color("Error Type:", 7, 3, 0x1F);
    print_at_color(error_name, 7, 15, 0x1E);
    print_at_color("---------------------------------------------------------------", 9, 3, 0x1F);
    print_at_color("REGS DUMP:", 10, 3, 0x1F);
    print_at_color("EIP:", 12, 3, 0x1F);
    int_to_hex(r->eip, hex_buf);
    print_at_color(hex_buf, 12, 8, 0x1F);
    print_at_color("CS:", 12, 22, 0x1F);
    int_to_hex(r->cs, hex_buf);
    print_at_color(hex_buf, 12, 26, 0x1F);
    print_at_color("EAX:", 14, 3, 0x1F);
    int_to_hex(r->eax, hex_buf);
    print_at_color(hex_buf, 14, 8, 0x1F);
    print_at_color("EBX:", 14, 22, 0x1F);
    int_to_hex(r->ebx, hex_buf);
    print_at_color(hex_buf, 14, 26, 0x1F);
    print_at_color("ECX:", 15, 3, 0x1F);
    int_to_hex(r->ecx, hex_buf);
    print_at_color(hex_buf, 15, 8, 0x1F);
    print_at_color("EDX:", 15, 22, 0x1F);
    int_to_hex(r->edx, hex_buf);
    print_at_color(hex_buf, 15, 26, 0x1F);
    print_at_color("ESP:", 17, 3, 0x1F);
    int_to_hex(r->kernel_esp, hex_buf);
    print_at_color(hex_buf, 17, 8, 0x1F);
    print_at_color("EBP:", 17, 22, 0x1F);
    int_to_hex(r->ebp, hex_buf);
    print_at_color(hex_buf, 17, 26, 0x1F);
    print_at_color("---------------------------------------------------------------", 19, 3, 0x1F);
    print_at_color("Please restart your computer.", 21, 3, 0x1F);
}

void isr_handler(struct registers *r) { 
    if (r->int_no == 32) {
        port_byte_out(0x20, 0x20);
        return;
    } 
    if (r->int_no == 255) {
        if (r->int_no >= 32) port_byte_out(0x20, 0x20);
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
    char dbg[16];
    itoa((uint32_t)&idt, dbg);
    print_at("IDT:", 0, 0);
    print_at(dbg, 0, 5);
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
    port_byte_out(0x21, 0xFE); 
    port_byte_out(0xA1, 0xFF);

    memset(&idt, 0, sizeof(struct idt_entry) * 256);

    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned int)isr_ignore, 0x08, 0x8E);
    }

    idt_set_gate(0, (unsigned int)isr0, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E); 
    idt_set_gate(32, (unsigned int)isr32, 0x08, 0x8E);

    idt_load((unsigned int)&idtp);
    init_timer(100);
}

void watchdog_reset() {
    watchdog_counter = 0;
}