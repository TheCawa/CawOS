#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* Segment selectors (index << 3 | RPL) */
#define KERNEL_CODE_SEG 0x08
#define KERNEL_DATA_SEG 0x10
#define USER_CODE_SEG   0x18
#define USER_DATA_SEG   0x20
#define TSS_SEG         0x28

#define USER_CODE_SELECTOR (USER_CODE_SEG | 0x03)
#define USER_DATA_SELECTOR (USER_DATA_SEG | 0x03)

/* 32-bit TSS layout (only ESP0/SS0 are used for ring-3 -> ring-0 transitions) */
typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

void gdt_init(void);
void tss_set_esp0(uint32_t esp0);

#endif
