#include <stdint.h>
#include <stddef.h>

#include "arch/cpu/exceptions.h"
#include "arch/cpu/idt.h"
#include "drivers/serial.h"

extern void idt_flush(uint64_t idtr_ptr);
/* EOI (End Of Interrupt) must be sent after every real hardware IRQ; declared here to avoid
   pulling the full APIC header into the generic interrupt dispatch path */
extern void apic_eoi(void);

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

extern void irq32(void);  extern void irq33(void);  extern void irq34(void);
extern void irq35(void);  extern void irq36(void);  extern void irq37(void);
extern void irq38(void);  extern void irq39(void);  extern void irq40(void);
extern void irq41(void);  extern void irq42(void);  extern void irq43(void);
extern void irq44(void);  extern void irq45(void);  extern void irq46(void);
extern void irq47(void);  extern void irq48(void);  extern void irq49(void);
extern void irq50(void);  extern void irq51(void);  extern void irq52(void);
extern void irq53(void);  extern void irq54(void);  extern void irq55(void);
extern void irq255(void);

struct InterruptGate64 {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t reserved;
} __attribute__((packed));

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

static void idt_set_gate(int vector, uint64_t handler, uint8_t type, uint8_t ist) {
    idt[vector].offset_1 = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_2 = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_3 = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].selector = GDT_KERNEL_CODE;
    idt[vector].type     = type;
    idt[vector].ist      = ist;
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
    serial_puts("  cs=");      serial_puthex64(frame->cs);
    serial_puts("\n  rsp=");   serial_puthex64(frame->rsp);
    serial_puts("  ss=");      serial_puthex64(frame->ss);
    serial_puts("\n  rflags="); serial_puthex64(frame->rflags);
    serial_puts("\n  rax=");   serial_puthex64(frame->rax);
    serial_puts("  rbx=");     serial_puthex64(frame->rbx);
    serial_puts("  rcx=");     serial_puthex64(frame->rcx);
    serial_puts("  rdx=");     serial_puthex64(frame->rdx);
    serial_puts("\n  rsi=");   serial_puthex64(frame->rsi);
    serial_puts("  rdi=");     serial_puthex64(frame->rdi);
    serial_puts("  rbp=");     serial_puthex64(frame->rbp);
    serial_puts("\n  r8=");    serial_puthex64(frame->r8);
    serial_puts("   r9=");     serial_puthex64(frame->r9);
    serial_puts("  r10=");     serial_puthex64(frame->r10);
    serial_puts("  r11=");     serial_puthex64(frame->r11);
    serial_puts("\n  r12=");   serial_puthex64(frame->r12);
    serial_puts("  r13=");     serial_puthex64(frame->r13);
    serial_puts("  r14=");     serial_puthex64(frame->r14);
    serial_puts("  r15=");     serial_puthex64(frame->r15);

    if (frame->vector == EXC_PF) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_puts("\n  cr2="); serial_puthex64(cr2);
        serial_puts("  (faulting address)");
    }

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


typedef void (*irq_handler_t)(struct InterruptFrame *);

static irq_handler_t irq_handlers[IRQ_VECTOR_COUNT];

void irq_register_handler(int vector, irq_handler_t handler) {
    int slot = vector - IRQ_VECTOR_BASE;
    if (slot >= 0 && slot < IRQ_VECTOR_COUNT) {
        irq_handlers[slot] = handler;
    }
}

/* Called from irq_common in idt.asm for every hardware interrupt */
void irq_dispatch(struct InterruptFrame *frame) {
    /* The APIC spurious vector fires when an IRQ is withdrawn between the
       APIC asserting it and the CPU acknowledging it.  No EOI is needed or
       permitted in that case — sending one would de-sync the APIC state. */
    if (frame->vector == IRQ_VECTOR_SPURIOUS) {
        return;
    }

    int slot = (int)frame->vector - IRQ_VECTOR_BASE;
    if (slot >= 0 && slot < IRQ_VECTOR_COUNT && irq_handlers[slot] != NULL) {
        irq_handlers[slot](frame);
    }

    /* EOI goes out after the handler returns so level-triggered devices see
       the line de-asserted only once the kernel has finished servicing it */
    apic_eoi();
}


static void (* const irq_stub_table[])(void) = {
    irq32,  irq33,  irq34,  irq35,  irq36,  irq37,  irq38,  irq39,
    irq40,  irq41,  irq42,  irq43,  irq44,  irq45,  irq46,  irq47,
    irq48,  irq49,  irq50,  irq51,  irq52,  irq53,  irq54,  irq55,
};

/* Number of entries in irq_stub_table (24 stubs: vectors 32-55) */
#define IRQ_STUB_COUNT  24

void idt_init(void) {
    idtr.limit = (uint16_t)(sizeof(struct InterruptGate64) * IDT_NUM_ENTRIES - 1);
    idtr.base  = (uint64_t)&idt;

    for (int i = 0; i < EXC_COUNT; i++) {
        /* #BP uses a trap gate so IF stays unchanged — all other exceptions
           use an interrupt gate which clears IF to prevent nested faults */
        uint8_t gate_type = (i == EXC_BP) ? IDT_GATE_TRAP : IDT_GATE_INTERRUPT;

        /* Critical exceptions get a dedicated IST stack so they remain
           handleable even if the main kernel stack is corrupted */
        uint8_t ist = 0;
        if (i == EXC_DF)  ist = 1;   /* #DF double fault  → IST1 */
        if (i == EXC_NMI) ist = 2;   /* NMI               → IST2 */

        idt_set_gate(i, (uint64_t)isr_table[i], gate_type, ist);
    }

    /* Install hardware IRQ stubs.  IST 0 is correct here: if a hardware IRQ
       arrives on a broken stack we have bigger problems than the stack itself. */
    for (int i = 0; i < IRQ_STUB_COUNT; i++) {
        int vector = IRQ_VECTOR_BASE + i;
        idt_set_gate(vector, (uint64_t)irq_stub_table[i], IDT_GATE_INTERRUPT, 0);
    }

    /* Spurious vector must be registered so the CPU has a valid gate to jump
       to if the APIC fires it; without this gate a double fault would occur */
    idt_set_gate(IRQ_VECTOR_SPURIOUS, (uint64_t)irq255, IDT_GATE_INTERRUPT, 0);

    idt_flush((uint64_t)&idtr);
}

void idt_load(void) {
    idt_flush((uint64_t)&idtr);
}
