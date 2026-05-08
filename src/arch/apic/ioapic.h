#pragma once
#include <stdint.h>

void ioapic_init(void);

/* Unmask a GSI so it can deliver to the CPU.  The GSI must be within the
   range managed by an I/O APIC discovered by acpi_init(). */
void ioapic_unmask_gsi(uint32_t gsi);

/* Mask a GSI to suppress further delivery. */
void ioapic_mask_gsi(uint32_t gsi);
