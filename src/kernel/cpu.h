#pragma once
#include <stdint.h>

/* IA32_GS_BASE MSR — writing this sets the GS base address used by gs: memory
   operands in 64-bit mode.  The GDT descriptor base is ignored for FS/GS. */
#define MSR_GS_BASE  0xC0000101u

/* Per-CPU data block.  cpu_id must stay at offset 0 so that any core can read
   its own ID with a single "movl %gs:0, %eax" without knowing the struct layout. */
struct cpu_local {
    uint32_t cpu_id;     /* offset  0 */
    uint32_t lapic_id;   /* offset  4 */
};

/* Read the calling core's CPU ID via the gs-relative zero-offset shortcut. */
static inline uint32_t this_cpu_id(void) {
    uint32_t id;
    asm volatile("movl %%gs:0, %0" : "=r"(id));
    return id;
}

/* Populate *data and point this core's GS base at it. */
void cpu_init(struct cpu_local *data, uint32_t id, uint32_t lapic_id);
