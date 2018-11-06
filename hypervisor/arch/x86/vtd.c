/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define pr_prefix		"iommu: "

#include <hypervisor.h>

#define DBG_IOMMU 0

#if DBG_IOMMU
#define ACRN_DBG_IOMMU LOG_INFO
#define DMAR_FAULT_LOOP_MAX 10
#else
#define ACRN_DBG_IOMMU 6U
#endif

#define LEVEL_WIDTH 9U

#define ROOT_ENTRY_LOWER_PRESENT_POS        (0U)
#define ROOT_ENTRY_LOWER_PRESENT_MASK       (1UL)
#define ROOT_ENTRY_LOWER_CTP_POS            (12U)
#define ROOT_ENTRY_LOWER_CTP_MASK           (0xFFFFFFFFFFFFFUL)

/* 4 iommu fault register state */
#define	IOMMU_FAULT_REGISTER_STATE_NUM	4U
#define	IOMMU_FAULT_REGISTER_SIZE	4U

#define CTX_ENTRY_UPPER_AW_POS          (0U)
#define CTX_ENTRY_UPPER_AW_MASK         \
		(0x7UL << CTX_ENTRY_UPPER_AW_POS)
#define CTX_ENTRY_UPPER_DID_POS         (8U)
#define CTX_ENTRY_UPPER_DID_MASK        \
		(0x3FUL << CTX_ENTRY_UPPER_DID_POS)
#define CTX_ENTRY_LOWER_P_POS           (0U)
#define CTX_ENTRY_LOWER_P_MASK          \
		(0x1UL << CTX_ENTRY_LOWER_P_POS)
#define CTX_ENTRY_LOWER_FPD_POS         (1U)
#define CTX_ENTRY_LOWER_FPD_MASK        \
		(0x1UL << CTX_ENTRY_LOWER_FPD_POS)
#define CTX_ENTRY_LOWER_TT_POS          (2U)
#define CTX_ENTRY_LOWER_TT_MASK         \
		(0x3UL << CTX_ENTRY_LOWER_TT_POS)
#define CTX_ENTRY_LOWER_SLPTPTR_POS     (12U)
#define CTX_ENTRY_LOWER_SLPTPTR_MASK    \
		(0xFFFFFFFFFFFFFUL <<  CTX_ENTRY_LOWER_SLPTPTR_POS)

static inline uint64_t
dmar_get_bitslice(uint64_t var, uint64_t mask, uint32_t pos)
{
	return ((var & mask) >> pos);
}

static inline uint64_t
dmar_set_bitslice(uint64_t var, uint64_t mask,
		  uint32_t pos, uint64_t val)
{
	return ((var & ~mask) | ((val << pos) & mask));
}

/* translation type */
#define DMAR_CTX_TT_UNTRANSLATED    0x0UL
#define DMAR_CTX_TT_ALL             0x1UL
#define DMAR_CTX_TT_PASSTHROUGH     0x2UL

/* Fault event MSI data register */
#define DMAR_MSI_DELIVERY_MODE_SHIFT     (8U)
#define DMAR_MSI_DELIVERY_FIXED          (0U << DMAR_MSI_DELIVERY_MODE_SHIFT)
#define DMAR_MSI_DELIVERY_LOWPRI         (1U << DMAR_MSI_DELIVERY_MODE_SHIFT)

/* Fault event MSI address register */
#define DMAR_MSI_DEST_MODE_SHIFT         (2U)
#define DMAR_MSI_DEST_MODE_PHYS          (0U << DMAR_MSI_DEST_MODE_SHIFT)
#define DMAR_MSI_DEST_MODE_LOGIC         (1U << DMAR_MSI_DEST_MODE_SHIFT)
#define DMAR_MSI_REDIRECTION_SHIFT       (3U)
#define DMAR_MSI_REDIRECTION_CPU         (0U << DMAR_MSI_REDIRECTION_SHIFT)
#define DMAR_MSI_REDIRECTION_LOWPRI      (1U << DMAR_MSI_REDIRECTION_SHIFT)

enum dmar_cirg_type {
	DMAR_CIRG_RESERVED = 0,
	DMAR_CIRG_GLOBAL,
	DMAR_CIRG_DOMAIN,
	DMAR_CIRG_DEVICE
};

enum dmar_iirg_type {
	DMAR_IIRG_RESERVED = 0,
	DMAR_IIRG_GLOBAL,
	DMAR_IIRG_DOMAIN,
	DMAR_IIRG_PAGE
};

/* dmar unit runtime data */
struct dmar_drhd_rt {
	uint32_t index;
	spinlock_t lock;

	struct dmar_drhd *drhd;

	uint64_t root_table_addr;

	uint64_t cap;
	uint64_t ecap;
	uint32_t gcmd;  /* sw cache value of global cmd register */

	uint32_t dmar_irq;

	bool cap_pw_coherency;  /* page-walk coherency */
	uint8_t cap_msagaw;
	uint16_t cap_num_fault_regs;
	uint16_t cap_fault_reg_offset;
	uint16_t ecap_iotlb_offset;
	uint32_t fault_state[IOMMU_FAULT_REGISTER_STATE_NUM]; /* 32bit registers */
};

struct dmar_root_entry {
	uint64_t lower;
	uint64_t upper;
};

struct dmar_context_entry {
	uint64_t lower;
	uint64_t upper;
};

struct iommu_domain {
	bool is_host;
	bool is_tt_ept;     /* if reuse EPT of the domain */
	uint16_t vm_id;
	uint32_t addr_width;   /* address width of the domain */
	uint64_t trans_table_ptr;
};

struct context_table {
	struct page buses[CONFIG_IOMMU_INIT_BUS_LIMIT];
};

static struct page root_tables[CONFIG_MAX_IOMMU_NUM] __aligned(CPU_PAGE_SIZE);
static struct context_table ctx_tables[CONFIG_MAX_IOMMU_NUM] __aligned(CPU_PAGE_SIZE);

