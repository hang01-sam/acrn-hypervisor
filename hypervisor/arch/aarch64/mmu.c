/*-
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (c) 2017-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <types.h>
#include <asm/lib/atomic.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/arch.h>
#include <reloc.h>
#include <asm/e820.h>
#include <asm/guest/vm.h>
#include <logmsg.h>
#include <misc_cfg.h>

#define DBG_LEVEL_MMU	6U

static uint64_t hv_ram_size;
static void *ppt_mmu_pml4_addr;
static uint8_t sanitized_page[PAGE_SIZE] __aligned(PAGE_SIZE);

/* PPT VA and PA are identical mapping */
uint64_t get_ppt_page_num(void)
{
	uint64_t ppt_pd_page_num;
	uint64_t ram_ppt_number = 1024; /* leaf page nubmer is predicted by experience */
	uint64_t dev_ppt_number = 1024; /* leaf page nubmer is predicted by experience */

	ppt_pd_page_num = ram_ppt_number + dev_ppt_number;

	return ppt_pd_page_num;
}

/* ppt: primary page pool */
static struct page_pool ppt_page_pool;

static inline void ppt_clflush_pagewalk(__unused const void *entry)
{
}

/* @pre: The PPT and EPT have same page granularity */
static inline bool ppt_large_page_support(enum _page_table_level level, __unused uint64_t prot)
{
	bool support;

	if (level == PGT_PD) {
		support = true;
	} else if (level == PGT_PDPT) {
		support = true;
	} else {
		support = false;
	}

	return support;
}

static inline void ppt_nop_tweak_exe_right(__unused uint64_t *entry) {}
static inline void ppt_nop_recover_exe_right(__unused uint64_t *entry) {}

static const struct pgtable ppt_pgtable = {
	.default_access_right = PAGE_TABLE,
	.pgentry_present_mask = PAGE_PRESENT,
	.pool = &ppt_page_pool,
	.large_page_support = ppt_large_page_support,
	.clflush_pagewalk = ppt_clflush_pagewalk,
	.tweak_exe_right = ppt_nop_tweak_exe_right,
	.recover_exe_right = ppt_nop_recover_exe_right,
};

void *ioremap(uint64_t start, uint64_t len)
{

	pgtable_add_map(ppt_mmu_pml4_addr, start, start, len, PAGE_DEV_FLAGS, &ppt_pgtable);

	return (void *)start;
}

uint64_t get_hv_ram_size(void)
{
	hv_ram_size = CONFIG_HV_RAM_SIZE;

	return hv_ram_size;
}

void enable_paging(void)
{
	mb();
	isb();

	/* Flush hypervisor TLB and I-cache*/
	asm volatile("tlbi alle2");
	asm volatile("ic iallu");
	mb();
	isb();

	asm volatile("msr TTBR0_EL2, %0" : : "r"(hva2hpa_early(ppt_mmu_pml4_addr)));
	isb();

	/* Flush hypervisor TLB and I-cache*/
	asm volatile("tlbi alle2");
	asm volatile("ic iallu");
	mb();
	isb();
}

/*
 * Clean USER bit in page table to update memory pages to be owned by hypervisor.
 */
void set_paging_supervisor(uint64_t base, uint64_t size)
{
	uint64_t base_aligned;
	uint64_t size_aligned;
	uint64_t region_end = base + size;

	/*rounddown base to 2MBytes aligned.*/
	base_aligned = round_pde_down(base);
	size_aligned = region_end - base_aligned;

	pgtable_modify_or_del_map((uint64_t *)ppt_mmu_pml4_addr, base_aligned,
							  round_pde_up(size_aligned), 0UL, PAGE_RAM_FLAGS, &ppt_pgtable, MR_MODIFY);
}

