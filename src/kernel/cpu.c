#include <stdint.h>
#include "cpu.h"

void cpu_init(struct cpu_local *data, uint32_t id, uint32_t lapic_id) {
    data->cpu_id   = id;
    data->lapic_id = lapic_id;

    /* Load the virtual address of this core's cpu_local into IA32_GS_BASE so
       that gs:0 reads give cpu_id without any indirection */
    uint64_t addr = (uint64_t)data;
    asm volatile("wrmsr"
                 : : "c"(MSR_GS_BASE),
                     "a"((uint32_t)addr),
                     "d"((uint32_t)(addr >> 32)));
}