static inline uint8_t*
get_root_table(uint32_t dmar_index)
{
	return root_tables[dmar_index].contents;
}

static inline uint8_t*
get_ctx_table(uint32_t dmar_index, uint8_t bus_no)
{
	return ctx_tables[dmar_index].buses[bus_no].contents;
}

static struct dmar_drhd_rt dmar_drhd_units[CONFIG_MAX_IOMMU_NUM];

static struct iommu_domain *vm0_domain;

/* Domain id 0 is reserved in some cases per VT-d */
#define MAX_DOMAIN_NUM (CONFIG_MAX_IOMMU_NUM + 1)
static struct iommu_domain iommu_domains[MAX_DOMAIN_NUM];

static inline uint16_t vmid_to_domainid(uint16_t vm_id)
{
	return vm_id + 1U;
}

static int dmar_register_hrhd(struct dmar_drhd_rt *dmar_uint);
static struct dmar_drhd_rt *device_to_dmaru(uint16_t segment, uint8_t bus,
					   uint8_t devfun);
static int register_hrhd_units(void)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *drhd_rt;
	uint32_t i;
	int ret = 0;

	if (info == NULL || info->drhd_count == 0U) {
		pr_fatal("%s: can't find dmar info\n", __func__);
		return -ENODEV;
	}

	if (info->drhd_count > CONFIG_MAX_IOMMU_NUM) {
		pr_fatal("%s: dmar count(%d) beyond the limitation(%d)\n",
				__func__, info->drhd_count, CONFIG_MAX_IOMMU_NUM);
		return -EINVAL;
	}

	for (i = 0U; i < info->drhd_count; i++) {
		drhd_rt = &dmar_drhd_units[i];
		drhd_rt->index = i;
		drhd_rt->drhd = &info->drhd_units[i];
		drhd_rt->dmar_irq = IRQ_INVALID;
		ret = dmar_register_hrhd(drhd_rt);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static uint32_t iommu_read32(const struct dmar_drhd_rt *dmar_uint, uint32_t offset)
{
	return mmio_read32(hpa2hva(dmar_uint->drhd->reg_base_addr + offset));
}

static uint64_t iommu_read64(const struct dmar_drhd_rt *dmar_uint, uint32_t offset)
{
	uint64_t value;

	value = mmio_read32(hpa2hva(dmar_uint->drhd->reg_base_addr + offset +
									4U));
	value = value << 32U;
	value = value | mmio_read32(hpa2hva(dmar_uint->drhd->reg_base_addr +
					offset));

	return value;
}

static void iommu_write32(const struct dmar_drhd_rt *dmar_uint, uint32_t offset,
			  uint32_t value)
{
	mmio_write32(value, hpa2hva(dmar_uint->drhd->reg_base_addr + offset));
}

static void iommu_write64(const struct dmar_drhd_rt *dmar_uint, uint32_t offset,
			  uint64_t value)
{
	uint32_t temp;

	temp = (uint32_t)value;
	mmio_write32(temp, hpa2hva(dmar_uint->drhd->reg_base_addr + offset));

	temp = (uint32_t)(value >> 32U);
	mmio_write32(temp,
		hpa2hva(dmar_uint->drhd->reg_base_addr + offset + 4U));
}

static inline void
dmar_wait_completion(const struct dmar_drhd_rt *dmar_uint, uint32_t offset,
	uint32_t mask, bool pre_condition, uint32_t *status)
{
	/* variable start isn't used when built as release version */
	__unused uint64_t start = rdtsc();
	bool condition, temp_condition;

	while (1) {
		*status = iommu_read32(dmar_uint, offset);
		temp_condition = ((*status & mask) == 0U) ? true : false;

		/*
		 * pre_condition    temp_condition    | condition
		 * -----------------------------------|----------
		 * true             true              | true
		 * true             false             | false
		 * false            true              | false
		 * false            false             | true
		 */
		condition = (temp_condition == pre_condition) ? true : false;

		if (condition) {
			break;
		}
		ASSERT(((rdtsc() - start) < CYCLES_PER_MS),
			"DMAR OP Timeout!");
		asm volatile ("pause" ::: "memory");
	}
}

/* flush cache when root table, context table updated */
static void iommu_flush_cache(const struct dmar_drhd_rt *dmar_uint,
			      void *p, uint32_t size)
{
	uint32_t i;

	/* if vtd support page-walk coherency, no need to flush cacheline */
	if (iommu_ecap_c(dmar_uint->ecap) != 0U) {
		return;
	}

	for (i = 0U; i < size; i += CACHE_LINE_SIZE) {
		clflush((char *)p + i);
	}
}

#if DBG_IOMMU
static void dmar_uint_show_capability(struct dmar_drhd_rt *dmar_uint)
{
	pr_info("dmar unit[0x%x]", dmar_uint->drhd->reg_base_addr);
	pr_info("\tNumDomain:%d",
		iommu_cap_ndoms(dmar_uint->cap));
	pr_info("\tAdvancedFaultLogging:%d",
		iommu_cap_afl(dmar_uint->cap));
	pr_info("\tRequiredWBFlush:%d",
		iommu_cap_rwbf(dmar_uint->cap));
	pr_info("\tProtectedLowMemRegion:%d",
		iommu_cap_plmr(dmar_uint->cap));
	pr_info("\tProtectedHighMemRegion:%d",
		iommu_cap_phmr(dmar_uint->cap));
	pr_info("\tCachingMode:%d",
		iommu_cap_caching_mode(dmar_uint->cap));
	pr_info("\tSAGAW:0x%x",
		iommu_cap_sagaw(dmar_uint->cap));
	pr_info("\tMGAW:%d",
		iommu_cap_mgaw(dmar_uint->cap));
	pr_info("\tZeroLenRead:%d",
		iommu_cap_zlr(dmar_uint->cap));
	pr_info("\tLargePageSupport:0x%x",
		iommu_cap_super_page_val(dmar_uint->cap));
	pr_info("\tPageSelectiveInvalidation:%d",
		iommu_cap_pgsel_inv(dmar_uint->cap));
	pr_info("\tPageSelectInvalidation:%d",
		iommu_cap_pgsel_inv(dmar_uint->cap));
	pr_info("\tNumOfFaultRecordingReg:%d",
		iommu_cap_num_fault_regs(dmar_uint->cap));
	pr_info("\tMAMV:0x%x",
		iommu_cap_max_amask_val(dmar_uint->cap));
	pr_info("\tWriteDraining:%d",
		iommu_cap_write_drain(dmar_uint->cap));
	pr_info("\tReadDraining:%d",
		iommu_cap_read_drain(dmar_uint->cap));
	pr_info("\tPostInterrupts:%d\n",
		iommu_cap_pi(dmar_uint->cap));
	pr_info("\tPage-walk Coherency:%d",
		iommu_ecap_c(dmar_uint->ecap));
	pr_info("\tQueuedInvalidation:%d",
		iommu_ecap_qi(dmar_uint->ecap));
	pr_info("\tDeviceTLB:%d",
		iommu_ecap_dt(dmar_uint->ecap));
	pr_info("\tInterruptRemapping:%d",
		iommu_ecap_ir(dmar_uint->ecap));
	pr_info("\tExtendedInterruptMode:%d",
		iommu_ecap_eim(dmar_uint->ecap));
	pr_info("\tPassThrough:%d",
		iommu_ecap_pt(dmar_uint->ecap));
	pr_info("\tSnoopControl:%d",
		iommu_ecap_sc(dmar_uint->ecap));
	pr_info("\tIOTLB RegOffset:0x%x",
		iommu_ecap_iro(dmar_uint->ecap));
	pr_info("\tMHMV:0x%x", iommu_ecap_mhmv(dmar_uint->ecap));
	pr_info("\tECS:%d", iommu_ecap_ecs(dmar_uint->ecap));
	pr_info("\tMTS:%d", iommu_ecap_mts(dmar_uint->ecap));
	pr_info("\tNEST:%d", iommu_ecap_nest(dmar_uint->ecap));
	pr_info("\tDIS:%d", iommu_ecap_dis(dmar_uint->ecap));
	pr_info("\tPRS:%d", iommu_ecap_prs(dmar_uint->ecap));
	pr_info("\tERS:%d", iommu_ecap_ers(dmar_uint->ecap));
	pr_info("\tSRS:%d", iommu_ecap_srs(dmar_uint->ecap));
	pr_info("\tNWFS:%d", iommu_ecap_nwfs(dmar_uint->ecap));
	pr_info("\tEAFS:%d", iommu_ecap_eafs(dmar_uint->ecap));
	pr_info("\tPSS:0x%x", iommu_ecap_pss(dmar_uint->ecap));
	pr_info("\tPASID:%d", iommu_ecap_pasid(dmar_uint->ecap));
	pr_info("\tDIT:%d", iommu_ecap_dit(dmar_uint->ecap));
	pr_info("\tPDS:%d\n", iommu_ecap_pds(dmar_uint->ecap));
}
#endif

static inline uint8_t width_to_level(uint32_t width)
{
	return (uint8_t)(((width - 12U) + (LEVEL_WIDTH)-1U) / (LEVEL_WIDTH));
}

static inline uint8_t width_to_agaw(uint32_t width)
{
	return width_to_level(width) - 2U;
}

static uint8_t dmar_uint_get_msagw(const struct dmar_drhd_rt *dmar_uint)
{
	uint8_t i;
	uint8_t sgaw = iommu_cap_sagaw(dmar_uint->cap);

	for (i = 5U; i > 0U; ) {
		i--;
		if (((1U << i) & sgaw) != 0U) {
			break;
		}
	}
	return i;
}

static bool
dmar_unit_support_aw(const struct dmar_drhd_rt *dmar_uint, uint32_t addr_width)
{
	uint8_t aw;

	aw = width_to_agaw(addr_width);

	return (((1U << aw) & iommu_cap_sagaw(dmar_uint->cap)) != 0U);
}

static void dmar_enable_translation(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t status;

	spinlock_obtain(&(dmar_uint->lock));
	dmar_uint->gcmd |= DMA_GCMD_TE;
	iommu_write32(dmar_uint, DMAR_GCMD_REG, dmar_uint->gcmd);

	/* 32-bit register */
	dmar_wait_completion(dmar_uint, DMAR_GSTS_REG, DMA_GSTS_TES, false,
				&status);

	status = iommu_read32(dmar_uint, DMAR_GSTS_REG);

	spinlock_release(&(dmar_uint->lock));

	dev_dbg(ACRN_DBG_IOMMU, "%s: gsr:0x%x", __func__, status);
}

static void dmar_disable_translation(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t status;

	spinlock_obtain(&(dmar_uint->lock));
	dmar_uint->gcmd &= ~DMA_GCMD_TE;
	iommu_write32(dmar_uint, DMAR_GCMD_REG, dmar_uint->gcmd);

	/* 32-bit register */
	dmar_wait_completion(dmar_uint, DMAR_GSTS_REG, DMA_GSTS_TES, true,
				&status);

	spinlock_release(&(dmar_uint->lock));
}

static int dmar_register_hrhd(struct dmar_drhd_rt *dmar_uint)
{
	dev_dbg(ACRN_DBG_IOMMU, "Register dmar uint [%d] @0x%llx",
			dmar_uint->index,
			dmar_uint->drhd->reg_base_addr);

	spinlock_init(&dmar_uint->lock);

	dmar_uint->cap = iommu_read64(dmar_uint, DMAR_CAP_REG);
	dmar_uint->ecap = iommu_read64(dmar_uint, DMAR_ECAP_REG);
	dmar_uint->gcmd = iommu_read32(dmar_uint, DMAR_GCMD_REG);

	dmar_uint->cap_msagaw = dmar_uint_get_msagw(dmar_uint);

	dmar_uint->cap_num_fault_regs =
		iommu_cap_num_fault_regs(dmar_uint->cap);
	dmar_uint->cap_fault_reg_offset =
		iommu_cap_fault_reg_offset(dmar_uint->cap);
	dmar_uint->ecap_iotlb_offset = iommu_ecap_iro(dmar_uint->ecap) * 16U;

#if DBG_IOMMU
	pr_info("version:0x%x, cap:0x%llx, ecap:0x%llx",
		iommu_read32(dmar_uint, DMAR_VER_REG),
		dmar_uint->cap,
		dmar_uint->ecap);
	pr_info("sagaw:0x%x, msagaw:0x%x, iotlb offset 0x%x",
		iommu_cap_sagaw(dmar_uint->cap),
		dmar_uint->cap_msagaw,
		dmar_uint->ecap_iotlb_offset);

	dmar_uint_show_capability(dmar_uint);
#endif

	/* check capability */
	if ((iommu_cap_super_page_val(dmar_uint->cap) & 0x1U) == 0U) {
		pr_fatal("%s: dmar uint doesn't support 2MB page!\n", __func__);
		return -ENODEV;
	}

	if ((iommu_cap_super_page_val(dmar_uint->cap) & 0x2U) == 0U) {
		pr_fatal("%s: dmar uint doesn't support 1GB page!\n", __func__);
		return -ENODEV;
	}

	/* when the hardware support snoop control,
	 * to make sure snoop control is always enabled,
	 * the SNP filed in the leaf PTE should be set.
	 * How to guarantee it when EPT is used as second-level
	 * translation paging structures?
	 */
	if (iommu_ecap_sc(dmar_uint->ecap) == 0U) {
		dev_dbg(ACRN_DBG_IOMMU,
			"dmar uint doesn't support snoop control!");
	}

	if ((dmar_uint->gcmd & DMA_GCMD_TE) != 0U) {
		dmar_disable_translation(dmar_uint);
	}

	return 0;
}

static struct dmar_drhd_rt *device_to_dmaru(uint16_t segment, uint8_t bus,
					   uint8_t devfun)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_uint;
	uint32_t i, j;

	for (j = 0U; j < info->drhd_count; j++) {
		dmar_uint = &dmar_drhd_units[j];

		if (dmar_uint->drhd->segment != segment) {
			continue;
		}

		for (i = 0U; i < dmar_uint->drhd->dev_cnt; i++) {
			if ((dmar_uint->drhd->devices[i].bus == bus) &&
				(dmar_uint->drhd->devices[i].devfun == devfun)) {
				return dmar_uint;
			}
		}

		/* has the same segment number and
		 * the dmar unit has INCLUDE_PCI_ALL set
		 */
		if ((dmar_uint->drhd->flags & DRHD_FLAG_INCLUDE_PCI_ALL_MASK) != 0U) {
			return dmar_uint;
		}
	}

	return NULL;
}

