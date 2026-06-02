[org 0x8000]
[bits 16]
    mov ax, cs
    mov ds, ax
    mov es, ax
    sti
    mov si, msg_loading
    call print16
    call check_recovery
    call enable_a20
    call init_vbe
    call load_kernel
    call switch_to_pm
    jmp $

check_recovery:
    push es
    xor ax, ax
    mov es, ax
    mov si, msg_recovery_prompt
    call print16
    mov eax, [es:0x046C]
    add eax, 9  ; ~500 мс ожидания (18.2 тика в сек)
    mov edx, eax

.loop:
    mov ah, 01h
    int 16h
    jz .no_key
    mov ah, 00h
    int 16h
    cmp ah, 0x53
    je .pressed

.no_key:
    mov eax, [es:0x046C]
    cmp eax, edx
    jl .loop
    mov byte [is_recovery], 0
    jmp .done

.pressed:
    mov si, msg_recovery_mode
    call print16
    mov byte [is_recovery], 1

.done:
    pop es
    ret

enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

init_vbe:
    pusha
    mov si, vbe_priority_list

.check_mode:
    lodsw
    test ax, ax
    jz .fallback_text
    mov [.current_mode], ax
    push es
    mov ax, 0x9000
    mov es, ax
    xor di, di
    mov ax, 0x4F01
    mov cx, [.current_mode]
    int 0x10
    pop es
    cmp ax, 0x004F
    jne .check_mode
    push es
    mov ax, 0x9000
    mov es, ax
    mov bx, [es:0x00]         
    movzx edx, byte [es:0x19] 
    pop es
    test bx, 0x0001
    jz .check_mode
    test bx, 0x0080
    jz .check_mode
    cmp edx, 16
    jl .check_mode
    mov ax, 0x4F02
    mov bx, [.current_mode]
    or bx, 0x4000
    int 0x10
    cmp ax, 0x004F
    jne .check_mode
    push es
    mov ax, 0x9000
    mov es, ax
    mov eax, [es:0x28]      
    mov [vbe_fb], eax
    movzx eax, word [es:0x10] 
    mov [vbe_pitch], eax
    movzx eax, word [es:0x12] 
    mov [vbe_width], eax
    movzx eax, word [es:0x14] 
    mov [vbe_height], eax
    movzx eax, byte [es:0x19] 
    mov [vbe_bpp], eax
    pop es

    popa
    ret

.fallback_text:
    popa
    mov byte [vbe_fb], 0
    ret

.current_mode: dw 0
vbe_priority_list:
    dw 0x118, 0x119, 0x11A  
    dw 0x115, 0x116, 0x114  
    dw 0x112, 0x111, 0x110  
    dw 0                    
.err:
    mov si, msg_vbe_err
    call print16
    jmp $

load_kernel:
    xor ax, ax
    mov ds, ax
    mov byte [dap], 0x10
    mov byte [dap+1], 0
    mov word [dap+2], 1
    mov word [dap+4], 0x0000
    mov word [dap+6], 0x1000
    
    cmp byte [is_recovery], 1
    je .load_rec
    mov dword [dap+8], KERNEL_LBA
    mov cx, KERNEL_SECTORS
    jmp .lp
.load_rec:
    mov dword [dap+8], RECOVERY_LBA
    mov cx, RECOVERY_SECTORS

.lp:
    push cx
    mov ah, 0x42
    mov dl, [0x0600]
    mov si, dap
    int 0x13
    jc .err
    add word [dap+6], 0x0020
    inc dword [dap+8]
    pop cx
    loop .lp
    ret
.err:
    mov si, msg_disk_err
    call print16
    jmp $

switch_to_pm:
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x08:pm32

[bits 32]
pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x200000
    mov eax, [vbe_fb]
    mov [0x0520], eax
    mov eax, [vbe_pitch]
    mov [0x0524], eax
    mov eax, [vbe_width]
    mov [0x0528], eax
    mov eax, [vbe_height]
    mov [0x052C], eax
    mov eax, [vbe_bpp]
    mov [0x0530], eax
    
    mov esi, 0x10000
    mov edi, 0x100000
    
    cmp byte [is_recovery], 1
    je .size_rec
    mov ecx, KERNEL_SECTORS * 512 / 4
    jmp .do_copy
.size_rec:
    mov ecx, RECOVERY_SECTORS * 512 / 4

.do_copy:
    rep movsd
    call 0x100000
    jmp $

[bits 16]
print16:
    pusha
    mov ah, 0x0E
.l: lodsb
    test al, al
    jz .d
    int 0x10
    jmp .l
.d: popa
    ret

align 4
gdt_start:
    dq 0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00

gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

dap: times 16 db 0
vbe_fb:     dd 0
vbe_pitch:  dd 0
vbe_width:  dd 0
vbe_height: dd 0
vbe_bpp:    dd 0

KERNEL_LBA equ 5

%ifndef KERNEL_SECTORS
KERNEL_SECTORS equ 384
%endif

%ifndef RECOVERY_LBA
RECOVERY_LBA equ 389
%endif

%ifndef RECOVERY_SECTORS
RECOVERY_SECTORS equ 256
%endif

is_recovery db 0

msg_loading         db "Loading CawOS...", 0x0D, 0x0A, 0
msg_recovery_prompt db "Press DEL to enter Recovery Mode...", 0x0D, 0x0A, 0
msg_recovery_mode   db "Loading Recovery Kernel...", 0x0D, 0x0A, 0
msg_vbe_err         db "VBE Error!", 0
msg_disk_err        db "Disk Error!", 0