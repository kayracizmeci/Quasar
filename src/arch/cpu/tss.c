#include "arch/cpu/tss.h"
#include "mm/kheap.h"
#include "kernel/panic.h"

#include <stdint.h>
#include <stddef.h>

#define KERNEL_STACK_SIZE  (8 * 1024)
#define IST_STACK_SIZE     (4 * 1024)

/* Selector for CPU n's TSS: base 0x30 + n * 0x10 (each TSS descriptor is 16 bytes) */
#define TSS_SELECTOR(n)  ((uint16_t)(0x30u + (n) * 0x10u))

/* x86-64 TSS layout (Intel SDM Vol. 3A §7.7) */
struct tss {
    uint32_t _res0;
    uint64_t rsp0;      /* kernel stack for ring-0 entry on interrupt/syscall */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t _res1;
    uint64_t ist1;      /* #DF double-fault stack */
    uint64_t ist2;      /* NMI stack */
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t _res2;
    uint16_t _res3;
    uint16_t iomap_base;
} __attribute__((packed));

/* Exported from gdt.asm — 64-slot pre-allocated TSS descriptor array */
extern uint64_t tss_descs[];

static void write_tss_descriptor(uint32_t cpu_id, uint64_t base) {
    uint32_t limit = sizeof(struct tss) - 1;
    uint64_t *d = &tss_descs[cpu_id * 2];

    d[0] = (uint64_t)(limit & 0xFFFFu)
         | ((base & 0x00FFFFFFull) << 16)
         | ((uint64_t)0x89ull << 40)               /* P=1 DPL=0 Type=9: available 64-bit TSS */
         | ((uint64_t)((limit >> 16) & 0xFu) << 48)
         | ((uint64_t)((base >> 24) & 0xFFu) << 56);
    d[1] = base >> 32;
}

void tss_init_ap(uint32_t cpu_id) {
    if (cpu_id >= TSS_MAX_CPUS)
        kpanic("tss_init_ap: cpu_id exceeds TSS_MAX_CPUS");

    struct tss *tss = kmalloc(sizeof(struct tss));
    if (!tss) kpanic("tss_init_ap: out of memory");
    __builtin_memset(tss, 0, sizeof(struct tss));

    uint8_t *kstack = kmalloc(KERNEL_STACK_SIZE);
    uint8_t *ist1   = kmalloc(IST_STACK_SIZE);
    uint8_t *ist2   = kmalloc(IST_STACK_SIZE);
    if (!kstack || !ist1 || !ist2)
        kpanic("tss_init_ap: out of memory for stacks");

    /* Stacks grow downward — RSP0/IST point to the top (highest address) */
    tss->rsp0      = (uint64_t)(kstack + KERNEL_STACK_SIZE);
    tss->ist1      = (uint64_t)(ist1   + IST_STACK_SIZE);
    tss->ist2      = (uint64_t)(ist2   + IST_STACK_SIZE);
    tss->iomap_base = sizeof(struct tss);  /* no I/O permission bitmap */

    write_tss_descriptor(cpu_id, (uint64_t)tss);

    uint16_t sel = TSS_SELECTOR(cpu_id);
    asm volatile("ltr %0" : : "rm"(sel));
}
