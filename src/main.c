#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "drivers/serial.h"
#include "arch/acpi/acpi.h"
#include "arch/apic/apic.h"
#include "arch/apic/ioapic.h"
#include "drivers/timer.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/kheap.h"
#include "kernel/cpu.h"
#include "arch/cpu/tss.h"

#define MAX_CPUS  64

extern void gdt_load(void);
extern void gdt_load_ap(void);
extern void idt_init(void);
extern void idt_load(void);

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
    .flags = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;


void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = dest;
    const uint8_t *restrict psrc = src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = dest;
    const uint8_t *psrc = src;

    if ((uintptr_t)src > (uintptr_t)dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if ((uintptr_t)src < (uintptr_t)dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = s1;
    const uint8_t *p2 = s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

static void __attribute__((noreturn)) halt(void) {
    for (;;) {
        asm volatile("hlt");
    }
}

static struct cpu_local cpu_locals[MAX_CPUS];

/* Entry point Limine calls for each AP.  extra_argument carries a pointer to
   the cpu_local block allocated for this core on the BSP. */
static void __attribute__((noreturn)) ap_entry(struct limine_mp_info *info) {
    /* Switch to the kernel page table before touching any kernel virtual address.
       Limine starts APs with its own CR3 which does not map device MMIO. */
    uint64_t cr3 = vmm_kernel_cr3();
    asm volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");

    gdt_load_ap();
    idt_load();

    struct cpu_local *local = (struct cpu_local *)info->extra_argument;

    /* Install this AP's TSS now that the GDT is loaded and the heap is usable */
    tss_init_ap(local->cpu_id);

    apic_init_ap();
    cpu_init(local, (uint32_t)local->cpu_id, info->lapic_id);

    asm volatile("sti");

    serial_lock();
    serial_puts_raw("[SMP] AP ");
    serial_putu64_raw(local->cpu_id);
    serial_puts_raw(" online\n");
    serial_unlock();

    for (;;) {
        asm volatile("hlt");
    }
}

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        halt();
    }

    gdt_load();
    serial_init();
    serial_puts("[GDT] loaded\n");
    serial_puts("[SERIAL] COM1 initialized\n");

    idt_init();
    serial_puts("[IDT] loaded\n");

    pmm_init();
    vmm_init();
    kheap_init();

    /* ACPI must be parsed before the APIC is touched because it locates the
       physical addresses of both the Local APIC and the I/O APIC. */
    if (rsdp_request.response == NULL) {
        serial_puts("[ACPI] bootloader did not provide RSDP — halting\n");
        halt();
    }
    acpi_init(rsdp_request.response->address);
    apic_init(acpi_lapic_phys_addr());
    serial_puts("[LAPIC] initialized\n");
    ioapic_init();
    serial_puts("[IOAPIC] initialized\n");

    /* Calibrate and start the LAPIC timer before enabling interrupts.
       The timer arms but does not fire until the CPU's IF flag is set. */
    timer_init();

    /* Wire up per-CPU state for the BSP (cpu 0) before enabling interrupts
       so that this_cpu_id() is valid before any IRQ handler runs. */
    cpu_init(&cpu_locals[0], 0, apic_id());

    /* Enable interrupts now that all handlers and the timer are in place */
    asm volatile("sti");

    /* Wake each AP in turn; goto_address write is the atomic release signal. */
    if (mp_request.response != NULL) {
        struct limine_mp_response *mp = mp_request.response;
        serial_puts("[SMP] ");
        serial_putu64(mp->cpu_count);
        serial_puts(" CPU(s) found\n");

        for (uint64_t i = 0; i < mp->cpu_count; i++) {
            struct limine_mp_info *cpu = mp->cpus[i];

            /* The BSP is already running; skip it */
            if (cpu->lapic_id == mp->bsp_lapic_id)
                continue;

            if (i >= MAX_CPUS) {
                serial_puts("[SMP] cpu_count exceeds MAX_CPUS — extra cores skipped\n");
                break;
            }

            cpu_locals[i].cpu_id = (uint32_t)i;
            cpu->extra_argument = (uint64_t)&cpu_locals[i];
            __atomic_store_n(&cpu->goto_address, ap_entry, __ATOMIC_RELEASE);
        }
    }

    serial_puts("[BOOT] ready\n");

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        serial_puts("[FB] no framebuffer found — halting\n");
        halt();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    volatile uint32_t *fb_ptr = framebuffer->address;
    size_t pitch_px = framebuffer->pitch / 4;

    for (size_t y = 0; y < framebuffer->height; y++)
        for (size_t x = 0; x < framebuffer->width; x++)
            fb_ptr[y * pitch_px + x] = 0x000000;

    halt();
}
