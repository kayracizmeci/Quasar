#pragma once

/*
 * x86-64 CPU exception vector assignments (Intel SDM Vol. 3A §6.3.1)
 *
 * Each constant names the IDT gate index that the CPU uses for that
 * exception.  When idt_init() calls idt_set_gate(EXC_*, isr*, …),
 * the CPU will invoke the matching stub on that fault.
 *
 * Entries marked ERR receive an error code from the CPU; the rest get
 * a dummy zero pushed by the stub so every handler sees the same frame.
 */

#define EXC_DE    0   /* #DE  Division Error                           no  err */
#define EXC_DB    1   /* #DB  Debug                                    no  err */
#define EXC_NMI   2   /*      Non-Maskable Interrupt                   no  err */
#define EXC_BP    3   /* #BP  Breakpoint                               no  err */
#define EXC_OF    4   /* #OF  Overflow                                 no  err */
#define EXC_BR    5   /* #BR  Bound Range Exceeded                     no  err */
#define EXC_UD    6   /* #UD  Invalid Opcode                           no  err */
#define EXC_NM    7   /* #NM  Device Not Available                     no  err */
#define EXC_DF    8   /* #DF  Double Fault                             err (0) */
#define EXC_CSO   9   /*      Coprocessor Segment Overrun (legacy)     no  err */
#define EXC_TS   10   /* #TS  Invalid TSS                              err     */
#define EXC_NP   11   /* #NP  Segment Not Present                      err     */
#define EXC_SS   12   /* #SS  Stack-Segment Fault                      err     */
#define EXC_GP   13   /* #GP  General Protection Fault                 err     */
#define EXC_PF   14   /* #PF  Page Fault                               err     */
/* 15 — reserved */
#define EXC_RSVD_15  15
#define EXC_MF   16   /* #MF  x87 Floating-Point Exception             no  err */
#define EXC_AC   17   /* #AC  Alignment Check                          err (0) */
#define EXC_MC   18   /* #MC  Machine Check                            no  err */
#define EXC_XM   19   /* #XM  SIMD Floating-Point Exception            no  err */
#define EXC_VE   20   /* #VE  Virtualization Exception                 no  err */
#define EXC_CP   21   /* #CP  Control Protection Exception             err     */
/* 22-27 — reserved, no Intel-assigned exception */
#define EXC_RSVD_22  22
#define EXC_RSVD_23  23
#define EXC_RSVD_24  24
#define EXC_RSVD_25  25
#define EXC_RSVD_26  26
#define EXC_RSVD_27  27
#define EXC_HV   28   /* #HV  Hypervisor Injection        (AMD only)   no  err */
#define EXC_VC   29   /* #VC  VMM Communication           (AMD only)   err     */
#define EXC_SX   30   /* #SX  Security Exception          (AMD only)   err     */
/* 31 — reserved */
#define EXC_RSVD_31  31

/* Total number of CPU exception vectors */
#define EXC_COUNT 32

/*
 * IDT gate type byte (bits: P | DPL[1:0] | 0 | type[3:0])
 *
 *  P   = 1   present
 *  DPL = 00  ring 0 only
 *  type 0xE  64-bit interrupt gate (clears IF on entry)
 *  type 0xF  64-bit trap gate      (leaves IF unchanged)
 */
#define IDT_GATE_INTERRUPT  0x8E
#define IDT_GATE_TRAP       0x8F

/* Maximum number of IDT entries (CPU supports 0–255) */
#define IDT_NUM_ENTRIES  256

/* GDT selector for the 64-bit ring-0 code segment (index 1, RPL 0) */
#define GDT_KERNEL_CODE  0x08
