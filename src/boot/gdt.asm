gdt_start:
    dq 0x0      

gdt_code:       
    dw 0xffff       ; Limit
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)

gdt_data:       
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start + 0x10000

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start