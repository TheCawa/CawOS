ENTRY(_start)

SECTIONS
{
    . = 0x10000; 

    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data) }
    .bss : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }

    .cmd : ALIGN(16) {
        __start_cmd = .; 
        KEEP(*(.cmd))   
        __stop_cmd = .;
    }

    /DISCARD/ : { *(.comment) *(.eh_frame) }
}