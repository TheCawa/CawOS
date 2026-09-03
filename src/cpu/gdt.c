#include "kernel/gdt.h"
#include "libc/util.h"

#define GDT_ENTRIES 8

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  limit_high_flags;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_t       tss;

extern uint32_t syscall_kstack_top;
extern uint32_t kernel_stack_top;

static void set_gdt_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags) {
    gdt[index].limit_low        = limit & 0xFFFF;
    gdt[index].base_low         = base & 0xFFFF;
    gdt[index].base_mid         = (base >> 16) & 0xFF;
    gdt[index].access           = access;
    gdt[index].limit_high_flags = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[index].base_high        = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    /* 0x00: null descriptor */
    set_gdt_entry(0, 0, 0, 0, 0);
    /* 0x08: kernel code, ring 0, flat 4 GB */
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    /* 0x10: kernel data, ring 0, flat 4 GB */
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    /* 0x18: user code, ring 3, flat 4 GB (selector 0x1B) */
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    /* 0x20: user data, ring 3, flat 4 GB (selector 0x23) */
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    /* 0x28: TSS descriptor */
    set_gdt_entry(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x40);

    tss.ss0  = KERNEL_DATA_SEG;
    tss.esp0 = (uint32_t)&kernel_stack_top;

    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
    __asm__ volatile("ltr %w0" : : "r"((uint16_t)TSS_SEG));
}

void tss_set_esp0(uint32_t esp0) {
    tss.esp0 = esp0;
}