static void dmar_write_buffer_flush(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t status;

	if (iommu_cap_rwbf(dmar_uint->cap) == 0U) {
		return;
	}

	spinlock_obtain(&(dmar_uint->lock));
	iommu_write32(dmar_uint, DMAR_GCMD_REG,
			  dmar_uint->gcmd | DMA_GCMD_WBF);

	/* read lower 32 bits to check */
	dmar_wait_completion(dmar_uint, DMAR_GSTS_REG, DMA_GSTS_WBFS, true,
				&status);
	spinlock_release(&(dmar_uint->lock));
}

/*
 * did: domain id
 * sid: source id
 * fm: function mask
 * cirg: cache-invalidation request granularity
 */
static void dmar_invalid_context_cache(struct dmar_drhd_rt *dmar_uint,
	uint16_t did, uint16_t sid, uint8_t fm, enum dmar_cirg_type cirg)
{
	uint64_t cmd = DMA_CCMD_ICC;
	uint32_t status;

	switch (cirg) {
	case DMAR_CIRG_GLOBAL:
		cmd |= DMA_CCMD_GLOBAL_INVL;
		break;
	case DMAR_CIRG_DOMAIN:
		cmd |= DMA_CCMD_DOMAIN_INVL | dma_ccmd_did(did);
		break;
	case DMAR_CIRG_DEVICE:
		cmd |= DMA_CCMD_DEVICE_INVL | dma_ccmd_did(did) |
			   dma_ccmd_sid(sid) | dma_ccmd_fm(fm);
		break;
	default:
		pr_err("unknown CIRG type");
		return;
	}

	spinlock_obtain(&(dmar_uint->lock));
	iommu_write64(dmar_uint, DMAR_CCMD_REG, cmd);
	/* read upper 32bits to check */
	dmar_wait_completion(dmar_uint, DMAR_CCMD_REG + 4U, DMA_CCMD_ICC_32,
				true, &status);

	spinlock_release(&(dmar_uint->lock));

	dev_dbg(ACRN_DBG_IOMMU, "cc invalidation granularity %d",
		dma_ccmd_get_caig_32(status));
}

