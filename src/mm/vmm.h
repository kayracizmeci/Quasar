#pragma once

#include <stdint.h>

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITE    (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_HUGE     (1ULL << 7)

void vmm_init(void);
void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_map_kernel_page(uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t vmm_kernel_cr3(void);
