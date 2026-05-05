#include "pmm.h"
#include "serial.h"

#define PAGE_SIZE       4096ULL
#define BITS_PER_BYTE   8
#define BITMAP_FULL     0xFF

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

static uint64_t hhdm_offset;
static uint8_t *bitmap;
static uint64_t total_pages;

static void bitmap_set(uint64_t page) {
    bitmap[page / BITS_PER_BYTE] |= (uint8_t)(1 << (page % BITS_PER_BYTE));
}

static void bitmap_clear(uint64_t page) {
    bitmap[page / BITS_PER_BYTE] &= (uint8_t)~(1 << (page % BITS_PER_BYTE));
}

static int bitmap_test(uint64_t page) {
    return bitmap[page / BITS_PER_BYTE] & (uint8_t)(1 << (page % BITS_PER_BYTE));
}

void pmm_init(void) {
    struct limine_memmap_response *memmap = memmap_request.response;
    hhdm_offset = hhdm_request.response->offset;

    uint64_t max_addr = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        uint64_t end = e->base + e->length;
        if (end > max_addr)
            max_addr = end;
    }

    total_pages = max_addr / PAGE_SIZE;
    uint64_t bitmap_size = (total_pages + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size) {
            bitmap = (uint8_t *)(hhdm_offset + e->base);
            break;
        }
    }

    for (uint64_t i = 0; i < bitmap_size; i++)
        bitmap[i] = BITMAP_FULL;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t p = e->base; p < e->base + e->length; p += PAGE_SIZE)
                bitmap_clear(p / PAGE_SIZE);
        }
    }

    uint64_t bitmap_phys = (uint64_t)bitmap - hhdm_offset;
    uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t p = 0; p < bitmap_pages; p++)
        bitmap_set(bitmap_phys / PAGE_SIZE + p);

    uint64_t usable_bytes = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        if (memmap->entries[i]->type == LIMINE_MEMMAP_USABLE)
            usable_bytes += memmap->entries[i]->length;
    }

    serial_puts("[PMM] initialized — ");
    serial_putu64(usable_bytes / (1024 * 1024));
    serial_puts(" MB usable\n");
}

void *pmm_alloc_page(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            return (void *)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *addr) {
    bitmap_clear((uint64_t)addr / PAGE_SIZE);
}
