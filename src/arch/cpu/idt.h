#pragma once
#include <stdint.h>

struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct InterruptFrame *);

void idt_init(void);

/* Reload the IDT register on the calling CPU.  Call this on each AP after
   gdt_load_ap() so they share the BSP's already-populated IDT table. */
void idt_load(void);

/* Register a C handler for a hardware IRQ vector (IRQ_VECTOR_BASE + n).
   Only one handler per vector is supported; a second registration replaces
   the first without warning, which is intentional for simple drivers. */
void irq_register_handler(int vector, irq_handler_t handler);
