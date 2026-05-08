; Interrupt Service Routine stubs and common dispatch path.
;
; Every exception follows this path:
;   1. CPU pushes SS, RSP, RFLAGS, CS, RIP onto the stack.
;      Some exceptions also push an error code; the others get a dummy
;      zero so the C handler always sees the same stack layout.
;   2. The stub pushes the vector number and jumps to isr_common.
;   3. isr_common saves every GP register and calls isr_handler(frame).
;   4. On return, registers are restored and iretq resumes execution.

bits 64

section .note.GNU-stack noalloc noexec nowrite progbits

section .text

global idt_flush
extern isr_handler
extern irq_dispatch

idt_flush:
    lidt [rdi]
    ret

; Exceptions that do NOT push an error code: push 0 as a placeholder.
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

; Exceptions that DO push an error code: the CPU already put it there.
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

; Hardware IRQ stubs: vectors 32-55 cover a full I/O APIC (24 lines) plus
; vector 255 for the APIC spurious interrupt.  Hardware never pushes an error
; code, so every stub pushes 0 to keep the frame layout identical to exceptions.
%macro IRQ_STUB 1
global irq%1
irq%1:
    push qword 0
    push qword %1
    jmp irq_common
%endmacro

IRQ_STUB 32
IRQ_STUB 33
IRQ_STUB 34
IRQ_STUB 35
IRQ_STUB 36
IRQ_STUB 37
IRQ_STUB 38
IRQ_STUB 39
IRQ_STUB 40
IRQ_STUB 41
IRQ_STUB 42
IRQ_STUB 43
IRQ_STUB 44
IRQ_STUB 45
IRQ_STUB 46
IRQ_STUB 47
IRQ_STUB 48
IRQ_STUB 49
IRQ_STUB 50
IRQ_STUB 51
IRQ_STUB 52
IRQ_STUB 53
IRQ_STUB 54
IRQ_STUB 55
IRQ_STUB 255

irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
[WARNING -reloc-rel-dword]
    call irq_dispatch
[WARNING +reloc-rel-dword]

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16     ; skip vector number and dummy error_code
    iretq

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp    ; rdi = pointer to the InterruptFrame struct on the stack
[WARNING -reloc-rel-dword]
    call isr_handler
[WARNING +reloc-rel-dword]

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16     ; skip vector number and error_code pushed by the stub
    iretq
