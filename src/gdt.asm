bits 64

global gdt_load
global stack_top

section .bss

tss:
    resb 104
    resb 8          ; explicit padding: 104+8=112 (7x16), stack_bottom stays 16-byte aligned

stack_bottom:
    resb 16384

stack_top:

section .data
align 16

gdt:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00CFF2000000FFFF
    dq 0x00AFFA000000FFFF

align 16
tss_desc:
    dq 0
    dq 0

gdt_end:

[WARNING -reloc-abs-qword]
gdtr:
    dw gdt_end - gdt - 1
    dq gdt
[WARNING +reloc-abs-qword]

section .text

[WARNING -reloc-rel-dword]

gdt_load:
    cld

    lea rdi, [rel tss]
    xor rax, rax
    mov rcx, 13
    rep stosq

    lea rdi, [rel tss]
    lea rsi, [rel tss_desc]
    mov rcx, rdi

    mov word [rsi + 0], 103
    mov word [rsi + 2], cx

    shr rcx, 16
    mov byte [rsi + 4], cl
    mov byte [rsi + 5], 0x89
    mov byte [rsi + 6], 0x00
    mov byte [rsi + 7], ch

    mov rax, rdi
    shr rax, 32
    mov dword [rsi + 8], eax
    mov dword [rsi + 12], 0

    lea rax, [rel gdtr]
    lgdt [rax]

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

    lea rax, [rel stack_top]
    lea rdi, [rel tss]
    mov [rdi + 4], rax

    mov ax, 0x30
    ltr ax

    ret
