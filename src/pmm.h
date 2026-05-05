#pragma once

#include <stdint.h>
#include <stddef.h>

#include <limine.h>

void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
