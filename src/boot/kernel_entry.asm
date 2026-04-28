[bits 32]
extern main
global _start
global bios_write_sector
global bios_read_sector
global thunk_init
global bios_get_mem
global fpu_init

section .text

_start:
    mov esp, 0x7FFFF
    extern __bss_start
    extern __bss_end
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
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

bios_get_mem:
    push ebp
    mov ebp, esp
    mov byte [operation_type], 0x15
    jmp common_bios_io


fpu_init:
    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x2
    mov cr0, eax
    mov eax, cr4
    or ax, 0x600
    mov cr4, eax
    fninit
    ret
    
common_bios_io:
    sgdt [saved_gdtr] 
    sidt [saved_idtr]
    pushad
    cld

    mov al, [operation_type]
    mov [0x9A10], al  

    mov eax, [ebp + 8]      ; LBA
    mov esi, [ebp + 12]     ; Buffer
    cmp byte [operation_type], 0x43
    jne .prepare_dap
    mov edi, 0x9D00 
    mov ecx, 512
    rep movsb

.prepare_dap:
    mov [dap_lba], eax
    mov dword [dap_lba + 4], 0
    mov word [dap_buffer_off], 0x0000
    mov word [dap_buffer_seg], 0x09D0
    mov esi, dap
    mov edi, 0x9B00   
    mov ecx, 16
    rep movsb

    mov esi, gdt_start2
    mov edi, 0x9C00
    mov ecx, gdt_end2 - gdt_start2
    rep movsb
    mov word [0x9A18], gdt_end2 - gdt_start2 - 1
    mov dword [0x9A1A], 0x9C00
    mov al, [boot_drive]
    mov [0x9A00], al 
    mov [0x9A04], esp 
    call .get_base
.get_base:
    pop ebx
    sub ebx, (.get_base - _start)
    lea esi, [ebx + rm_thunk_start - _start]
    mov edi, 0xA000    
    mov ecx, rm_thunk_end - rm_thunk_start
    rep movsb
    cli
    mov [0x9A04], esp
    lgdt [0x9A18]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    db 0xEA
    dd 0x0000A000
    dw 0x18

[bits 16]

[bits 16]
rm_thunk_start:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax
    db 0xEA
    dw (rm_real_start - rm_thunk_start + 0xA000)
    dw 0x0000

rm_real_start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x8000
    mov bx, 0x7B08
    mov word [bx], 0x3FF
    mov dword [bx + 2], 0
    lidt [bx]
    sti
    mov al, [0x9A10]
    cmp al, 0x15
    je .do_mem_check
    mov dl, [0x9A00]
    test dl, dl
    jnz .drive_ok
    mov dl, 0x80
.drive_ok:
    mov ah, [0x9A10]
    mov si, 0x9B00
    int 0x13
    cli
    jc .disk_error
    jmp .back_to_pm

.do_mem_check:
    xor ax, ax
    xor bx, bx
    xor cx, cx
    xor dx, dx
    mov ax, 0xE801
    int 0x15
    cli
    jc .disk_error
    
    test ax, ax
    jnz .use_axbx
    mov ax, cx
    mov bx, dx
.use_axbx:
    mov [0x9D00], ax
    mov [0x9D02], bx
    jmp .back_to_pm

.disk_error:
    jmp .back_to_pm

.back_to_pm:
    cli
    mov bx, 0x7B00
    mov word [bx], gdt_end2 - gdt_start2 - 1
    mov dword [bx + 2], 0x9C00
    lgdt [bx]

    mov eax, cr0
    or al, 1
    mov cr0, eax
    db 0x66, 0xEA
    dd .apply_pm32   
    dw 0x08          

[bits 32]
.apply_pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [0x9A04]
    jmp pm32_return
rm_thunk_end:

[bits 32]
pm32_return:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov al, [0x9A10]
    cmp al, 0x15
    je .process_mem
    cmp al, 0x42
    jne .done
    mov edi, [ebp + 12]
    mov esi, 0x9D00
    mov ecx, 512
    rep movsb
    jmp .done

.process_mem:
    movzx ebx, word [0x9D02]
    shl ebx, 6
    movzx eax, word [0x9D00]
    add eax, ebx
    add eax, 1024
    shr eax, 10
    mov [0x9D04], eax
    
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
    dq 0x0                        ; Null
    dq 0x00cf9a000000ffff         ; 0x08: 32-bit Code (Flat)
    dq 0x00cf92000000ffff         ; 0x10: 32-bit Data (Flat)
    dq 0x00009a000000ffff         ; 0x18: 16-bit Code (64KB limit)
    dq 0x000092000000ffff         ; 0x20: 16-bit Data (64KB limit)
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