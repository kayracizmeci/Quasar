bits 64

section .note.GNU-stack noalloc noexec nowrite progbits

global gdt_load
global gdt_load_ap
global tss_descs

section .bss

tss:
    resb 104                        ; x86-64 TSS structure is exactly 104 bytes
    resb 8                          ; padding to keep stack 16-byte aligned

stack_bottom:
    resb 16384                      ; 16 KiB kernel stack

stack_top:

df_stack_bottom:
    resb 4096                       ; 4 KiB double-fault stack (IST1)
df_stack_top:

nmi_stack_bottom:
    resb 4096                       ; 4 KiB NMI stack (IST2)
nmi_stack_top:

section .data
align 16

gdt:
    dq 0x0000000000000000           ; index 0 — null descriptor (required)
    dq 0x00AF9A000000FFFF           ; index 1 (0x08) — 64-bit ring-0 code:  P=1 DPL=0 L=1 G=1
    dq 0x00CF92000000FFFF           ; index 2 (0x10) — ring-0 data:         P=1 DPL=0 D=1 G=1 RW=1
    dq 0x00CFF2000000FFFF           ; index 3 (0x18) — ring-3 data:         P=1 DPL=3 D=1 G=1 RW=1
    dq 0x00AFFA000000FFFF           ; index 4 (0x20) — 64-bit ring-3 code:  P=1 DPL=3 L=1 G=1

align 16
tss_descs:
    times 128 dq 0     ; 64 TSS descriptors × 16 bytes each; CPU n uses selector 0x30 + n*0x10

gdt_end:

[WARNING -reloc-abs-qword]
gdtr:
    dw gdt_end - gdt - 1
    dq gdt
[WARNING +reloc-abs-qword]

section .text

; gdt_load uses lea [rel <symbol>] to reference .data/.bss from .text, which
; produces 32-bit section-crossing relocations.  For a position-dependent
; kernel whose image fits within 2 GB this is always safe; suppress the noise.
[WARNING -reloc-rel-dword]
gdt_load:
    cld

    lea rdi, [rel tss]
    xor rax, rax
    mov rcx, 13                     ; 13 qwords = 104 bytes — zero-fill the entire TSS
    rep stosq

    lea rdi, [rel tss]
    lea rsi, [rel tss_descs]
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

    lea rax, [rel df_stack_top]
    mov [rdi + 36], rax             ; TSS.IST1 — double fault stack
    lea rax, [rel nmi_stack_top]
    mov [rdi + 44], rax             ; TSS.IST2 — NMI stack

    mov ax, 0x30                    ; GDT index 6 — TSS selector (tss_desc at offset 0x30 after alignment)
    ltr ax

    ret

; GDT reload for application processors.  The GDT itself was
; already built by the BSP's gdt_load(); APs just need to switch their
; segment registers to our descriptors.  TSS loading is intentionally skipped
; until each AP has its own TSS with a valid IST stack.
gdt_load_ap:
    lea rax, [rel gdtr]
    lgdt [rax]

    push qword 0x08              ; kernel code selector — needed for far return
    lea rax, [rel .ap_cs_flush]
    push rax
    retfq

.ap_cs_flush:
    mov ax, 0x10                 ; kernel data selector
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax                   ; null FS/GS — GS base is set later via MSR
    mov fs, ax
    mov gs, ax
    ret
[WARNING +reloc-rel-dword]
