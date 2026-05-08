#pragma once
#include <stdint.h>

/* Calibrate the LAPIC timer against the PIT and start a 10 ms periodic tick.
   Must be called after apic_init() and ioapic_init(), before sti. */
void     timer_init(void);

uint64_t timer_uptime_ms(void);

/* Raw 10 ms tick counter. */
uint64_t timer_tick_count(void);
