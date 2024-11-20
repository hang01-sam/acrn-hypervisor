/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file pgtable.h
 *
 * @brief Address translation and page table operations
 */
#ifndef __AARCH64_PGTABLE_H__
#define __AARCH64_PGTABLE_H__

#include <asm/page.h>

#define PAGE_TYPE_MSK (0x3)
#define PAGE_PRESENT (1UL << 0U)
/* translation table level 0-level 2 block descriptor */
#define PAGE_BLOCK (0x1)
/* translation table level 0-level 2 table descriptor */
#define PAGE_TABLE (0x3)
/* translation table level 3 descriptor */
#define PAGE_PAGE (0x3)

#define PAGE_nG           (1UL << 11U)
#define PAGE_AF           (1UL << 10U)
#define PAGE_SH_SHIFT     (8U)
#define PAGE_SH_MSK       (0x3UL << PAGE_SH_SHIFT)
#define PAGE_SH_OUTER     (0x2UL << PAGE_SH_SHIFT) /* outer shareable */
#define PAGE_SH_INNER     (0x3UL << PAGE_SH_SHIFT) /* inner shareable */
#define PAGE_AP_SHIFT     (6U)
#define PAGE_AP_MSK       (0x3UL << PAGE_AP_SHIFT)
#define PAGE_AP_RW        (0x1UL << PAGE_AP_SHIFT)
#define PAGE_AP_RO        (0x3UL << PAGE_AP_SHIFT)
#define PAGE_NS           (1UL << 5U)
/*
 Stage 1 Memory Type.
 the AttrIndx[2:0] field in a block or page translation table descriptor
 for a stage 1 translation indicates the 8-bit field in the MAIR_ELx that specifies the attributes for the corresponding
 memory region.
 In head.S, MAIR_EL2 define 4 type attributes
 attr[0] = 0x00(Device-nGnRnE), attr[1] = 0x44(Normal-Non-Cacheable)
 attr[4] = 0x04(Device-nGnRE), attr[5] = 0xff(Normal-Write-Back-RW allocate)
*/
#define PAGE_ATTR_SHIFT    (2U)
/* Device-nGnRE */
#define PAGE_DEVICE_ATTR   (0x4UL << PAGE_ATTR_SHIFT)
/* Normal-Write-Back-RW allocate */
#define PAGE_NORMAL_ATTR   (0x5UL << PAGE_ATTR_SHIFT)

/* stage 1 translation flages for hypervisor */
#define PAGE_RAM_FLAGS (PAGE_NORMAL_ATTR | PAGE_NS | PAGE_AP_RW | PAGE_SH_INNER | PAGE_AF | PAGE_nG)
#define PAGE_DEV_FLAGS (PAGE_DEVICE_ATTR | PAGE_NS | PAGE_AP_RW | PAGE_SH_INNER | PAGE_AF)


/**
 * @defgroup ept_mem_access_right EPT Memory Access Right
 *
 * This is a group that includes EPT Memory Access Right Definitions.
 *
 * @{
 */

/**
 * @brief EPT memory access right is read-only.
 */
#define EPT_RD			(0x1UL << PAGE_AP_SHIFT)

/**
 * @brief EPT memory access right is write-only.
 */

#define EPT_WD			(0x2UL << PAGE_AP_SHIFT)

/**
 * @brief EPT memory access right is read/write.
 */
#define EPT_WR			(0x3UL << PAGE_AP_SHIFT)

/**
 * @brief EPT memory access right is executable.
 * XN[1:0], bits[54:53]
   The Execute-never field, see Access permissions for instruction execution on page D5-2606.
   If FEAT_XNX is not implemented, bit[53] is RES0
 **/
#define EPT_XN_OFF              (53U)
#define EPT_NOEXE               (0x3UL << EPT_XN_OFF)
#define EPT_EXE                 (0x0UL << EPT_XN_OFF)

/**
 * @brief EPT memory access right is read/write and executable.
 */
#define EPT_RWX			(EPT_RD | EPT_WR & EPT_EXE)

#define EPT_PRESENT		(1UL << 0U)
#define EPT_TABLE		(3UL << 0U)


/**
 * @}
 */
/* End of ept_mem_access_right */

/**
 * @defgroup ept_mem_type EPT Memory Type
 *
 * This is a group that includes EPT Memory Type Definitions.
 *
 * @{
 */
/* Stage 2 fields */
/*
 * Stage 2 Memory Type.
 *
 * These are valid in the MemAttr[3:0] field of an LPAE stage 2 page
 * table entry.
 *
 */


#define EPT_MEMATTR_OFF (2)
#define EPT_MATTR_DEV     (0x1 << EPT_MEMATTR_OFF)
#define EPT_MATTR_MEM_NC  (0x5 << EPT_MEMATTR_OFF)
#define EPT_MATTR_MEM     (0xf << EPT_MEMATTR_OFF)

