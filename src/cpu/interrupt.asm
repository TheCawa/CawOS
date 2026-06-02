[bits 32]
extern isr_handler
extern exit_recovery_esp
global isr0
global idt_load
global isr13
global isr14
global isr32
global isr33
global isr44
global isr128
global isr_ignore
global run_program_asm

; --- Стек ядра для обработки системных вызовов ---
section .bss
align 16
syscall_kstack:      resb 16384          ; 16 КБ — достаточно для scroll+memmove+memcpy
syscall_kstack_top:                      ; ESP будет указывать сюда (стек растёт вниз)
saved_user_esp:      resd 1 
section .text

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

    mov [saved_user_esp], esp   ; Сохраняем указатель на struct registers
    mov esp, syscall_kstack_top ; Переключаемся на стек ядра
    
    and esp, -16
    sub esp, 12
    
    sti                         ; <--- ДОБАВИТЬ СЮДА: разрешаем прерывания во время тяжелого syscall

    push dword [saved_user_esp] 
    call isr_handler
    add esp, 16

    cli                   ; Запрещаем прерывания перед возвратом в user-space (уже есть)
    
    mov esp, [saved_user_esp]   ; Восстанавливаем user stack

    pop eax               ; Restore DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

; -------------------------------------------------------
; Общий стаб для всех остальных прерываний
; -------------------------------------------------------
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

    ; --- ИСПРАВЛЕНИЕ ДЛЯ ВЛОЖЕННЫХ ПРЕРЫВАНИЙ И ВЫРАВНИВАНИЯ ESP ---
    mov ecx, esp          ; Временный указатель на struct registers
    and esp, -16          ; Динамически выравниваем ESP по границе 16 байт
    sub esp, 8            ; Резервируем 8 байт отступа (для сохранения выравнивания)
    push ecx              ; Сохраняем оригинальный ESP на стек [будет лежать в esp + 4]
    push ecx              ; Пушим указатель на struct registers как аргумент Си-функции [в esp]
    
    call isr_handler
    
    mov esp, [esp + 4]    ; Восстанавливаем оригинальный ESP прямо со стека!
    ; --------------------------------------------------------

    pop eax               ; Восстанавливаем сегменты данных
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8            ; Очищаем error_code и int_no
    iret

run_program_asm:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]      ; entry point
    mov edx, [ebp + 12]     ; stack_top
    mov [exit_recovery_esp], esp
    mov esp, edx
    sti
    call eax
    cli
    mov esp, [exit_recovery_esp]
    pop ebp
    ret