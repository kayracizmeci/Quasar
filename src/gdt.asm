bits 64

global gdt_load
global stack_top

section .bss

tss:
    resb 104                        ; x86-64 TSS structure is exactly 104 bytes
    resb 8                          ; padding to keep stack 16-byte aligned

stack_bottom:
    resb 16384                      ; 16 KiB kernel stack

stack_top:

section .data
align 16

gdt:
    dq 0x0000000000000000           ; index 0 — null descriptor (required)
    dq 0x00AF9A000000FFFF           ; index 1 (0x08) — 64-bit ring-0 code:  P=1 DPL=0 L=1 G=1
    dq 0x00CF92000000FFFF           ; index 2 (0x10) — ring-0 data:         P=1 DPL=0 D=1 G=1 RW=1
    dq 0x00CFF2000000FFFF           ; index 3 (0x18) — ring-3 data:         P=1 DPL=3 D=1 G=1 RW=1
    dq 0x00AFFA000000FFFF           ; index 4 (0x20) — 64-bit ring-3 code:  P=1 DPL=3 L=1 G=1

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
    mov rcx, 13                     ; 13 qwords = 104 bytes — zero-fill the entire TSS
    rep stosq

    lea rdi, [rel tss]
    lea rsi, [rel tss_desc]
    mov rcx, rdi

    mov word [rsi + 0], 103         ; TSS limit = 104 - 1 (limit is inclusive)
    mov word [rsi + 2], cx         ; base[15:0]

    shr rcx, 16
    mov byte [rsi + 4], cl         ; base[23:16]
    mov byte [rsi + 5], 0x89       ; access: P=1 DPL=0 S=0 Type=9 (64-bit TSS, available)
    mov byte [rsi + 6], 0x00       ; flags + limit[19:16] all zero (limit fits in 8 bits)
    mov byte [rsi + 7], ch         ; base[31:24]

    mov rax, rdi
    shr rax, 32
    mov dword [rsi + 8], eax
    mov dword [rsi + 12], 0

    lea rax, [rel gdtr]
    lgdt [rax]

    push 0x08                       ; GDT index 1 — kernel code selector for far return
    lea rax, [rel .flush]
    push rax
    retfq

.flush:
    mov ax, 0x10                    ; GDT index 2 — kernel data selector
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    lea rax, [rel stack_top]
    lea rdi, [rel tss]
    mov [rdi + 4], rax              ; TSS.RSP0 — ring-0 stack pointer used on privilege change

    mov ax, 0x30                    ; GDT index 6 — TSS selector (tss_desc at offset 0x30 after alignment)
    ltr ax

    ret