static void dmar_invalid_context_cache_global(struct dmar_drhd_rt *dmar_uint)
{
	dmar_invalid_context_cache(dmar_uint, 0U, 0U, 0U, DMAR_CIRG_GLOBAL);
}

static void dmar_invalid_iotlb(struct dmar_drhd_rt *dmar_uint,
				   uint16_t did, uint64_t address, uint8_t am,
				   bool hint, enum dmar_iirg_type iirg)
{
	/* set Drain Reads & Drain Writes,
	 * if hardware doesn't support it, will be ignored by hardware
	 */
	uint64_t cmd = DMA_IOTLB_IVT | DMA_IOTLB_DR | DMA_IOTLB_DW;
	uint64_t addr = 0UL;
	uint32_t status;

	switch (iirg) {
	case DMAR_IIRG_GLOBAL:
		cmd |= DMA_IOTLB_GLOBAL_INVL;
		break;
	case DMAR_IIRG_DOMAIN:
		cmd |= DMA_IOTLB_DOMAIN_INVL | dma_iotlb_did(did);
		break;
	case DMAR_IIRG_PAGE:
		cmd |= DMA_IOTLB_PAGE_INVL | dma_iotlb_did(did);
		addr = address | dma_iotlb_invl_addr_am(am);
		if (hint) {
			addr |= DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED;
		}
		break;
	default:
		pr_err("unknown IIRG type");
		return;
	}
	spinlock_obtain(&(dmar_uint->lock));
	if (addr != 0U) {
		iommu_write64(dmar_uint, dmar_uint->ecap_iotlb_offset, addr);
	}

	iommu_write64(dmar_uint, dmar_uint->ecap_iotlb_offset + 8U, cmd);
	/* read upper 32bits to check */
	dmar_wait_completion(dmar_uint, dmar_uint->ecap_iotlb_offset + 12U,
				DMA_IOTLB_IVT_32, true, &status);
	spinlock_release(&(dmar_uint->lock));

	if (dma_iotlb_get_iaig_32(status) == 0U) {
		pr_err("fail to invalidate IOTLB!, 0x%x, 0x%x",
			status, iommu_read32(dmar_uint, DMAR_FSTS_REG));
	}
}

