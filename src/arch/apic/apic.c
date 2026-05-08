#include <stdint.h>

#include "arch/apic/apic.h"
#include "arch/cpu/io.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "kernel/panic.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define PIC_ICW1_INIT     0x11
#define PIC_ICW4_8086     0x01
#define PIC_MASK_ALL      0xFF
#define PIC_REMAP_MASTER  0x20
#define PIC_REMAP_SLAVE   0x28

static volatile uint32_t *lapic_mmio;

static uint32_t lapic_read(uint32_t reg) {
    return lapic_mmio[reg / sizeof(uint32_t)];
}

static void lapic_write(uint32_t reg, uint32_t val) {
    lapic_mmio[reg / sizeof(uint32_t)] = val;
}

static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void wrmsr(uint32_t msr, uint64_t val) {
    asm volatile("wrmsr" : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

/* Remap PIC to vectors 32-47 before masking so stray PIC interrupts after
   the remap don't land on CPU exception vectors 0-15. */
static void pic_disable(void) {
    outb(PIC1_CMD,  PIC_ICW1_INIT); io_wait();
    outb(PIC2_CMD,  PIC_ICW1_INIT); io_wait();
    outb(PIC1_DATA, PIC_REMAP_MASTER); io_wait();
    outb(PIC2_DATA, PIC_REMAP_SLAVE);  io_wait();
    outb(PIC1_DATA, 0x04); io_wait();  /* master: IRQ2 has slave */
    outb(PIC2_DATA, 0x02); io_wait();  /* slave: cascade identity = 2 */
    outb(PIC1_DATA, PIC_ICW4_8086); io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086); io_wait();
    outb(PIC1_DATA, PIC_MASK_ALL);
    outb(PIC2_DATA, PIC_MASK_ALL);
}

static void lapic_mask_all_lvt(void) {
    lapic_write(LAPIC_REG_LVT_TIMER,   LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_THERMAL, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_PERF,    LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT0,   LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT1,   LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_ERROR,   LAPIC_LVT_MASKED);
}

void apic_init(uint64_t lapic_phys) {
    uint32_t edx;
    asm volatile("cpuid" : "=d"(edx) : "a"(1) : "ebx", "ecx");
    if (!(edx & CPUID1_EDX_APIC))
        kpanic("CPU does not have a local APIC");

    uint64_t base_msr = rdmsr(MSR_APIC_BASE);
    if (base_msr & MSR_APIC_BASE_X2APIC)
        kpanic("x2APIC is already enabled by firmware — xAPIC MMIO not usable");

    base_msr |= MSR_APIC_BASE_ENABLE;
    wrmsr(MSR_APIC_BASE, base_msr);

    lapic_mmio = (volatile uint32_t *)(lapic_phys + pmm_hhdm_offset());

    pic_disable();
    lapic_mask_all_lvt();

    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_TPR, 0);
    lapic_write(LAPIC_REG_DFR, 0xFFFFFFFFu);   /* flat logical model */
    lapic_write(LAPIC_REG_LDR, 0xFF000000u);   /* all CPUs in group 0xFF */
    lapic_write(LAPIC_REG_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);

    serial_puts("[LAPIC] id=");
    serial_puthex64(lapic_read(LAPIC_REG_ID) >> 24);
    serial_puts("  version=");
    serial_puthex64(lapic_read(LAPIC_REG_VERSION) & 0xFF);
    serial_puts("\n");
}

void apic_init_ap(void) {
    uint64_t base_msr = rdmsr(MSR_APIC_BASE);
    if (base_msr & MSR_APIC_BASE_X2APIC)
        kpanic("x2APIC enabled on AP — xAPIC MMIO not usable");

    base_msr |= MSR_APIC_BASE_ENABLE;
    wrmsr(MSR_APIC_BASE, base_msr);

    lapic_mask_all_lvt();
    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_TPR, 0);
    lapic_write(LAPIC_REG_DFR, 0xFFFFFFFFu);
    lapic_write(LAPIC_REG_LDR, 0xFF000000u);
    lapic_write(LAPIC_REG_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);
}

void apic_eoi(void) {
    lapic_write(LAPIC_REG_EOI, 0);
}

uint8_t apic_id(void) {
    return (uint8_t)(lapic_read(LAPIC_REG_ID) >> 24);
}

void apic_timer_arm_calibration(void) {
    lapic_write(LAPIC_REG_TIMER_DCR, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED | LAPIC_TIMER_ONESHOT);
    lapic_write(LAPIC_REG_TIMER_ICR, 0xFFFFFFFFu);
}

uint32_t apic_timer_read_elapsed(void) {
    return 0xFFFFFFFFu - lapic_read(LAPIC_REG_TIMER_CCR);
}

void apic_timer_start_periodic(uint32_t ticks_per_period, uint8_t vector) {
    lapic_write(LAPIC_REG_TIMER_DCR, LAPIC_TIMER_DIV1);
    /* LVT mode must be written before ICR — ICR write starts the countdown */
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | vector);
    lapic_write(LAPIC_REG_TIMER_ICR, ticks_per_period);
}
