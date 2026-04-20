[bits 32]
extern main
global _start
global bios_write_sector
global bios_read_sector
global thunk_init

section .text

_start:
    mov esp, 0x7FFFF
    call main
    jmp $

thunk_init:
    push ebp
    mov ebp, esp
    mov al, [ebp + 8]
    mov [boot_drive], al
    pop ebp
    ret

bios_write_sector:
    push ebp
    mov ebp, esp
    mov byte [operation_type], 0x43
    jmp common_bios_io

bios_read_sector:
    push ebp
    mov ebp, esp
    mov byte [operation_type], 0x42
    jmp common_bios_io

common_bios_io:
    sgdt [saved_gdtr] 
    sidt [saved_idtr]
    pushad
    cld

    mov al, [operation_type]
    mov [0x7A10], al

    mov eax, [ebp + 8]      ; LBA
    mov esi, [ebp + 12]     ; Buffer
    cmp byte [operation_type], 0x43
    jne .prepare_dap
    mov edi, 0x7000
    mov ecx, 512
    rep movsb

.prepare_dap:
    mov [dap_lba], eax
    mov dword [dap_lba + 4], 0
    mov word [dap_buffer_off], 0x0000
    mov word [dap_buffer_seg], 0x0700
    mov esi, dap
    mov edi, 0x5000
    mov ecx, 16
    rep movsb

    mov esi, gdt_start2
    mov edi, 0x6000
    mov ecx, gdt_end2 - gdt_start2
    rep movsb

    mov al, [boot_drive]
    mov [0x7A00], al
    mov [0x7A04], esp
    call .get_base
.get_base:
    pop ebx
    sub ebx, (.get_base - _start)
    
    lea esi, [ebx + rm_thunk_start - _start]
    mov edi, 0xA000    
    mov ecx, rm_thunk_end - rm_thunk_start
    rep movsb

    cli
    mov [0x7A04], esp
    lea eax, [ebx + gdt_start2 - _start]
    push eax
    push word (gdt_end2 - gdt_start2 - 1)
    lgdt [esp]
    add esp, 6

    lea eax, [ebx + .pm16 - _start]
    push 0x18
    push eax
    retf

[bits 16]
.pm16:
    mov ax, 0x20    ; 16-bit data selector
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax
    jmp 0x0000:0xA000

rm_thunk_start:
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000

    mov bx, 0x7B08
    mov word [bx], 0x3FF
    mov dword [bx + 2], 0
    lidt [bx]

    sti
    mov dl, [0x7A00]
    test dl, dl
    jnz .drive_ok
    mov dl, 0x80
.drive_ok:
    mov ah, [0x7A10]
    mov si, 0x5000
    int 0x13

    cli
    jc .disk_error
    jmp .back_to_pm

.disk_error:
    ;mov ax, 0x0E45
    ;int 0x10

.back_to_pm:
    cli

    mov bx, 0x7B00
    mov word [bx], gdt_end2 - gdt_start2 - 1
    mov dword [bx + 2], 0x6000
    lgdt [bx]

    mov eax, cr0
    or al, 1
    mov cr0, eax
    db 0x66, 0xEA
    dd .apply_pm32   
    dw 0x08          

[bits 32]
.apply_pm32:
    mov ax, 0x10      ; Data 32 Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [0x7A04]
    jmp pm32_return
rm_thunk_end:

[bits 32]
pm32_return:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    cmp byte [operation_type], 0x42
    jne .done
    mov edi, [ebp + 12]
    mov esi, 0x7000
    mov ecx, 512
    rep movsb

.done:
    lgdt [saved_gdtr]
    lidt [saved_idtr]
    popad
    pop ebp
    sti
    ret


saved_gdtr: dw 0
            dd 0
saved_idtr: dw 0
            dd 0
align 16
gdt_start2:
    dq 0x0
    dq 0x00cf9a000000ffff ; 0x08
    dq 0x00cf92000000ffff ; 0x10
    dq 0x00009a000000ffff ; 0x18
    dq 0x000092000000ffff ; 0x20 data 16
gdt_end2:

boot_drive: db 0
operation_type: db 0 ; 0x42 = Read, 0x43 = Write

align 16
dap:
    db 0x10, 0
    dw 1
dap_buffer_off: dw 0
dap_buffer_seg: dw 0
dap_lba:         dq 0