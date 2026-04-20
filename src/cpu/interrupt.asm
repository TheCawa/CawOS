[bits 32]
extern isr_handler
global isr0
global idt_load

global isr32

global isr_ignore
global isr_ignore
isr_ignore:
    push dword 0  
    push dword 255
    jmp isr_common_stub

global isr13
isr13:
    push dword 13
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

isr_common_stub:
    pusha 
    mov ax, ds
    push eax 
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push eax
    call isr_handler
    add esp, 4
    
    pop eax            
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa             
    add esp, 8         
    iret               