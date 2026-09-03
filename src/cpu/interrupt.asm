[bits 32]
extern isr_handler
extern exit_recovery_esp
extern current_process
global isr0
global idt_load
global isr13
global isr14
global isr32
global isr33
global isr44
global isr128
global isr_ignore
global enter_user_mode
global syscall_kstack_top
global kernel_stack_top
global context_switch

; --- Стек ядра для обработки системных вызовов ---
section .bss
align 16
syscall_kstack:      resb 16384          ; 16 КБ
syscall_kstack_top:                      ; ESP будет указывать сюда (стек растёт вниз)
kernel_stack:        resb 32768          ; 32 КБ стека ring 0 (TSS ESP0)
kernel_stack_top:
saved_user_esp:      resd 1
section .text

extern exit_recovery_esp
SYS_EXIT equ 1
USER_CODE_SELECTOR equ 0x1B
USER_DATA_SELECTOR equ 0x23

isr_ignore:
    push dword 0
    push dword 255
    jmp isr_common_stub

isr13:
    push dword 13
    jmp isr_common_stub

isr14:
    push dword 14
    jmp isr_common_stub


isr44:
    push dword 0
    push dword 44
    jmp isr_common_stub

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

isr0:
    push byte 0
    push byte 0
    jmp isr_common_stub

isr32:
    push byte 0
    push byte 32
    jmp isr_common_stub

isr33:
    push dword 0
    push dword 33
    jmp isr_common_stub

isr128:
    cmp eax, SYS_EXIT
    jne .handle_other
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, [exit_recovery_esp]
    pop ebp
    sti
    ret

.handle_other:
    push dword 0          ; Error code
    push dword 0x80       ; Interrupt number
    pusha                 ; General purpose registers
    cld
    mov ax, ds
    push eax              ; Save DS

    mov ax, 0x10          ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    push esp
    call isr_handler
    add esp, 4

    pop eax               ; Restore DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

isr_common_stub:
    pusha
    cld
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp              ; pointer to struct registers
    call isr_handler
    add esp, 4

    pop eax               ; restore data segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8            ; discard error_code and int_no
    iret

enter_user_mode:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]      ; entry point
    mov edx, [ebp + 12]     ; stack_top
    mov [exit_recovery_esp], ebp
    push dword USER_DATA_SELECTOR   ; SS
    push edx                        ; ESP
    pushf
    pop ecx
    or ecx, 0x200                   ; Enable interrupts in user mode.
    push ecx                        ; EFLAGS
    push dword USER_CODE_SELECTOR   ; CS
    push eax                        ; EIP
    mov ax, USER_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iret

context_switch:
    push ebp
    push ebx
    push esi
    push edi
    pushfd
    mov eax, [esp + 24]
    mov edx, [esp + 28]

    mov [eax], esp          ; prev->saved_esp = current esp (points at pushed eflags)
    mov ebx, [esp]          ; current eflags
    mov [eax + 4], ebx      ; prev->saved_eflags

    mov ebx, [edx + 4]      ; next->saved_eflags
    mov ecx, [edx]          ; next->saved_esp
    mov [ecx], ebx          ; place eflags on top of next's stack
    mov esp, ecx            ; switch to next's stack

    popfd
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