/* Invalidate IOTLB globally,
 * all iotlb entries are invalidated,
 * all PASID-cache entries are invalidated,
 * all paging-structure-cache entries are invalidated.
 */
static void dmar_invalid_iotlb_global(struct dmar_drhd_rt *dmar_uint)
{
	dmar_invalid_iotlb(dmar_uint, 0U, 0UL, 0U, false, DMAR_IIRG_GLOBAL);
}

static void dmar_set_root_table(struct dmar_drhd_rt *dmar_uint)
{
	uint64_t address;
	uint32_t status;

	spinlock_obtain(&(dmar_uint->lock));

	/*
	 * dmar_set_root_table is called from init_iommu and
	 * resume_iommu. So NULL check on this pointer is needed
	 * so that we do not change the root table pointer in the
	 * resume flow.
	 */

	if (dmar_uint->root_table_addr == 0UL) {
		dmar_uint->root_table_addr = hva2hpa(get_root_table(dmar_uint->index));
	}

	/* Currently don't support extended root table */
	address = dmar_uint->root_table_addr;

	iommu_write64(dmar_uint, DMAR_RTADDR_REG, address);

	iommu_write32(dmar_uint, DMAR_GCMD_REG,
			dmar_uint->gcmd | DMA_GCMD_SRTP);

	/* 32-bit register */
	dmar_wait_completion(dmar_uint, DMAR_GSTS_REG, DMA_GSTS_RTPS, false,
				&status);
	spinlock_release(&(dmar_uint->lock));
}

static void dmar_fault_event_mask(struct dmar_drhd_rt *dmar_uint)
{
	spinlock_obtain(&(dmar_uint->lock));
	iommu_write32(dmar_uint, DMAR_FECTL_REG, DMA_FECTL_IM);
	spinlock_release(&(dmar_uint->lock));
}

static void dmar_fault_event_unmask(struct dmar_drhd_rt *dmar_uint)
{
	spinlock_obtain(&(dmar_uint->lock));
	iommu_write32(dmar_uint, DMAR_FECTL_REG, 0U);
	spinlock_release(&(dmar_uint->lock));
}

static void dmar_fault_msi_write(struct dmar_drhd_rt *dmar_uint,
			uint32_t vector)
{
	uint32_t data;
	uint32_t addr_low;
	uint32_t lapic_id = get_cur_lapic_id();

	data = DMAR_MSI_DELIVERY_LOWPRI | vector;
	/* redirection hint: 0
	 * destination mode: 0
	 */
	addr_low = 0xFEE00000U | ((uint32_t)(lapic_id) << 12U);

	spinlock_obtain(&(dmar_uint->lock));
	iommu_write32(dmar_uint, DMAR_FEDATA_REG, data);
	iommu_write32(dmar_uint, DMAR_FEADDR_REG, addr_low);
	spinlock_release(&(dmar_uint->lock));
}

#if DBG_IOMMU
static void fault_status_analysis(uint32_t status)
{
	if (dma_fsts_pfo(status)) {
		pr_info("Primary Fault Overflow");
	}

	if (dma_fsts_ppf(status)) {
		pr_info("Primary Pending Fault");
	}

	if (dma_fsts_afo(status)) {
		pr_info("Advanced Fault Overflow");
	}

	if (dma_fsts_apf(status)) {
		pr_info("Advanced Pending Fault");
	}

	if (dma_fsts_iqe(status)) {
		pr_info("Invalidation Queue Error");
	}

	if (dma_fsts_ice(status)) {
		pr_info("Invalidation Completion Error");
	}

	if (dma_fsts_ite(status)) {
		pr_info("Invalidation Time-out Error");
	}

	if (dma_fsts_pro(status)) {
		pr_info("Page Request Overflow");
	}
}
#endif

static void fault_record_analysis(__unused uint64_t low, uint64_t high)
{
	if (dma_frcd_up_f(high)) {
		return;
	}

	/* currently skip PASID related parsing */
	pr_info("%s, Reason: 0x%x, SID: %x.%x.%x @0x%llx",
		(dma_frcd_up_t(high) != 0U) ? "Read/Atomic" : "Write",
		dma_frcd_up_fr(high),
		dma_frcd_up_sid(high) >> 8U,
		(dma_frcd_up_sid(high) >> 3U) & 0x1fUL,
		dma_frcd_up_sid(high) & 0x7UL,
		low);
#if DBG_IOMMU
	if (iommu_ecap_dt(dmar_uint->ecap)i != 0U) {
		pr_info("Address Type: 0x%x",
				dma_frcd_up_at(high));
	}
#endif
}