#define EPT_MEMATTR_DEV_nGnRnE ((0x00 << 0) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_DEV_nGnRE ((0x01 << 0) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_DEV_nGRE ((0x02 << 0) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_DEV_GRE ((0x03 << 0) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_NRML_ONC ((0x01 << 2) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_NRML_OWTC ((0x02 << 2) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_NRML_OWBC ((0x03 << 2) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_NRML_INC ((0x01 << 0) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_NRML_IWTC ((0x02 << 0) << EPT_MEMATTR_OFF)
#define EPT_MEMATTR_NRML_IWBC ((0x03 << 0) << EPT_MEMATTR_OFF)

/**
 * @brief EPT memory type is specified in bits 5:2 of the EPT paging-structure entry.
 */
#define EPT_MT_SHIFT		2U

/**
 * @brief EPT memory type is uncacheable.
 */
#define EPT_UNCACHED		(1UL << EPT_MT_SHIFT)

/**
 * @brief EPT memory type is write through.
 */
#define EPT_WT			(2UL << EPT_MT_SHIFT)

/**
 * @brief EPT memory type is write back.
 */
#define EPT_WB			(3UL << EPT_MT_SHIFT)

/**
 * @}
 */
/* End of ept_mem_type */

#define EPT_VM_FLAGS     \
    (EPT_MATTR_MEM | PAGE_SH_INNER | EPT_WR | PAGE_AF)

#define EPT_VM_FLAGS_NC     \
    (EPT_MATTR_MEM_NC | PAGE_SH_INNER | EPT_WR | PAGE_AF)


#define EPT_VM_DEV_FLAGS \
    (EPT_MEMATTR_DEV_GRE | PAGE_SH_OUTER | EPT_WR | PAGE_AF | EPT_NOEXE)

#define EPT_MT_MASK		(7UL << EPT_MT_SHIFT)
#define EPT_VE			(1UL << 63U)
/* EPT leaf entry bits (bit 52 - bit 63) should be maksed  when calculate PFN */
#define EPT_PFN_HIGH_MASK	0xFFF0000000000000UL

#define PML4E_SHIFT		39U
#define PTRS_PER_PML4E		512UL
#define PML4E_SIZE		(1UL << PML4E_SHIFT)
#define PML4E_MASK		(~(PML4E_SIZE - 1UL))

#define PDPTE_SHIFT		30U
#define PTRS_PER_PDPTE		512UL
#define PDPTE_SIZE		(1UL << PDPTE_SHIFT)
#define PDPTE_MASK		(~(PDPTE_SIZE - 1UL))

#define PDE_SHIFT		21U
#define PTRS_PER_PDE		512UL
#define PDE_SIZE		(1UL << PDE_SHIFT)
#define PDE_MASK		(~(PDE_SIZE - 1UL))

#define PTE_SHIFT		12U
#define PTRS_PER_PTE		512UL
#define PTE_SIZE		(1UL << PTE_SHIFT)
#define PTE_MASK		(~(PTE_SIZE - 1UL))

/* TODO: PAGE_MASK & PHYSICAL_MASK */
#define PML4E_PFN_MASK		0x0000FFFFFFFFF000UL
#define PDPTE_PFN_MASK		0x0000FFFFFFFFF000UL
#define PDE_PFN_MASK		0x0000FFFFFFFFF000UL

#define EPT_ENTRY_PFN_MASK	((~EPT_PFN_HIGH_MASK) & PAGE_MASK)

/**
 * @brief Page tables level in IA32 paging mode
 */
enum _page_table_level {
	/**
	 * @brief The PML4 level in the page tables
	 */
	PGT_PML4 = 0,
	/**
	 * @brief The Page-Directory-Pointer-Table level in the page tables
	 */
	PGT_PDPT = 1,
	/**
	 * @brief The Page-Directory level in the page tables
	 */
	PGT_PD = 2,
	/**
	 * @brief The Page-Table level in the page tables
	 */
	PGT_PT = 3,
};

struct pgtable {
	uint64_t default_access_right;
	uint64_t pgentry_present_mask;
	struct page_pool *pool;
	bool (*large_page_support)(enum _page_table_level level, uint64_t prot);
	void (*clflush_pagewalk)(const void *p);
	void (*tweak_exe_right)(uint64_t *entry);
	void (*recover_exe_right)(uint64_t *entry);
};

static inline bool pgentry_present(const struct pgtable *table, uint64_t pte)
{
	return ((table->pgentry_present_mask & (pte)) != 0UL);
}

