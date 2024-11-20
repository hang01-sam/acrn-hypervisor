/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/guest/ept.h>
#include <logmsg.h>
#include <trace.h>
#include <asm/e820.h>

#define DBG_LEVEL_EPT	6U

/* EPT address space will not beyond the platform physical address space */
#define EPT_PML4_PAGE_NUM	PML4_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)
#define EPT_PDPT_PAGE_NUM	PDPT_PAGE_NUM(MAX_PHY_ADDRESS_SPACE)

/* get_ept_page_num
 * Currently aarch64 support only IPA=PA
 */
static uint64_t get_ept_page_num(void)
{
	/* Currently aarch64 support only IPA=PA 1024 is enough for now. */
	return 1024; /* reserved page numbers */
}

uint64_t get_total_ept_4k_pages_size(void)
{
	return CONFIG_MAX_VM_NUM * (get_ept_page_num()) * PAGE_SIZE;
}

static struct page *ept_pages[CONFIG_MAX_VM_NUM];
static uint64_t *ept_page_bitmap[CONFIG_MAX_VM_NUM];
static struct page ept_dummy_pages[CONFIG_MAX_VM_NUM];

/* ept: extended page pool*/
static struct page_pool ept_page_pool[CONFIG_MAX_VM_NUM];

static void reserve_ept_bitmap(void)
{
	uint32_t i;
	uint64_t bitmap_base;
	uint64_t bitmap_size;
	uint64_t bitmap_offset;

	bitmap_size = (get_ept_page_num() * CONFIG_MAX_VM_NUM) / 8;
	bitmap_offset = get_ept_page_num() / 8;

	bitmap_base = e820_alloc_hv_memory(bitmap_size);

	for (i = 0; i < CONFIG_MAX_VM_NUM; i++) {
		ept_page_bitmap[i] = (uint64_t *)(void *)(bitmap_base + bitmap_offset * i);
	}
}

/*
 * @brief Reserve space for EPT 4K pages from platform E820 table
 */
void reserve_buffer_for_ept_pages(void)
{
	uint64_t page_base;
	uint16_t vm_id;
	uint32_t offset = 0U;

	page_base = e820_alloc_hv_memory(get_total_ept_4k_pages_size());
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		ept_pages[vm_id] = (struct page *)(void *)(page_base + offset);
		/* assume each VM has same amount of EPT pages */
		offset += get_ept_page_num() * PAGE_SIZE;
	}

	reserve_ept_bitmap();
}

/* @pre: The PPT and EPT have same page granularity */
static inline bool ept_large_page_support(enum _page_table_level level, __unused uint64_t prot)
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

/*
 * Pages without execution right, such as MMIO, can always use large page
 * base on hardware capability, even if the VM is an RTVM. This can save
 * page table page # and improve TLB hit rate.
 */
static inline bool use_large_page(enum _page_table_level level, uint64_t prot)
{
	bool ret = false;	/* for code page */

	if ((prot & EPT_NOEXE) != 0UL) {
		ret = ept_large_page_support(level, prot);
	}

	return ret;
}

static inline void ept_clflush_pagewalk(const void *etry)
{
	flush_cache_range(etry, sizeof(uint64_t));
}

static inline void ept_nop_tweak_exe_right(__unused uint64_t *entry) {}
static inline void ept_nop_recover_exe_right(__unused uint64_t *entry) {}

/* The function is used to disable execute right for (2MB / 1GB)large pages in EPT */
static inline void ept_tweak_exe_right(uint64_t *entry)
{
	*entry |= EPT_NOEXE;
}

/* The function is used to recover the execute right when large pages are breaking into 4KB pages
 * Hypervisor doesn't control execute right for guest memory, recovers execute right by default.
 */
static inline void ept_recover_exe_right(uint64_t *entry)
{
	*entry &= EPT_EXE;
}

static bool is_ept_force_4k_ipage(void)
{
	return false;
}