static void dmar_fault_handler(uint32_t irq, void *data)
{
	struct dmar_drhd_rt *dmar_uint = (struct dmar_drhd_rt *)data;
	uint32_t fsr;
	uint32_t index;
	uint32_t record_reg_offset;
	uint64_t record[2];
	int loop = 0;

	dev_dbg(ACRN_DBG_IOMMU, "%s: irq = %d", __func__, irq);

	fsr = iommu_read32(dmar_uint, DMAR_FSTS_REG);

#if DBG_IOMMU
	fault_status_analysis(fsr);
#endif

	while (dma_fsts_ppf(fsr)) {
		loop++;
		index = dma_fsts_fri(fsr);
		record_reg_offset = (uint32_t)dmar_uint->cap_fault_reg_offset
				+ (index * 16U);
		if (index >= dmar_uint->cap_num_fault_regs) {
			dev_dbg(ACRN_DBG_IOMMU, "%s: invalid FR Index",
					__func__);
			break;
		}

		/* read 128-bit fault recording register */
		record[0] = iommu_read64(dmar_uint, record_reg_offset);
		record[1] = iommu_read64(dmar_uint, record_reg_offset + 8U);

		dev_dbg(ACRN_DBG_IOMMU, "%s: record[%d] @0x%x:  0x%llx, 0x%llx",
			__func__, index, record_reg_offset,
			record[0], record[1]);

		fault_record_analysis(record[0], record[1]);

		/* write to clear */
		iommu_write64(dmar_uint, record_reg_offset, record[0]);
		iommu_write64(dmar_uint, record_reg_offset + 8U, record[1]);

#ifdef DMAR_FAULT_LOOP_MAX
		if (loop > DMAR_FAULT_LOOP_MAX) {
			dev_dbg(ACRN_DBG_IOMMU, "%s: loop more than %d times",
				__func__, DMAR_FAULT_LOOP_MAX);
			break;
		}
#endif

		fsr = iommu_read32(dmar_uint, DMAR_FSTS_REG);
	}
}

static int dmar_setup_interrupt(struct dmar_drhd_rt *dmar_uint)
{
	uint32_t vector;
	int32_t retval;

	if (dmar_uint->dmar_irq != IRQ_INVALID) {
		dev_dbg(ACRN_DBG_IOMMU, "%s: irq already setup", __func__);
		return 0;
	}

	retval = request_irq(IRQ_INVALID,
			     dmar_fault_handler,
			     dmar_uint,
			     IRQF_NONE);

	if (retval < 0 ) {
		pr_err("%s: fail to setup interrupt", __func__);
		return retval;
	} else {
		dmar_uint->dmar_irq = (uint32_t)retval;
	}

	vector = irq_to_vector(dmar_uint->dmar_irq);

	dev_dbg(ACRN_DBG_IOMMU, "alloc irq#%d vector#%d for dmar_uint",
			dmar_uint->dmar_irq, vector);

	dmar_fault_msi_write(dmar_uint, vector);
	dmar_fault_event_unmask(dmar_uint);

	return 0;
}

static void dmar_enable(struct dmar_drhd_rt *dmar_uint)
{
	dev_dbg(ACRN_DBG_IOMMU, "enable dmar uint [0x%x]",
		dmar_uint->drhd->reg_base_addr);
	dmar_setup_interrupt(dmar_uint);
	dmar_write_buffer_flush(dmar_uint);
	dmar_set_root_table(dmar_uint);
	dmar_invalid_context_cache_global(dmar_uint);
	dmar_invalid_iotlb_global(dmar_uint);
	dmar_enable_translation(dmar_uint);
}

static void dmar_disable(struct dmar_drhd_rt *dmar_uint)
{
	if ((dmar_uint->gcmd & DMA_GCMD_TE) != 0U) {
		dmar_disable_translation(dmar_uint);
	}

	dmar_fault_event_mask(dmar_uint);
}

struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table,
		uint32_t addr_width)
{
	struct iommu_domain *domain;

	/* TODO: check if a domain with the vm_id exists */

	if (translation_table == 0UL) {
		pr_err("translation table is NULL");
		return NULL;
	}

	/*
	 * A hypercall is called to create an iommu domain for a valid VM,
	 * and hv code limit the VM number to CONFIG_MAX_VM_NUM.
	 * So the array iommu_domains will not be accessed out of range.
	 */
	domain = &iommu_domains[vmid_to_domainid(vm_id)];

	domain->is_host = false;
	domain->vm_id = vm_id;
	domain->trans_table_ptr = translation_table;
	domain->addr_width = addr_width;
	domain->is_tt_ept = true;

	dev_dbg(ACRN_DBG_IOMMU, "create domain [%d]: vm_id = %hu, ept@0x%x",
		vmid_to_domainid(domain->vm_id),
		domain->vm_id,
		domain->trans_table_ptr);

	return domain;
}

/**
 * @pre domain != NULL
 */
void destroy_iommu_domain(struct iommu_domain *domain)
{
	/* currently only support ept */
	if (!domain->is_tt_ept) {
		ASSERT(false, "translation_table is not EPT!");
	}

	/* TODO: check if any device assigned to this domain */
	(void)memset(domain, 0U, sizeof(*domain));
}

