[org 0x7c00]
KERNEL_OFFSET equ 0x10000

jmp start
nop
times 33 db 0 

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [BOOT_DRIVE], dl

    mov ax, 0x0003
    int 0x10

    mov si, MSG_BOOTING
    call print_string_16

    call enable_a20
    
    mov si, MSG_LOAD_KERNEL
    call print_string_16
    call load_kernel

    mov si, MSG_SWITCH_PM
    call print_string_16
    
    mov ax, 0x0003
    int 0x10

    call switch_to_pm
    jmp $

[bits 16]
print_string_16:
    pusha
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

load_kernel:
    mov byte [kern_dap + 0], 0x10
    mov byte [kern_dap + 1], 0x00
    mov word [kern_dap + 2], 1
    mov word [kern_dap + 4], 0x0000
    mov word [kern_dap + 6], 0x1000
    mov dword [kern_dap + 8], 1
    mov dword [kern_dap + 12], 0
    mov cx, KERNEL_SECTORS

.loop:
    push cx
    xor ax, ax
    mov ds, ax
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, kern_dap
    int 0x13
    jc disk_error
    add word [kern_dap + 6], 0x0020
    inc dword [kern_dap + 8]
    pop cx
    loop .loop
    ret

disk_error:
    mov si, MSG_DISK_ERROR
    call print_string_16
    jmp $


switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:init_pm_32 

[bits 32]
init_pm_32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax 
    mov ebp, 0x200000 
    mov esp, ebp
    
    call KERNEL_OFFSET
    jmp $

align 4
gdt_start:
    dq 0x0
gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
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
    dd gdt_start

kern_dap: times 16 db 0

BOOT_DRIVE          db 0
MSG_BOOTING         db "CawOS Booting...", 0x0D, 0x0A, 0
MSG_LOAD_KERNEL     db " Loading Kernel...", 0x0D, 0x0A, 0
MSG_SWITCH_PM       db " Jumping to Protected Mode...", 0
MSG_DISK_ERROR      db "FATAL: Disk error!", 0

times 510-($-$$) db 0
dw 0xAA55