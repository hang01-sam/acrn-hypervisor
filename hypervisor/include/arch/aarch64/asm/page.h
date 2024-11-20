/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_PAGE_H__
#define __AARCH64_PAGE_H__

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	0xFFFFFFFFFFFFF000UL

#define MAX_PHY_ADDRESS_SPACE	(1UL << 48)

#ifndef __ASSEMBLY__
#include <asm/lib/spinlock.h>
#include <board_info.h>

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

/* size of the high MMIO address space: 1GB */
#define PLATFORM_HI_MMIO_SIZE	0x40000000UL

#define PML4_PAGE_NUM(size)	1UL
#define PDPT_PAGE_NUM(size)	(((size) + PML4E_SIZE - 1UL) >> PML4E_SHIFT)
#define PD_PAGE_NUM(size)	(((size) + PDPTE_SIZE - 1UL) >> PDPTE_SHIFT)
#define PT_PAGE_NUM(size)	(((size) + PDE_SIZE - 1UL) >> PDE_SHIFT)
#define ALIGN(VAL, TO) ((((VAL) + (TO)-1) / (TO)) * TO)
#define NUM_PAGES(SZ) (ALIGN(SZ, PAGE_SIZE)/PAGE_SIZE)


struct page {
	uint8_t contents[PAGE_SIZE];
} __aligned(PAGE_SIZE);

struct page_pool {
	struct page *start_page;
	spinlock_t lock;
	uint64_t bitmap_size;
	uint64_t *bitmap;
	uint64_t last_hint_id;

	struct page *dummy_page;
};

struct page *alloc_page(struct page_pool *pool);
void free_page(struct page_pool *pool, struct page *page);
#endif /* !__ASSEMBLY__ */
#endif /* __AARCH64_PAGE_H__ */