static int add_iommu_device(const struct iommu_domain *domain, uint16_t segment,
		uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_uint;
	struct dmar_root_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_context_entry *context_table;
	struct dmar_root_entry *root_entry;
	struct dmar_context_entry *context_entry;
	uint64_t upper;
	uint64_t lower = 0UL;

	dmar_uint = device_to_dmaru(segment, bus, devfun);
	if (dmar_uint == NULL) {
		pr_err("no dmar unit found for device:0x%x:%x.%x",
			bus, devfun >> 3U, devfun & 0x7U);
		return 1;
	}

	if (dmar_uint->drhd->ignore) {
		dev_dbg(ACRN_DBG_IOMMU, "device is ignored :0x%x:%x.%x",
			bus, devfun >> 3U, devfun & 0x7U);
		return 0;
	}

	if (!dmar_unit_support_aw(dmar_uint, domain->addr_width)) {
		pr_err("dmar doesn't support addr width %d",
			domain->addr_width);
		return 1;
	}

	ASSERT(dmar_uint->root_table_addr != 0UL, "root table is not setup");

	root_table =
		(struct dmar_root_entry *)hpa2hva(dmar_uint->root_table_addr);

	root_entry = root_table + bus;

	if (dmar_get_bitslice(root_entry->lower,
	        ROOT_ENTRY_LOWER_PRESENT_MASK,
		ROOT_ENTRY_LOWER_PRESENT_POS) == 0UL) {
		/* create context table for the bus if not present */
		context_table_addr = hva2hpa(get_ctx_table(dmar_uint->index, bus));

		context_table_addr = context_table_addr >> CPU_PAGE_SHIFT;

		lower = dmar_set_bitslice(lower,
				ROOT_ENTRY_LOWER_CTP_MASK,
				ROOT_ENTRY_LOWER_CTP_POS,
				context_table_addr);
		lower = dmar_set_bitslice(lower,
				ROOT_ENTRY_LOWER_PRESENT_MASK,
				ROOT_ENTRY_LOWER_PRESENT_POS, 1UL);

		root_entry->upper = 0UL;
		root_entry->lower = lower;
		iommu_flush_cache(dmar_uint, root_entry,
				sizeof(struct dmar_root_entry));
	} else {
		context_table_addr = dmar_get_bitslice(root_entry->lower,
				ROOT_ENTRY_LOWER_CTP_MASK,
			        ROOT_ENTRY_LOWER_CTP_POS);
	}

	context_table_addr = context_table_addr << CPU_PAGE_SHIFT;

	context_table =
		(struct dmar_context_entry *)hpa2hva(context_table_addr);
	context_entry = context_table + devfun;

	/* the context entry should not be present */
	if (dmar_get_bitslice(context_entry->lower,
	        CTX_ENTRY_LOWER_P_MASK,
		CTX_ENTRY_LOWER_P_POS) != 0UL) {
		pr_err("%s: context entry@0x%llx (Lower:%x) ",
				__func__, context_entry, context_entry->lower);
		pr_err("already present for %x:%x.%x",
				bus, devfun >> 3U, devfun & 0x7U);
		return 1;
	}

	/* setup context entry for the devfun */
	upper = 0UL;
	lower = 0UL;
	if (domain->is_host) {
		if (iommu_ecap_pt(dmar_uint->ecap) != 0U) {
			/* When the Translation-type (T) field indicates
			 * pass-through processing (10b), AW field must be
			 * programmed to indicate the largest AGAW value
			 * supported by hardware.
			 */
			upper = dmar_set_bitslice(upper,
				  CTX_ENTRY_UPPER_AW_MASK,
				  CTX_ENTRY_UPPER_AW_POS,
				  dmar_uint->cap_msagaw);
			lower = dmar_set_bitslice(lower,
				  CTX_ENTRY_LOWER_TT_MASK,
				  CTX_ENTRY_LOWER_TT_POS,
			          DMAR_CTX_TT_PASSTHROUGH);
		} else {
			ASSERT(false,
				  "dmaru doesn't support trans passthrough");
		}
	} else {
		/* TODO: add Device TLB support */
		upper = dmar_set_bitslice(upper,
		          CTX_ENTRY_UPPER_AW_MASK,
		          CTX_ENTRY_UPPER_AW_POS,
			  (uint64_t)width_to_agaw(domain->addr_width));
		lower = dmar_set_bitslice(lower,
		          CTX_ENTRY_LOWER_TT_MASK,
			  CTX_ENTRY_LOWER_TT_POS,
			  DMAR_CTX_TT_UNTRANSLATED);
	}

	upper = dmar_set_bitslice(upper,
	          CTX_ENTRY_UPPER_DID_MASK,
		  CTX_ENTRY_UPPER_DID_POS,
		  (uint64_t)vmid_to_domainid(domain->vm_id));
	lower = dmar_set_bitslice(lower,
		  CTX_ENTRY_LOWER_SLPTPTR_MASK,
		  CTX_ENTRY_LOWER_SLPTPTR_POS,
		  domain->trans_table_ptr >> CPU_PAGE_SHIFT);
	lower = dmar_set_bitslice(lower,
		  CTX_ENTRY_LOWER_P_MASK,
		  CTX_ENTRY_LOWER_P_POS,
		  1UL);

	context_entry->upper = upper;
	context_entry->lower = lower;
	iommu_flush_cache(dmar_uint, context_entry,
			sizeof(struct dmar_context_entry));

	return 0;
}

