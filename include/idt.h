#ifndef IDT_H
#define IDT_H


#include <stdint.h>

struct idt_entry {
    unsigned short base_lo;
    unsigned short sel;      
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_hi;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

void idt_init();
void pic_init();
void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags);
void watchdog_check();
void watchdog_reset();
extern volatile int watchdog_counter;
void idt_reload();
#define KEY_QUEUE_SIZE 16
extern volatile unsigned char key_queue[KEY_QUEUE_SIZE];
extern volatile int key_queue_head;
extern volatile int key_queue_tail;
extern volatile uint32_t system_ticks;

struct registers {
    unsigned int ds;                  
    unsigned int edi, esi, ebp, kernel_esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;    
    unsigned int eip, cs, eflags, useresp, ss; 
};
#endif