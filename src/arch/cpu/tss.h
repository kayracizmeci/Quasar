#pragma once
#include <stdint.h>

#define TSS_MAX_CPUS 64

/* Allocate and install a TSS for the calling AP.
   Must be called after gdt_load_ap() and before sti. */
void tss_init_ap(uint32_t cpu_id);