static int
remove_iommu_device(const struct iommu_domain *domain, uint16_t segment,
		uint8_t bus, uint8_t devfun)
{
	struct dmar_drhd_rt *dmar_uint;
	struct dmar_root_entry *root_table;
	uint64_t context_table_addr;
	struct dmar_context_entry *context_table;
	struct dmar_root_entry *root_entry;
	struct dmar_context_entry *context_entry;
	uint16_t dom_id;

	dmar_uint = device_to_dmaru(segment, bus, devfun);
	if (dmar_uint == NULL) {
		pr_err("no dmar unit found for device:0x%x:%x",
				bus, devfun);
		return 1;
	}

	root_table =
		(struct dmar_root_entry *)hpa2hva(dmar_uint->root_table_addr);
	root_entry = root_table + bus;

	context_table_addr = dmar_get_bitslice(root_entry->lower,
					       ROOT_ENTRY_LOWER_CTP_MASK,
					       ROOT_ENTRY_LOWER_CTP_POS);
	context_table_addr = context_table_addr << CPU_PAGE_SHIFT;
	context_table =
		(struct dmar_context_entry *)hpa2hva(context_table_addr);

	context_entry = context_table + devfun;

	dom_id = (uint16_t)dmar_get_bitslice(context_entry->upper,
		        CTX_ENTRY_UPPER_DID_MASK, CTX_ENTRY_UPPER_DID_POS);
	if (dom_id != vmid_to_domainid(domain->vm_id)) {
		pr_err("%s: domain id mismatch", __func__);
		return 1;
	}

	/* clear the present bit first */
	context_entry->lower = 0UL;
	context_entry->upper = 0UL;
	iommu_flush_cache(dmar_uint, context_entry,
			sizeof(struct dmar_context_entry));

	/* if caching mode is present, need to invalidate translation cache */
	/* if(cap_caching_mode(dmar_uint->cap)) { */
	dmar_invalid_context_cache_global(dmar_uint);
	dmar_invalid_iotlb_global(dmar_uint);
	/* } */
	return 0;
}

int assign_iommu_device(const struct iommu_domain *domain, uint8_t bus,
				uint8_t devfun)
{
	/* TODO: check if the device assigned */

	if ((vm0_domain != NULL) && (remove_iommu_device(vm0_domain, 0U, bus, devfun) == 1)) {
		return 1;
	}

	return add_iommu_device(domain, 0U, bus, devfun);
}

int unassign_iommu_device(const struct iommu_domain *domain, uint8_t bus,
				uint8_t devfun)
{
	/* TODO: check if the device assigned */

	if (remove_iommu_device(domain, 0U, bus, devfun) == 1) {
		return 1;
	}

	if ((vm0_domain != NULL) && (add_iommu_device(vm0_domain, 0U, bus, devfun) == 1)) {
		return 1;
	}

	return 0;
}

void enable_iommu(void)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_uint;
	uint32_t j;

	for (j = 0U; j < info->drhd_count; j++) {
		dmar_uint = &dmar_drhd_units[j];
		if (!dmar_uint->drhd->ignore) {
			dmar_enable(dmar_uint);
		}
		else {
			dev_dbg(ACRN_DBG_IOMMU, "ignore dmar_uint @0x%x",
				dmar_uint->drhd->reg_base_addr);
		}
	}
}

void disable_iommu(void)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_uint;
	uint32_t j;

	for (j = 0U; j < info->drhd_count; j++) {
		dmar_uint = &dmar_drhd_units[j];
		dmar_disable(dmar_uint);
	}
}

void suspend_iommu(void)
{
	struct dmar_info *info = get_dmar_info();
	struct dmar_drhd_rt *dmar_unit;
	uint32_t i, j;

	for (j = 0U; j < info->drhd_count; j++) {
		dmar_unit = &dmar_drhd_units[j];

		if (dmar_unit->drhd->ignore) {
			continue;
		}

		/* flush */
		dmar_write_buffer_flush(dmar_unit);
		dmar_invalid_context_cache_global(dmar_unit);
		dmar_invalid_iotlb_global(dmar_unit);

		/* save IOMMU fault register state */
		for (i = 0U; i < IOMMU_FAULT_REGISTER_STATE_NUM; i++) {
			dmar_unit->fault_state[i] =  iommu_read32(dmar_unit,
				DMAR_FECTL_REG + (i * IOMMU_FAULT_REGISTER_SIZE));

		}
		/* disable translation */
		dmar_disable_translation(dmar_unit);
	}
}

void resume_iommu(void)
{
	struct dmar_drhd_rt *dmar_unit;
	struct dmar_info *info = get_dmar_info();
	uint32_t i, j;

	/* restore IOMMU fault register state */
	for (j = 0U; j < info->drhd_count; j++) {
		dmar_unit = &dmar_drhd_units[j];

		if (dmar_unit->drhd->ignore) {
			continue;
		}

		/* set root table */
		dmar_set_root_table(dmar_unit);

		/* flush */
		dmar_write_buffer_flush(dmar_unit);
		dmar_invalid_context_cache_global(dmar_unit);
		dmar_invalid_iotlb_global(dmar_unit);

		/* restore IOMMU fault register state */
		for (i = 0U; i < IOMMU_FAULT_REGISTER_STATE_NUM; i++) {
			iommu_write32(dmar_unit, DMAR_FECTL_REG +
				(i * IOMMU_FAULT_REGISTER_SIZE),
				dmar_unit->fault_state[i]);
		}
		/* enable translation */
		dmar_enable_translation(dmar_unit);
	}
}

int init_iommu(void)
{
	int ret = 0;

	ret = register_hrhd_units();
	if (ret != 0) {
		return ret;
	}

	enable_iommu();

	cache_flush_invalidate_all();

	return ret;
}

void init_iommu_vm0_domain(struct acrn_vm *vm0)
{
	uint16_t bus;
	uint16_t devfun;

	vm0->iommu = create_iommu_domain(vm0->vm_id,
		hva2hpa(vm0->arch_vm.nworld_eptp), 48U);

	vm0_domain = (struct iommu_domain *) vm0->iommu;

	for (bus = 0U; bus <= CONFIG_IOMMU_INIT_BUS_LIMIT; bus++) {
		for (devfun = 0U; devfun <= 255U; devfun++) {
			add_iommu_device(vm0_domain, 0U,
				(uint8_t)bus, (uint8_t)devfun);
		}
	}
	cache_flush_invalidate_all();
}
