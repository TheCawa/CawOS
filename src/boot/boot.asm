[bits 16]
[org 0x7C00]

%define STACK_TOP       0x7C00
%define BOOTINFO_DRIVE  0x0600

%define STAGE2_SEG      0x0800
%define STAGE2_OFF      0x0000

%define STAGE2_LBA      1
%define STAGE2_SECTORS  4

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP
    sti
    cld

    mov [BOOT_DRIVE], dl
    mov [BOOTINFO_DRIVE], dl

    call check_lba
    jc disk_err

    mov cx, 3

.read_retry:
    push cx

    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, stage2_dap
    int 0x13
    jnc .read_ok

    mov bl, ah

    xor ax, ax
    mov ds, ax
    mov es, ax

    mov [disk_err_code], bl

    mov dl, [BOOT_DRIVE]
    xor ax, ax
    int 0x13

    xor ax, ax
    mov ds, ax
    mov es, ax

    pop cx
    loop .read_retry

    jmp disk_err

.read_ok:
    pop cx

    xor ax, ax
    mov ds, ax
    mov es, ax

    cli
    jmp 0x0000:0x8000

check_lba:
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .lba_error

    cmp bx, 0xAA55
    jne .lba_bad

    test cx, 1
    jz .lba_bad

    xor ax, ax
    mov ds, ax
    mov es, ax

    clc
    ret

.lba_error:
    mov bl, ah

    xor ax, ax
    mov ds, ax
    mov es, ax

    mov [disk_err_code], bl
    stc
    ret

.lba_bad:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov byte [disk_err_code], 0xFF
    stc
    ret

disk_err:
    xor ax, ax
    mov ds, ax
    mov es, ax
    cld

    mov si, msg_err
    call print16

    mov si, msg_code
    call print16

    mov al, [disk_err_code]
    call print_hex8

    mov si, msg_crlf
    call print16

    sti

.halt:
    hlt
    jmp .halt

print16:
    pusha
    mov [print_ptr], si
    cld

.lp:
    xor ax, ax
    mov ds, ax

    mov si, [print_ptr]
    lodsb
    mov [print_ptr], si

    test al, al
    jz .done

    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10

    jmp .lp

.done:
    popa
    ret

print_hex8:
    pusha
    mov [hex_tmp], al

    mov bx, hex_digits
    mov al, [hex_tmp]
    shr al, 4
    xlatb

    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10

    xor ax, ax
    mov ds, ax

    mov bx, hex_digits
    mov al, [hex_tmp]
    and al, 0x0F
    xlatb

    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10

    xor ax, ax
    mov ds, ax

    popa
    ret

stage2_dap:
    db 0x10, 0x00
    dw STAGE2_SECTORS
    dw STAGE2_OFF
    dw STAGE2_SEG
    dq STAGE2_LBA

BOOT_DRIVE:
    db 0

disk_err_code:
    db 0

print_ptr:
    dw 0

hex_tmp:
    db 0

msg_err:
    db "Boot error!", 0

msg_code:
    db " code=0x", 0

msg_crlf:
    db 0x0D, 0x0A, 0

hex_digits:
    db "0123456789ABCDEF"

times 510-($-$$) db 0
dw 0xAA55