void allocate_ppt_pages(void)
{
	uint64_t page_base;

	page_base = e820_alloc_hv_memory(sizeof(struct page) * get_ppt_page_num());
	memset((void *)page_base, 0, get_ppt_page_num());
	ppt_page_pool.bitmap = (uint64_t *)e820_alloc_hv_memory(get_ppt_page_num() / 8);
	memset((void *)ppt_page_pool.bitmap, 0, get_ppt_page_num() / 8);

	ppt_page_pool.start_page = (struct page *)(void *)page_base;
	ppt_page_pool.bitmap_size = get_ppt_page_num() / 64;
	ppt_page_pool.dummy_page = NULL;
	ppt_page_pool.last_hint_id = 0UL;
}

void init_paging(void)
{
	uint32_t i;
	const struct e820_entry *initial_hv_e820;

	pr_info("HV MMU Initialization");

	initial_hv_e820 = get_initial_e820_entry();

	init_sanitized_page((uint64_t *)sanitized_page, hva2hpa_early(sanitized_page));

	ppt_mmu_pml4_addr = pgtable_create_root(&ppt_pgtable);

	for (i = 0; i < E820_MAX_ENTRIES; i++) {
		if (!initial_hv_e820[i].length || !initial_hv_e820[i].length) {
			break;
		}
		switch (initial_hv_e820[i].type) {
		case E820_TYPE_RAM:
			dev_dbg(DBG_LEVEL_MMU, "[%u] address %#lx is E820_TYPE_RAM",
					i, initial_hv_e820[i].baseaddr);
			pgtable_add_map(ppt_mmu_pml4_addr, initial_hv_e820[i].baseaddr,
							initial_hv_e820[i].baseaddr, initial_hv_e820[i].length,
							PAGE_RAM_FLAGS, &ppt_pgtable);
			break;
		case E820_TYPE_RESERVED:
			dev_dbg(DBG_LEVEL_MMU, "[%u] address %#lx is E820_TYPE_RESERVED",
					i, initial_hv_e820[i].baseaddr);
			break;
		case E820_TYPE_DEV:
			dev_dbg(DBG_LEVEL_MMU, "[%u] address %#lx is E820_TYPE_DEV",
					i, initial_hv_e820[i].baseaddr);
			pgtable_add_map(ppt_mmu_pml4_addr, initial_hv_e820[i].baseaddr,
							initial_hv_e820[i].baseaddr, initial_hv_e820[i].length,
							PAGE_DEV_FLAGS, &ppt_pgtable);
			break;
		case E820_TYPE_UNUSABLE:
			dev_dbg(DBG_LEVEL_MMU, "[%u] address %#lx is E820_TYPE_UNUSABLE",
					i, initial_hv_e820[i].baseaddr);
			break;
		default:
			pr_err("[%u] address %#lx is unknown type!",
				   i, initial_hv_e820[i].baseaddr);
			break;
		}
	}

	/* Enable paging */
	enable_paging();
}

/* Flush TLB of all processors in the inner-shareable domain for address addr */
void flush_tlb(uint64_t addr)
{
	asm volatile("tlbi vae2is, %0;" : : "r"(addr>>PAGE_SHIFT) : "memory");
}

void flush_tlb_range(uint64_t addr, uint64_t size)
{
	uint64_t linear_addr;

	for (linear_addr = addr; linear_addr < (addr + size); linear_addr += PAGE_SIZE) {
		flush_tlb(linear_addr);
	}
}

/* Invalidate all instruction caches in the inner-shareable domain domain */
void invalidate_icache(void)
{
	asm volatile("ic ialluis");
	dsbish();
	isb();
}

void invalidate_cacheline(const volatile void *p)
{
	asm volatile("dc ivac, %0;" : : "r"(p));
}

/* flush = clean + invalidate */
void flush_cacheline(const volatile void *p)
{
	asm volatile(\
				 "isb;" \
				 "dsb	sy;" \
				 "dc	civac, %0;" \
				 "dsb	sy;" \
				 : : "r"(p));
}

void flush_cache_range(const volatile void *p, uint64_t size)
{
	uint64_t i;

	for (i = 0UL; i < size; i += CACHE_LINE_SIZE) {
		flush_cacheline(p + i);
	}
}
