/*
 * ACPI (Advanced Configuration and Power Interface) is a system used by
 * UEFI and BIOS to expose hardware information to the operating system.
 * https://wiki.osdev.org/ACPI
 *
 * RSDP (Root System Description Pointer) is an entry point used to locate
 * and access the other ACPI tables. https://wiki.osdev.org/RSDP
 *
 * The SDT Header is a common header present at the beginning of every
 * ACPI table.
 *
 * MADT (Multiple APIC Description Table) contains information about the
 * interrupt controllers in the system, including the Local APIC address,
 * I/O APIC entries, and Interrupt Source Overrides.
 *
 * What we do in this file is locate and retrieve the MADT from the XSDT.
 * This file simply parses the ACPI tables at boot time and exposes
 * the results to the rest of the kernel.
 *
 * You can find information about ACPI tables in the UEFI spec:
 * https://uefi.org/sites/default/files/resources/ACPI_Spec_6_5_Aug29.pdf
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "arch/acpi/acpi.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "kernel/panic.h"

#define MADT_ENTRY_IOAPIC         1
#define MADT_ENTRY_ISO            2
#define MADT_ENTRY_LAPIC_OVERRIDE 5

#define ACPI_RSDP_REVISION_V2   2
#define ACPI_SDT_SIG_LEN        4
#define ACPI_MAX_ISOS           32


/* Root System Description Pointer (RSDP) */
struct rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_phys;
    /* Fields below exist only when revision >= 2 (ACPI 2.0+) */
    uint32_t length;
    uint64_t xsdt_phys;
    uint8_t  ext_checksum;
    uint8_t  _reserved[3];
} __attribute__((packed));

/* ACPI System Description Table (SDT) */
struct sdt_header {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

/* Multiple APIC Description Table (MADT) */
struct madt {
    struct sdt_header base;
    uint32_t lapic_phys;
    uint32_t flags;
} __attribute__((packed));

struct madt_entry_hdr {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_ioapic_raw {
    struct madt_entry_hdr base;
    uint8_t  id;
    uint8_t  _reserved;
    uint32_t phys_addr;
    uint32_t gsi_base;
} __attribute__((packed));

struct madt_iso_raw {
    struct madt_entry_hdr base;
    uint8_t  bus;
    uint8_t  irq;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

/* 64-bit LAPIC address override for systems whose LAPIC lives above 4 GB */
struct madt_lapic_override {
    struct madt_entry_hdr base;
    uint16_t _reserved;
    uint64_t lapic_phys;
} __attribute__((packed));


static uint64_t                  lapic_phys;
static struct acpi_madt_ioapic   madt_ioapics[ACPI_MAX_IOAPICS];
static int                       ioapic_count;
static struct acpi_madt_iso      madt_isos[ACPI_MAX_ISOS];
static int                       iso_count;


static void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + pmm_hhdm_offset());
}

static void parse_madt(const struct madt *m) {
    lapic_phys = (uint64_t)m->lapic_phys;

    const uint8_t *p   = (const uint8_t *)(m + 1);
    const uint8_t *end = (const uint8_t *)m + m->base.length;

    while (p < end) {
        const struct madt_entry_hdr *hdr = (const struct madt_entry_hdr *)p;

        if (hdr->length == 0)
            kpanic("MADT zero-length entry — malformed table");

        switch (hdr->type) {
        case MADT_ENTRY_IOAPIC:
            if (ioapic_count < ACPI_MAX_IOAPICS) {
                const struct madt_ioapic_raw *r = (const struct madt_ioapic_raw *)p;
                madt_ioapics[ioapic_count].id       = r->id;
                madt_ioapics[ioapic_count].phys_addr = r->phys_addr;
                madt_ioapics[ioapic_count].gsi_base  = r->gsi_base;
                ioapic_count++;
            }
            break;

        case MADT_ENTRY_ISO:
            if (iso_count < ACPI_MAX_ISOS) {
                const struct madt_iso_raw *r = (const struct madt_iso_raw *)p;
                madt_isos[iso_count].bus   = r->bus;
                madt_isos[iso_count].irq   = r->irq;
                madt_isos[iso_count].gsi   = r->gsi;
                madt_isos[iso_count].flags = r->flags;
                iso_count++;
            }
            break;

        case MADT_ENTRY_LAPIC_OVERRIDE: {
            const struct madt_lapic_override *r = (const struct madt_lapic_override *)p;
            __builtin_memcpy(&lapic_phys, &r->lapic_phys, sizeof(uint64_t));
            break;
        }
        default:
            break;
        }

        p += hdr->length;
    }
}

static const struct sdt_header *find_table(const struct sdt_header *root,
                                           bool use_xsdt,
                                           const char sig[ACPI_SDT_SIG_LEN]) {
    size_t entry_size = use_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
    size_t n = (root->length - sizeof(struct sdt_header)) / entry_size;
    const uint8_t *ptr = (const uint8_t *)(root + 1);

    for (size_t i = 0; i < n; i++, ptr += entry_size) {
        uint64_t child_phys;
        if (use_xsdt) {
            __builtin_memcpy(&child_phys, ptr, sizeof(uint64_t));
        } else {
            uint32_t tmp;
            __builtin_memcpy(&tmp, ptr, sizeof(uint32_t));
            child_phys = tmp;
        }

        const struct sdt_header *child = phys_to_virt(child_phys);
        if (__builtin_memcmp(child->signature, sig, 4) == 0) {
            return child;
        }
    }
    return NULL;
}

void acpi_init(void *rsdp_virt) {
    /* rsdp_virt is already a virtual address provided by Limine —
     * do NOT pass it through phys_to_virt(). The child table addresses
     * (xsdt_phys, rsdt_phys) are physical and must be converted. If you do,
     * the pointer will be corrupted and ACPI will not work correctly and you
     * will get a #GP. */

    const struct rsdp *rsdp = rsdp_virt;

    bool use_xsdt = (rsdp->revision >= ACPI_RSDP_REVISION_V2) && (rsdp->xsdt_phys != 0);

    const struct sdt_header *root = phys_to_virt(
        use_xsdt ? rsdp->xsdt_phys : (uint64_t)rsdp->rsdt_phys
    );

    const struct sdt_header *madt_hdr = find_table(root, use_xsdt, "APIC");
    if (madt_hdr == NULL) {
        kpanic("MADT not found — cannot locate APIC hardware");
    }

    parse_madt((const struct madt *)madt_hdr);

    serial_puts("[ACPI] LAPIC phys=");
    serial_puthex64(lapic_phys);
    serial_puts("  ioapics=");
    serial_putu64((uint64_t)ioapic_count);
    serial_puts("  isos=");
    serial_putu64((uint64_t)iso_count);
    serial_puts("\n");
}

uint64_t acpi_lapic_phys_addr(void) { return lapic_phys; }
int      acpi_ioapic_count(void)    { return ioapic_count; }
int      acpi_iso_count(void)       { return iso_count; }

const struct acpi_madt_ioapic *acpi_ioapic_get(int index) {
    return &madt_ioapics[index];
}
const struct acpi_madt_iso *acpi_iso_get(int index) {
    return &madt_isos[index];
}
