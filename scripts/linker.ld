ENTRY(_start)

SECTIONS
{
    . = 0x1000;

    .text   : { *(.text) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data) }
    .bss    : { *(.bss) *(COMMON) }

    .cmd : ALIGN(4) {
        __start_cmd = .; 
        KEEP(*(.cmd))   
        __stop_cmd = .;
    }

    /DISCARD/ : { *(.comment) *(.eh_frame) }
}