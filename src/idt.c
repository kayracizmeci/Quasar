#include <stdint.h>

#include "exceptions.h"
#include "serial.h"

extern void idt_flush(uint64_t idtr_ptr);

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

struct InterruptGate64 {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t reserved;
} __attribute__((packed));

struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

static struct InterruptGate64 idt[IDT_NUM_ENTRIES];

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_ptr idtr;

static const char *exception_names[EXC_COUNT] = {
    [EXC_DE]       = "#DE  Division Error",
    [EXC_DB]       = "#DB  Debug",
    [EXC_NMI]      = "     Non-Maskable Interrupt",
    [EXC_BP]       = "#BP  Breakpoint",
    [EXC_OF]       = "#OF  Overflow",
    [EXC_BR]       = "#BR  Bound Range Exceeded",
    [EXC_UD]       = "#UD  Invalid Opcode",
    [EXC_NM]       = "#NM  Device Not Available",
    [EXC_DF]       = "#DF  Double Fault",
    [EXC_CSO]      = "     Coprocessor Segment Overrun",
    [EXC_TS]       = "#TS  Invalid TSS",
    [EXC_NP]       = "#NP  Segment Not Present",
    [EXC_SS]       = "#SS  Stack-Segment Fault",
    [EXC_GP]       = "#GP  General Protection Fault",
    [EXC_PF]       = "#PF  Page Fault",
    [EXC_RSVD_15]  = "     Reserved",
    [EXC_MF]       = "#MF  x87 Floating-Point Exception",
    [EXC_AC]       = "#AC  Alignment Check",
    [EXC_MC]       = "#MC  Machine Check",
    [EXC_XM]       = "#XM  SIMD Floating-Point Exception",
    [EXC_VE]       = "#VE  Virtualization Exception",
    [EXC_CP]       = "#CP  Control Protection Exception",
    [EXC_RSVD_22]  = "     Reserved",
    [EXC_RSVD_23]  = "     Reserved",
    [EXC_RSVD_24]  = "     Reserved",
    [EXC_RSVD_25]  = "     Reserved",
    [EXC_RSVD_26]  = "     Reserved",
    [EXC_RSVD_27]  = "     Reserved",
    [EXC_HV]       = "#HV  Hypervisor Injection",
    [EXC_VC]       = "#VC  VMM Communication",
    [EXC_SX]       = "#SX  Security Exception",
    [EXC_RSVD_31]  = "     Reserved",
};

static void (* const isr_table[EXC_COUNT])(void) = {
    [EXC_DE]       = isr0,
    [EXC_DB]       = isr1,
    [EXC_NMI]      = isr2,
    [EXC_BP]       = isr3,
    [EXC_OF]       = isr4,
    [EXC_BR]       = isr5,
    [EXC_UD]       = isr6,
    [EXC_NM]       = isr7,
    [EXC_DF]       = isr8,
    [EXC_CSO]      = isr9,
    [EXC_TS]       = isr10,
    [EXC_NP]       = isr11,
    [EXC_SS]       = isr12,
    [EXC_GP]       = isr13,
    [EXC_PF]       = isr14,
    [EXC_RSVD_15]  = isr15,
    [EXC_MF]       = isr16,
    [EXC_AC]       = isr17,
    [EXC_MC]       = isr18,
    [EXC_XM]       = isr19,
    [EXC_VE]       = isr20,
    [EXC_CP]       = isr21,
    [EXC_RSVD_22]  = isr22,
    [EXC_RSVD_23]  = isr23,
    [EXC_RSVD_24]  = isr24,
    [EXC_RSVD_25]  = isr25,
    [EXC_RSVD_26]  = isr26,
    [EXC_RSVD_27]  = isr27,
    [EXC_HV]       = isr28,
    [EXC_VC]       = isr29,
    [EXC_SX]       = isr30,
    [EXC_RSVD_31]  = isr31,
};

static void idt_set_gate(int vector, uint64_t handler, uint8_t type) {
    idt[vector].offset_1 = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_2 = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_3 = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].selector = GDT_KERNEL_CODE;
    idt[vector].type     = type;
    idt[vector].ist      = 0;
    idt[vector].reserved = 0;
}

void isr_handler(struct InterruptFrame *frame) {
    const char *name = (frame->vector < EXC_COUNT)
                       ? exception_names[frame->vector]
                       : "Unknown interrupt";

    serial_puts("\n[EXCEPTION] ");
    serial_puts(name);
    serial_puts("\n  vector="); serial_puthex64(frame->vector);
    serial_puts("  err=");     serial_puthex64(frame->error_code);
    serial_puts("\n  rip=");   serial_puthex64(frame->rip);
    serial_puts("  rsp=");     serial_puthex64(frame->rsp);
    serial_puts("  rflags=");  serial_puthex64(frame->rflags);
    serial_puts("\n");

    /* #BP is a trap: RIP already points past the int3, so just return */
    if (frame->vector == EXC_BP) {
        return;
    }

    serial_puts("[HALTED]\n");
    for (;;) {
        asm volatile("hlt");
    }
}

void idt_init(void) {
    idtr.limit = (uint16_t)(sizeof(struct InterruptGate64) * IDT_NUM_ENTRIES - 1);
    idtr.base  = (uint64_t)&idt;

    for (int i = 0; i < EXC_COUNT; i++) {
        /* #BP uses a trap gate so IF stays unchanged — all other exceptions
           use an interrupt gate which clears IF to prevent nested faults */
        uint8_t gate_type = (i == EXC_BP) ? IDT_GATE_TRAP : IDT_GATE_INTERRUPT;
        idt_set_gate(i, (uint64_t)isr_table[i], gate_type);
    }

    idt_flush((uint64_t)&idtr);
}