/**
 * @brief Address space translation
 *
 * @addtogroup acrn_mem ACRN Memory Management
 * @{
 */
/* hpa <--> hva, now it is 1:1 mapping */
/**
 * @brief Translate host-physical address to host-virtual address
 *
 * @param[in] x The specified host-physical address
 *
 * @return The translated host-virtual address
 */
static inline void *hpa2hva_early(uint64_t x)
{
	return (void *)x;
}
/**
 * @brief Translate host-virtual address to host-physical address
 *
 * @param[in] x The specified host-virtual address
 *
 * @return The translated host-physical address
 */
static inline uint64_t hva2hpa_early(void *x)
{
	return (uint64_t)x;
}

/**
 * @brief Translate host-physical address to host-virtual address
 *
 * @param[in] x The specified host-physical address
 *
 * @return The translated host-virtual address
 */
static inline void *hpa2hva(uint64_t x)
{
	return (void *)x;
}
/**
 * @brief Translate host-virtual address to host-physical address
 *
 * @param[in] x The specified host-virtual address
 *
 * @return The translated host-physical address
 */
static inline uint64_t hva2hpa(const void *x)
{
	return (uint64_t)x;
}

static inline uint64_t pml4e_index(uint64_t address)
{
	return (address >> PML4E_SHIFT) & (PTRS_PER_PML4E - 1UL);
}

static inline uint64_t pdpte_index(uint64_t address)
{
	return (address >> PDPTE_SHIFT) & (PTRS_PER_PDPTE - 1UL);
}

static inline uint64_t pde_index(uint64_t address)
{
	return (address >> PDE_SHIFT) & (PTRS_PER_PDE - 1UL);
}

static inline uint64_t pte_index(uint64_t address)
{
	return (address >> PTE_SHIFT) & (PTRS_PER_PTE - 1UL);
}

static inline uint64_t *pml4e_page_vaddr(uint64_t pml4e)
{
	return hpa2hva(pml4e & PML4E_PFN_MASK);
}

static inline uint64_t *pdpte_page_vaddr(uint64_t pdpte)
{
	return hpa2hva(pdpte & PDPTE_PFN_MASK);
}

static inline uint64_t *pde_page_vaddr(uint64_t pde)
{
	return hpa2hva(pde & PDE_PFN_MASK);
}

static inline uint64_t *pml4e_offset(uint64_t *pml4_page, uint64_t addr)
{
	return pml4_page + pml4e_index(addr);
}

static inline uint64_t *pdpte_offset(const uint64_t *pml4e, uint64_t addr)
{
	return pml4e_page_vaddr(*pml4e) + pdpte_index(addr);
}

static inline uint64_t *pde_offset(const uint64_t *pdpte, uint64_t addr)
{
	return pdpte_page_vaddr(*pdpte) + pde_index(addr);
}

static inline uint64_t *pte_offset(const uint64_t *pde, uint64_t addr)
{
	return pde_page_vaddr(*pde) + pte_index(addr);
}

/*
 * pgentry may means pml4e/pdpte/pde/pte
 */
static inline uint64_t get_pgentry(const uint64_t *pte)
{
	return *pte;
}

/*
 * pgentry may means pml4e/pdpte/pde/pte
 */
static inline void set_pgentry(uint64_t *ptep, uint64_t pte, const struct pgtable *table)
{
	*ptep = pte;
	table->clflush_pagewalk(ptep);
}

static inline uint64_t pde_large(uint64_t pde)
{
	return (pde & PAGE_TYPE_MSK) == PAGE_BLOCK;
}

static inline uint64_t pdpte_large(uint64_t pdpte)
{
	return (pdpte  & PAGE_TYPE_MSK) == PAGE_BLOCK;
}

void init_sanitized_page(uint64_t *sanitized_page, uint64_t hpa);

void *pgtable_create_root(const struct pgtable *table);
void *pgtable_create_trusty_root(const struct pgtable *table,
								 void *nworld_pml4_page, uint64_t prot_table_present, uint64_t prot_clr);
/**
 *@pre (pml4_page != NULL) && (pg_size != NULL)
 */
const uint64_t *pgtable_lookup_entry(uint64_t *pml4_page, uint64_t addr,
									 uint64_t *pg_size, const struct pgtable *table);

void pgtable_add_map(uint64_t *pml4_page, uint64_t paddr_base,
					 uint64_t vaddr_base, uint64_t size,
					 uint64_t prot, const struct pgtable *table);
void pgtable_modify_or_del_map(uint64_t *pml4_page, uint64_t vaddr_base,
							   uint64_t size, uint64_t prot_set, uint64_t prot_clr,
							   const struct pgtable *table, uint32_t type);
/**
 * @}
 */
#endif /* __AARCH64_PGTABLE_H__ */
