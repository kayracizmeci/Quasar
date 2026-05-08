#include <stdint.h>
#include "timer.h"
#include "pit.h"
#include "arch/apic/apic.h"
#include "arch/cpu/idt.h"
#include "arch/cpu/exceptions.h"
#include "drivers/serial.h"

/* Written only from the IRQ handler, read from normal code.
   volatile prevents the compiler from caching the value in a register. */
static volatile uint64_t ticks;

static void timer_irq_handler(struct InterruptFrame *frame) {
    (void)frame;
    ticks++;
}

void timer_init(void) {
    /* Use PIT channel 2 as a reference: measure how many LAPIC timer ticks
       elapse in exactly one 10 ms window.  This accounts for the actual bus
       clock frequency of the machine without any hard-coded assumptions. */
    apic_timer_arm_calibration();
    pit_start_calibration();
    pit_wait_calibration();
    uint32_t ticks_per_10ms = apic_timer_read_elapsed();

    /* Vector IRQ_VECTOR_BASE (32) is the conventional system-timer slot;
       its IDT gate was installed by idt_init() via the irq32 stub. */
    irq_register_handler(IRQ_VECTOR_BASE, timer_irq_handler);
    apic_timer_start_periodic(ticks_per_10ms, IRQ_VECTOR_BASE);

    serial_puts("[TIMER] 10 ms tick  LAPIC ticks/period=");
    serial_putu64(ticks_per_10ms);
    serial_puts("\n");
}

uint64_t timer_uptime_ms(void) {
    return ticks * 10;
}

uint64_t timer_tick_count(void) {
    return ticks;
}
