[org 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [BOOT_DRIVE], dl
    mov [0x0600], dl 
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, stage2_dap
    int 0x13
    jc .disk_err

    jmp 0x0000:0x8000

.disk_err:
    mov si, msg_err
    call print16
    jmp $

print16:
    pusha
    mov ah, 0x0E
.lp: lodsb
    test al, al
    jz .done
    int 0x10
    jmp .lp
.done:
    popa
    ret

stage2_dap:
    db 0x10, 0x00
    dw 4
    dw 0x0000
    dw 0x0800
    dq STAGE2_LBA

BOOT_DRIVE db 0
msg_err    db "Boot error!", 0
STAGE2_LBA equ 1

times 510-($-$$) db 0
dw 0xAA55