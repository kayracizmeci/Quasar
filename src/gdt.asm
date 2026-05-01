bits 64

global gdt_load
global stack_top

section .bss
align 16

tss:
    resb 104

align 16
stack_bottom:
    resb 16384

stack_top:

section .data
align 16

gdt:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
    dq 0x00AFF2000000FFFF
    dq 0x00AFFA000000FFFF

align 16
tss_desc:
    dq 0
    dq 0

gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dq gdt

section .text

gdt_load:
    cld

    xor rax, rax
    lea rdi, [tss]
    mov rcx, 13
    rep stosq

    mov rax, tss
    mov rcx, rax

    mov word [tss_desc + 0], 103
    mov word [tss_desc + 2], cx

    shr rcx, 16
    mov byte [tss_desc + 4], cl
    mov byte [tss_desc + 5], 0x89
    mov byte [tss_desc + 6], 0x00
    mov byte [tss_desc + 7], ch

    shr rax, 32
    mov dword [tss_desc + 8], eax
    mov dword [tss_desc + 12], 0

    lgdt [gdtr]

    push 0x08
    lea rax, [rel .flush]
    push rax
    retfq

.flush:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rax, stack_top
    mov [tss + 4], rax

    mov ax, 0x30
    ltr ax

    ret
