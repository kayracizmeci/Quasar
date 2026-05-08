#include <stdint.h>
#include <stddef.h>

#include "arch/apic/ioapic.h"
#include "arch/acpi/acpi.h"
#include "arch/apic/apic.h"
#include "arch/cpu/exceptions.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "kernel/panic.h"

/*
 * I/O APIC MMIO layout (Intel 82093AA datasheet §3).
 * Two registers are exposed: an index port and a data window.
 * All named I/O APIC registers are accessed by writing the index first.
 */
#define IOAPIC_REGSEL  0x00  /* byte offset of the register select port */
#define IOAPIC_REGWIN  0x10  /* byte offset of the data window */

/* Named index values for the I/O APIC register set */
#define IOAPIC_REG_VER  0x01  /* bits [23:16] hold (max_redirection_entries - 1) */

/* Redirection table: entry n occupies two 32-bit registers at index 0x10+2n (lo)
   and 0x11+2n (hi).  The base covers all 24 lines that the 82093AA supports. */
#define IOAPIC_REDTBL_BASE      0x10
#define IOAPIC_MAX_LINES        24   /* hardware maximum on the 82093AA */

/* Redirection entry low-word field layout */
#define IOAPIC_REDIR_DELIV_LOWPRI    (1u << 8)   /* lowest-priority delivery — any eligible CPU */
#define IOAPIC_REDIR_DESTMODE_LOGICAL (1u << 11) /* destination is a logical APIC group */
#define IOAPIC_REDIR_POLARITY_HIGH (0u << 13)  /* active high (ISA default) */
#define IOAPIC_REDIR_POLARITY_LOW  (1u << 13)  /* active low (PCI default) */
#define IOAPIC_REDIR_TRIGGER_EDGE  (0u << 15)  /* edge-triggered (ISA default) */
#define IOAPIC_REDIR_TRIGGER_LEVEL (1u << 15)  /* level-triggered (PCI) */
#define IOAPIC_REDIR_MASKED        (1u << 16)  /* suppress delivery */

#define IOAPIC_VER_MAXREDIR_SHIFT  16
#define IOAPIC_VER_MAXREDIR_MASK   0xFFu

/* Redirection entry high-word: bits [31:24] hold the destination APIC ID */
#define IOAPIC_REDIR_DEST_SHIFT    24


struct ioapic_state {
    volatile uint32_t *base;   /* virtual address of MMIO region */
    uint32_t           gsi_base;
    uint8_t            max_lines; /* number of redirection entries */
};

static struct ioapic_state ioapics[ACPI_MAX_IOAPICS];
static int                 num_ioapics;


static uint32_t ioapic_read(const struct ioapic_state *io, uint8_t reg) {
    /* Two-step indexed access: write index to REGSEL, read data from REGWIN */
    io->base[IOAPIC_REGSEL / sizeof(uint32_t)] = reg;
    return io->base[IOAPIC_REGWIN / sizeof(uint32_t)];
}

static void ioapic_write(const struct ioapic_state *io, uint8_t reg, uint32_t val) {
    io->base[IOAPIC_REGSEL / sizeof(uint32_t)] = reg;
    io->base[IOAPIC_REGWIN / sizeof(uint32_t)] = val;
}


static void redir_write(const struct ioapic_state *io, uint8_t line,
                        uint32_t lo, uint32_t hi) {
    /* High word must be written first so the vector is never visible to the
       hardware with a stale destination while the low word is being updated */
    ioapic_write(io, (uint8_t)(IOAPIC_REDTBL_BASE + 2 * line + 1), hi);
    ioapic_write(io, (uint8_t)(IOAPIC_REDTBL_BASE + 2 * line),     lo);
}

static uint32_t redir_read_lo(const struct ioapic_state *io, uint8_t line) {
    return ioapic_read(io, (uint8_t)(IOAPIC_REDTBL_BASE + 2 * line));
}

/* Locate which I/O APIC owns a given GSI */
static struct ioapic_state *find_ioapic_for_gsi(uint32_t gsi, uint8_t *out_line) {
    for (int i = 0; i < num_ioapics; i++) {
        uint32_t gsi_end = ioapics[i].gsi_base + ioapics[i].max_lines;
        if (gsi >= ioapics[i].gsi_base && gsi < gsi_end) {
            *out_line = (uint8_t)(gsi - ioapics[i].gsi_base);
            return &ioapics[i];
        }
    }
    return NULL;
}


