#include "kheap.h"
#include "pmm.h"
#include "vmm.h"
#include "drivers/serial.h"
#include "kernel/panic.h"

#include <stdint.h>

#define HEAP_BASE   0xffffffff90000000ULL
#define HEAP_PAGES  1024
#define PAGE_SIZE   4096ULL

struct block {
    size_t        size;
    int           free;
    struct block *next;
};

#define HEADER_SIZE  sizeof(struct block)

static struct block *heap_start;

void kheap_init(void) {
    for (size_t i = 0; i < HEAP_PAGES; i++) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        if (!phys)
            kpanic("out of physical memory");
        vmm_map_kernel_page(HEAP_BASE + i * PAGE_SIZE, phys, VMM_PRESENT | VMM_WRITE);
    }

    heap_start        = (struct block *)HEAP_BASE;
    heap_start->size  = HEAP_PAGES * PAGE_SIZE - HEADER_SIZE;
    heap_start->free  = 1;
    heap_start->next  = NULL;

    serial_puts("[HEAP] initialized — ");
    serial_putu64((HEAP_PAGES * PAGE_SIZE) / 1024);
    serial_puts(" KB\n");
}

void *kmalloc(size_t size) {
    if (!size) return NULL;
    struct block *cur = heap_start;
    while (cur) {
        if (cur->free && cur->size >= size) {
            if (cur->size >= size + HEADER_SIZE + 1) {
                struct block *split = (struct block *)((uint8_t *)(cur + 1) + size);
                split->size = cur->size - size - HEADER_SIZE;
                split->free = 1;
                split->next = cur->next;
                cur->size   = size;
                cur->next   = split;
            }
            cur->free = 0;
            return (void *)(cur + 1);
        }
        cur = cur->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    struct block *blk = (struct block *)ptr - 1;
    if (blk->free)
        kpanic("kfree double free");
    blk->free = 1;

    /* Forward coalescing */
    while (blk->next && blk->next->free) {
        blk->size += HEADER_SIZE + blk->next->size;
        blk->next  = blk->next->next;
    }

    /* Backward coalescing: find the block immediately before blk */
    if (blk != heap_start) {
        struct block *prev = heap_start;
        while (prev->next && prev->next != blk)
            prev = prev->next;
        if (prev->free) {
            prev->size += HEADER_SIZE + blk->size;
            prev->next  = blk->next;
        }
    }
}
