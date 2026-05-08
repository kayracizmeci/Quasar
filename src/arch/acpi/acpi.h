#pragma once
#include <stdint.h>

#define MADT_ISO_POLARITY_MASK   0x03
#define MADT_ISO_POLARITY_LOW    0x03
#define MADT_ISO_TRIGGER_MASK    0x0C
#define MADT_ISO_TRIGGER_LEVEL   0x0C

#define ACPI_MAX_IOAPICS   8

struct acpi_madt_ioapic {
    uint8_t  id;
    uint32_t phys_addr;
    uint32_t gsi_base;
};

struct acpi_madt_iso {
    uint8_t  bus;
    uint8_t  irq;
    uint32_t gsi;
    uint16_t flags;
};

void     acpi_init(void *rsdp_virt);
uint64_t acpi_lapic_phys_addr(void);
int      acpi_ioapic_count(void);
int      acpi_iso_count(void);
const struct acpi_madt_ioapic *acpi_ioapic_get(int index);
const struct acpi_madt_iso    *acpi_iso_get(int index);