void init_ept_pgtable(struct pgtable *table, uint16_t vm_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(vm_id);

	ept_page_pool[vm_id].start_page = ept_pages[vm_id];
	ept_page_pool[vm_id].bitmap_size = get_ept_page_num() / 64;
	ept_page_pool[vm_id].bitmap = ept_page_bitmap[vm_id];
	ept_page_pool[vm_id].dummy_page = &ept_dummy_pages[vm_id];

	spinlock_init(&ept_page_pool[vm_id].lock);
	memset((void *)ept_page_pool[vm_id].bitmap, 0, ept_page_pool[vm_id].bitmap_size * sizeof(uint64_t));
	ept_page_pool[vm_id].last_hint_id = 0UL;

	table->pool = &ept_page_pool[vm_id];
	table->default_access_right = EPT_TABLE;
	table->pgentry_present_mask = EPT_PRESENT;
	table->clflush_pagewalk = ept_clflush_pagewalk;
	table->large_page_support = ept_large_page_support;


	/* Mitigation for issue "Machine Check Error on Page Size Change" */
	if (is_ept_force_4k_ipage()) {
		table->tweak_exe_right = ept_tweak_exe_right;
		table->recover_exe_right = ept_recover_exe_right;
		/* For RTVM, build 4KB page mapping in EPT for code pages */
		if (is_rt_vm(vm)) {
			table->large_page_support = use_large_page;
		}
	} else {
		table->tweak_exe_right = ept_nop_tweak_exe_right;
		table->recover_exe_right = ept_nop_recover_exe_right;
	}
}
/*
 * To enable the identical map and support of legacy devices/ACPI method in Service VM,
 * ACRN presents the entire host 0-4GB memory region to Service VM, except the memory
 * regions explicitly assigned to pre-launched VMs or HV (DRAM and MMIO). However,
 * virtual e820 only contains the known DRAM regions. For this reason,
 * we can't know if the GPA range is guest valid or not, by checking with
 * its ve820 tables only.
 *
 * instead, we Check if the GPA range is guest valid by whether the GPA range is mapped
 * in EPT pagetable or not
 */
bool ept_is_valid_mr(struct acrn_vm *vm, uint64_t mr_base_gpa, uint64_t mr_size)
{
	bool present = true;
	uint32_t sz;
	uint64_t end = mr_base_gpa + mr_size, address = mr_base_gpa;

	while (address < end) {
		if (local_gpa2hpa(vm, address, &sz) == INVALID_HPA) {
			present = false;
			break;
		}
		address += sz;
	}

	return present;
}

void destroy_ept(struct acrn_vm *vm)
{
	if (vm->arch_vm.nworld_eptp != NULL) {
		(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
	}
}

/**
 * @pre: vm != NULL.
 */
uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size)
{
	/* using return value INVALID_HPA as error code */
	uint64_t hpa = INVALID_HPA;
	const uint64_t *pgentry;
	uint64_t pg_size = 0UL;
	void *eptp;

	eptp = get_eptp(vm);
	pgentry = pgtable_lookup_entry((uint64_t *)eptp, gpa, &pg_size, &vm->arch_vm.ept_pgtable);
	if (pgentry != NULL) {
		hpa = (((*pgentry & (~EPT_PFN_HIGH_MASK)) & (~(pg_size - 1UL)))
			   | (gpa & (pg_size - 1UL)));
	}

	/**
	 * If specified parameter size is not NULL and
	 * the HPA of parameter gpa is found, pg_size shall
	 * be returned through parameter size.
	 */
	if ((size != NULL) && (hpa != INVALID_HPA)) {
		*size = (uint32_t)pg_size;
	}

	return hpa;
}

/* using return value INVALID_HPA as error code */
uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa)
{
	return local_gpa2hpa(vm, gpa, NULL);
}

/**
 * @pre: the gpa and hpa are identical mapping in Service VM.
 */
uint64_t service_vm_hpa2gpa(uint64_t hpa)
{
	return hpa;
}