void ioapic_init(void) {
    if (acpi_ioapic_count() == 0) {
        kpanic("no I/O APIC found in MADT");
    }

    num_ioapics = acpi_ioapic_count();

    for (int i = 0; i < num_ioapics; i++) {
        const struct acpi_madt_ioapic *info = acpi_ioapic_get(i);
        ioapics[i].base     = (volatile uint32_t *)((uint64_t)info->phys_addr + pmm_hhdm_offset());
        ioapics[i].gsi_base = info->gsi_base;

        /* Version register encodes (entries - 1) in bits [23:16] */
        uint32_t ver = ioapic_read(&ioapics[i], IOAPIC_REG_VER);
        ioapics[i].max_lines =
            (uint8_t)(((ver >> IOAPIC_VER_MAXREDIR_SHIFT) & IOAPIC_VER_MAXREDIR_MASK) + 1);

        serial_puts("[IOAPIC] id=");
        serial_puthex64(info->id);
        serial_puts("  gsi_base=");
        serial_putu64(info->gsi_base);
        serial_puts("  lines=");
        serial_putu64(ioapics[i].max_lines);
        serial_puts("\n");
    }

    /* Broadcast to all CPUs in logical group 0xFF — the hardware picks whichever
       core has the lowest current task priority (lowest-priority delivery). */
    uint32_t dest_hi = 0xFFu << IOAPIC_REDIR_DEST_SHIFT;

    for (int i = 0; i < num_ioapics; i++) {
        for (uint8_t line = 0; line < ioapics[i].max_lines; line++) {
            uint32_t gsi = ioapics[i].gsi_base + line;

            if (IRQ_VECTOR_BASE + gsi >= (uint32_t)IRQ_VECTOR_SPURIOUS) {
                continue;
            }

            uint8_t vector = (uint8_t)(IRQ_VECTOR_BASE + gsi);

            uint32_t lo = IOAPIC_REDIR_DELIV_LOWPRI
                        | IOAPIC_REDIR_DESTMODE_LOGICAL
                        | IOAPIC_REDIR_POLARITY_HIGH
                        | IOAPIC_REDIR_TRIGGER_EDGE
                        | IOAPIC_REDIR_MASKED
                        | vector;

            redir_write(&ioapics[i], line, lo, dest_hi);
        }
    }

    /* Apply ACPI Interrupt Source Overrides — these tell us when an ISA IRQ
       arrives on a different GSI and/or with non-default polarity/trigger mode.
       For example IRQ 0 (PIT) is often wired to GSI 2 on ACPI systems. */
    for (int i = 0; i < acpi_iso_count(); i++) {
        const struct acpi_madt_iso *iso = acpi_iso_get(i);

        uint8_t line;
        struct ioapic_state *io = find_ioapic_for_gsi(iso->gsi, &line);
        if (io == NULL) continue;

        uint8_t  vector = (uint8_t)(IRQ_VECTOR_BASE + iso->gsi);
        uint32_t lo     = IOAPIC_REDIR_DELIV_LOWPRI
                        | IOAPIC_REDIR_DESTMODE_LOGICAL
                        | IOAPIC_REDIR_MASKED
                        | vector;

        /* Honour polarity override only when firmware says it differs from bus default */
        uint8_t polarity = iso->flags & MADT_ISO_POLARITY_MASK;
        if (polarity == MADT_ISO_POLARITY_LOW) {
            lo |= IOAPIC_REDIR_POLARITY_LOW;
        }

        /* Honour trigger mode override only when firmware says it differs */
        uint8_t trigger = iso->flags & MADT_ISO_TRIGGER_MASK;
        if (trigger == MADT_ISO_TRIGGER_LEVEL) {
            lo |= IOAPIC_REDIR_TRIGGER_LEVEL;
        }

        redir_write(io, line, lo, dest_hi);

        serial_puts("[IOAPIC] ISO: IRQ ");
        serial_putu64(iso->irq);
        serial_puts(" -> GSI ");
        serial_putu64(iso->gsi);
        serial_puts("\n");
    }
}

void ioapic_unmask_gsi(uint32_t gsi) {
    uint8_t line;
    struct ioapic_state *io = find_ioapic_for_gsi(gsi, &line);
    if (io == NULL) return;

    uint32_t lo = redir_read_lo(io, line);
    lo &= ~IOAPIC_REDIR_MASKED;
    ioapic_write(io, (uint8_t)(IOAPIC_REDTBL_BASE + 2 * line), lo);
}

void ioapic_mask_gsi(uint32_t gsi) {
    uint8_t line;
    struct ioapic_state *io = find_ioapic_for_gsi(gsi, &line);
    if (io == NULL) return;

    uint32_t lo = redir_read_lo(io, line);
    lo |= IOAPIC_REDIR_MASKED;
    ioapic_write(io, (uint8_t)(IOAPIC_REDTBL_BASE + 2 * line), lo);
}
