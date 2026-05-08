#include "vmm.h"
#include "pmm.h"
#include "drivers/serial.h"
#include "kernel/panic.h"

#include <limine.h>

#define PAGE_SIZE  4096ULL
#define HUGE_SIZE  (2ULL * 1024 * 1024)
#define ENTRIES    512

#define ALIGN_DOWN(x, a)  ((x) & ~((a) - 1))

#define PML4_IDX(v)  (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v)  (((v) >> 30) & 0x1FF)
#define PD_IDX(v)    (((v) >> 21) & 0x1FF)
#define PT_IDX(v)    (((v) >> 12) & 0x1FF)

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exec_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

static uint64_t *kernel_pml4;
static uint64_t  kernel_cr3_phys;

static uint64_t *pgtable_descend(uint64_t *table, uint64_t idx) {
    if (!(table[idx] & VMM_PRESENT)) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        if (!phys)
            kpanic("out of physical memory");
        uint64_t *virt = (uint64_t *)(pmm_hhdm_offset() + phys);
        for (int i = 0; i < ENTRIES; i++) virt[i] = 0;
        table[idx] = phys | VMM_PRESENT | VMM_WRITE;
    }
    return (uint64_t *)(ALIGN_DOWN(table[idx], PAGE_SIZE) + pmm_hhdm_offset());
}

void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pdpt = pgtable_descend(pml4, PML4_IDX(virt));
    uint64_t *pd   = pgtable_descend(pdpt, PDPT_IDX(virt));

    if (flags & VMM_HUGE) {
        if (pd[PD_IDX(virt)] & VMM_PRESENT)
            kpanic("remapping already-mapped huge page");
        pd[PD_IDX(virt)] = ALIGN_DOWN(phys, HUGE_SIZE) | flags;
        return;
    }

    uint64_t *pt = pgtable_descend(pd, PD_IDX(virt));
    if (pt[PT_IDX(virt)] & VMM_PRESENT)
        kpanic("remapping already-mapped page");
    pt[PT_IDX(virt)] = ALIGN_DOWN(phys, PAGE_SIZE) | flags;
}

void vmm_init(void) {
    uint64_t hhdm      = pmm_hhdm_offset();
    uint64_t max_phys  = pmm_max_phys();
    uint64_t kern_phys = exec_addr_request.response->physical_base;
    uint64_t kern_virt = exec_addr_request.response->virtual_base;

    uint64_t pml4_phys = (uint64_t)pmm_alloc_page();
    if (!pml4_phys)
        kpanic("out of physical memory");
    kernel_cr3_phys = pml4_phys;
    kernel_pml4 = (uint64_t *)(hhdm + pml4_phys);
    for (int i = 0; i < ENTRIES; i++) kernel_pml4[i] = 0;

    for (uint64_t off = 0; off < max_phys; off += HUGE_SIZE)
        vmm_map_page(kernel_pml4, hhdm + off, off, VMM_PRESENT | VMM_WRITE | VMM_HUGE);

    for (uint64_t off = 0; off < 64ULL * 1024 * 1024; off += PAGE_SIZE)
        vmm_map_page(kernel_pml4, kern_virt + off, kern_phys + off, VMM_PRESENT | VMM_WRITE);

    asm volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
    serial_puts("[VMM] paging initialized\n");
}

uint64_t vmm_kernel_cr3(void) { return kernel_cr3_phys; }

void vmm_map_kernel_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    vmm_map_page(kernel_pml4, virt, phys, flags);
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}