static inline void ept_flush_guest(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	/* Here doesn't do the real flush, just makes the request which will be handled before vcpu vmenter */
	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

void ept_add_mr(struct acrn_vm *vm, uint64_t *pml4_page,
				uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig)
{
	uint64_t prot = prot_orig;

	dev_dbg(DBG_LEVEL_EPT, "%s, vm[%d] hpa: 0x%016lx gpa: 0x%016lx size: 0x%016lx prot: 0x%016x\n",
			__func__, vm->vm_id, hpa, gpa, size, prot);

	spinlock_obtain(&vm->ept_lock);

	pgtable_add_map(pml4_page, hpa, gpa, size, prot, &vm->arch_vm.ept_pgtable);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}

void ept_modify_mr(struct acrn_vm *vm, uint64_t *pml4_page,
				   uint64_t gpa, uint64_t size,
				   uint64_t prot_set, uint64_t prot_clr)
{
	uint64_t local_prot = prot_set;

	dev_dbg(DBG_LEVEL_EPT, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	spinlock_obtain(&vm->ept_lock);

	pgtable_modify_or_del_map(pml4_page, gpa, size, local_prot, prot_clr, &(vm->arch_vm.ept_pgtable), MR_MODIFY);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}
/**
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void ept_del_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa, uint64_t size)
{
	dev_dbg(DBG_LEVEL_EPT, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	spinlock_obtain(&vm->ept_lock);

	pgtable_modify_or_del_map(pml4_page, gpa, size, 0UL, 0UL, &(vm->arch_vm.ept_pgtable), MR_DEL);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}

/**
 * @pre: vm != NULL.
 */
void *get_eptp(struct acrn_vm *vm)
{
	void *eptp;
	struct acrn_vcpu *vcpu = vcpu_from_pid(vm, get_pcpu_id());

	if ((vcpu != NULL) && (vcpu->arch.cur_context == SECURE_WORLD)) {
		eptp = vm->arch_vm.sworld_eptp;
	} else {
		eptp = vm->arch_vm.nworld_eptp;
	}

	return eptp;
}

/**
 * @pre vm != NULL && cb != NULL.
 */
void walk_ept_table(struct acrn_vm *vm, pge_handler cb)
{
	const struct pgtable *table = &vm->arch_vm.ept_pgtable;
	uint64_t *pml4e, *pdpte, *pde, *pte;
	uint64_t i, j, k, m;

	for (i = 0UL; i < PTRS_PER_PML4E; i++) {
		pml4e = pml4e_offset((uint64_t *)get_eptp(vm), i << PML4E_SHIFT);
		if (!pgentry_present(table, (*pml4e))) {
			continue;
		}
		for (j = 0UL; j < PTRS_PER_PDPTE; j++) {
			pdpte = pdpte_offset(pml4e, j << PDPTE_SHIFT);
			if (!pgentry_present(table, (*pdpte))) {
				continue;
			}
			if (pdpte_large(*pdpte) != 0UL) {
				cb(pdpte, PDPTE_SIZE);
				continue;
			}
			for (k = 0UL; k < PTRS_PER_PDE; k++) {
				pde = pde_offset(pdpte, k << PDE_SHIFT);
				if (!pgentry_present(table, (*pde))) {
					continue;
				}
				if (pde_large(*pde) != 0UL) {
					cb(pde, PDE_SIZE);
					continue;
				}
				for (m = 0UL; m < PTRS_PER_PTE; m++) {
					pte = pte_offset(pde, m << PTE_SHIFT);
					if (pgentry_present(table, (*pte))) {
						cb(pte, PTE_SIZE);
					}
				}
			}
			/*
			 * Walk through the whole page tables of one VM is a time-consuming
			 * operation. Preemption is not support by hypervisor scheduling
			 * currently, so the walk through page tables operation might occupy
			 * CPU for long time what starve other threads.
			 *
			 * Give chance to release CPU to make other threads happy.
			 */
			if (need_reschedule(get_pcpu_id())) {
				schedule();
			}
		}
	}
}
