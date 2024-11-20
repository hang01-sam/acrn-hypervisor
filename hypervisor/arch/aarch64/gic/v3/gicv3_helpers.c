/*
 * Copyright (c) 2015-2023, Arm Limited and Contributors. All rights reserved.
 * Copyright (c) 2023, NVIDIA Corporation. All rights reserved.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <debug/logmsg.h>
#include "../common/gic_common.h"
#include "../common/gic_common_private.h"
#include "gicv3_private.h"

uintptr_t gicv3_get_multichip_base(uint32_t spi_id, uintptr_t gicd_base)
{
	(void)spi_id;
#if GICV3_IMPL_GIC600_MULTICHIP
	if (gic600_multichip_is_initialized()) {
		return gic600_multichip_gicd_base_for_spi(spi_id);
	}
#endif
	return gicd_base;
}

/******************************************************************************
 * This function marks the core as awake in the re-distributor and
 * ensures that the interface is active.
 *****************************************************************************/
void gicv3_rdistif_mark_core_awake(uintptr_t gicr_base)
{
	/*
	 * The WAKER_PS_BIT should be changed to 0
	 * only when WAKER_CA_BIT is 1.
	 */
	ASSERT((gicr_read_waker(gicr_base) & WAKER_CA_BIT) != 0U);

	/* Mark the connected core as awake */
	gicr_write_waker(gicr_base, gicr_read_waker(gicr_base) & ~WAKER_PS_BIT);

	/* Wait till the WAKER_CA_BIT changes to 0 */
	while ((gicr_read_waker(gicr_base) & WAKER_CA_BIT) != 0U) {
	}
}

/******************************************************************************
 * This function marks the core as asleep in the re-distributor and ensures
 * that the interface is quiescent.
 *****************************************************************************/
void gicv3_rdistif_mark_core_asleep(uintptr_t gicr_base)
{
	/* Mark the connected core as asleep */
	gicr_write_waker(gicr_base, gicr_read_waker(gicr_base) | WAKER_PS_BIT);

	/* Wait till the WAKER_CA_BIT changes to 1 */
	while ((gicr_read_waker(gicr_base) & WAKER_CA_BIT) == 0U) {
	}
}

/*******************************************************************************
 * This function probes the Redistributor frames when the driver is initialised
 * and saves their base addresses. These base addresses are used later to
 * initialise each Redistributor interface.
 ******************************************************************************/
void gicv3_rdistif_base_addrs_probe(uintptr_t *rdistif_base_addrs,
									unsigned int rdistif_num,
									uintptr_t gicr_base,
									mpidr_hash_fn mpidr_to_core_pos)
{
	u_register_t mpidr;
	unsigned int proc_num;
	uint64_t typer_val;
	uintptr_t rdistif_base = gicr_base;

	ASSERT(rdistif_base_addrs != NULL);

	/*
	 * Iterate over the Redistributor frames. Store the base address of each
	 * frame in the platform provided array. Use the "Processor Number"
	 * field to index into the array if the platform has not provided a hash
	 * function to convert an MPIDR (obtained from the "Affinity Value"
	 * field into a linear index.
	 */
	do {
		typer_val = gicr_read_typer(rdistif_base);
		if (mpidr_to_core_pos != NULL) {
			mpidr = mpidr_from_gicr_typer(typer_val);
			proc_num = mpidr_to_core_pos(mpidr);
		} else {
			proc_num = (typer_val >> TYPER_PROC_NUM_SHIFT) &
					   TYPER_PROC_NUM_MASK;
		}

		if (proc_num < rdistif_num) {
			rdistif_base_addrs[proc_num] = rdistif_base;
		}
		rdistif_base += gicv3_redist_size(typer_val);
	} while ((typer_val & TYPER_LAST_BIT) == 0U);
}

/*******************************************************************************
 * Helper function to get the maximum SPI INTID + 1.
 ******************************************************************************/
unsigned int gicv3_get_spi_limit(uintptr_t gicd_base)
{
	unsigned int spi_limit;
	unsigned int typer_reg = gicd_read_typer(gicd_base);

	/* (maximum SPI INTID + 1) is equal to 32 * (GICD_TYPER.ITLinesNumber+1) */
	spi_limit = ((typer_reg & TYPER_IT_LINES_NO_MASK) + 1U) << 5;

	/* Filter out special INTIDs 1020-1023 */
	if (spi_limit > (MAX_SPI_ID + 1U)) {
		return MAX_SPI_ID + 1U;
	}

	return spi_limit;
}

#if GIC_EXT_INTID
/*******************************************************************************
 * Helper function to get the maximum ESPI INTID + 1.
 ******************************************************************************/
unsigned int gicv3_get_espi_limit(uintptr_t gicd_base)
{
	unsigned int typer_reg = gicd_read_typer(gicd_base);

	/* Check if extended SPI range is implemented */
	if ((typer_reg & TYPER_ESPI) != 0U) {
		/*
		 * (maximum ESPI INTID + 1) is equal to
		 * 32 * (GICD_TYPER.ESPI_range + 1) + 4096
		 */
		return ((((typer_reg >> TYPER_ESPI_RANGE_SHIFT) &
				  TYPER_ESPI_RANGE_MASK) + 1U) << 5) + MIN_ESPI_ID;
	}

	return 0U;
}
#endif /* GIC_EXT_INTID */

/*******************************************************************************
 * Helper function to configure the default attributes of (E)SPIs.
 ******************************************************************************/
void gicv3_spis_config_defaults(uintptr_t gicd_base)
{
	unsigned int i, num_ints;
#if GIC_EXT_INTID
	unsigned int num_eints;
#endif

	num_ints = gicv3_get_spi_limit(gicd_base);
	pr_info("Maximum SPI INTID supported: %u", num_ints - 1);

	/* Treat all (E)SPIs as G1NS by default. We do 32 at a time. */
	for (i = MIN_SPI_ID; i < num_ints; i += (1U << IGROUPR_SHIFT)) {
		gicd_write_igroupr(gicv3_get_multichip_base(i, gicd_base), i, ~0U);
	}

#if GIC_EXT_INTID
	num_eints = gicv3_get_espi_limit(gicd_base);
	if (num_eints != 0U) {
		pr_info("Maximum ESPI INTID supported: %u\n", num_eints - 1);

		for (i = MIN_ESPI_ID; i < num_eints;
			 i += (1U << IGROUPR_SHIFT)) {
			gicd_write_igroupr(gicv3_get_multichip_base(i, gicd_base), i, ~0U);
		}
	} else {
		pr_info("ESPI range is not implemented.\n");
	}
#endif

	/* Setup the default (E)SPI priorities doing four at a time */
	for (i = MIN_SPI_ID; i < num_ints; i += (1U << IPRIORITYR_SHIFT)) {
		gicd_write_ipriorityr(gicv3_get_multichip_base(i, gicd_base), i, GICD_IPRIORITYR_DEF_VAL);
	}

#if GIC_EXT_INTID
	for (i = MIN_ESPI_ID; i < num_eints;
		 i += (1U << IPRIORITYR_SHIFT)) {
		gicd_write_ipriorityr(gicv3_get_multichip_base(i, gicd_base), i, GICD_IPRIORITYR_DEF_VAL);
	}
#endif
	/*
	 * Treat all (E)SPIs as level triggered by default, write 16 at a time
	 */
	for (i = MIN_SPI_ID; i < num_ints; i += (1U << ICFGR_SHIFT)) {
		gicd_write_icfgr(gicv3_get_multichip_base(i, gicd_base), i, 0U);
	}

#if GIC_EXT_INTID
	for (i = MIN_ESPI_ID; i < num_eints; i += (1U << ICFGR_SHIFT)) {
		gicd_write_icfgr(gicv3_get_multichip_base(i, gicd_base), i, 0U);
	}
#endif
}

/*******************************************************************************
 * Helper function to configure the default attributes of (E)PPIs/SGIs
 ******************************************************************************/
void gicv3_ppi_sgi_config_defaults(uintptr_t gicr_base)
{
	unsigned int i, ppi_regs_num, regs_num, sgi_regs_num;

#if GIC_EXT_INTID
	/* Calculate number of PPI registers */
	ppi_regs_num = (unsigned int)((gicr_read_typer(gicr_base) >>
								   TYPER_PPI_NUM_SHIFT) & TYPER_PPI_NUM_MASK) + 1;
	/* All other values except PPInum [0-2] are reserved */
	if (ppi_regs_num > 3U) {
		ppi_regs_num = 1U;
	}
#else
	ppi_regs_num = 1U;
#endif
	/*
	 * Disable all SGIs (imp. def.)/(E)PPIs before configuring them.
	 * This is a more scalable approach as it avoids clearing
	 * the enable bits in the GICD_CTLR.
	 */
	for (i = 0U; i < ppi_regs_num; ++i) {
		gicr_write_icenabler(gicr_base, i, ~0U);
	}

	/* Wait for pending writes to GICR_ICENABLER */
	gicr_wait_for_pending_write(gicr_base);

	/* 32 interrupt IDs per GICR_IGROUPR register */
	for (i = 0U; i < ppi_regs_num; ++i) {
		/* Treat all SGIs/(E)PPIs as G1NS by default */
		gicr_write_igroupr(gicr_base, i, ~0U);
	}

	sgi_regs_num = MIN_PPI_ID >> 2;
	for (i = 0U; i < sgi_regs_num; ++i) {
		/* Setup the default (E)PPI/SGI priorities doing 4 at a time */
		gicr_write_ipriorityr(gicr_base, i << 2, GIC_SGI_DEF_PRIORITY);
	}

	/* 4 interrupt IDs per GICR_IPRIORITYR register */
	regs_num = ppi_regs_num << 3;
	for (i = sgi_regs_num; i < regs_num; ++i) {
		/* Setup the default (E)PPI/SGI priorities doing 4 at a time */
		gicr_write_ipriorityr(gicr_base, i << 2, GICD_IPRIORITYR_DEF_VAL);
	}

	for (i = 0U; i < ppi_regs_num; ++i) {
		gicr_write_isenabler(gicr_base, i, 0xffffU);
	}
}

/**
 * gicv3_rdistif_get_number_frames() - determine size of GICv3 GICR region
 * @gicr_frame: base address of the GICR region to check
 *
 * This iterates over the GICR_TYPER registers of multiple GICR frames in
 * a GICR region, to find the instance which has the LAST bit set. For most
 * systems this corresponds to the number of cores handled by a redistributor,
 * but there could be disabled cores among them.
 * It assumes that each GICR region is fully accessible (till the LAST bit
 * marks the end of the region).
 * If a platform has multiple GICR regions, this function would need to be
 * called multiple times, providing the respective GICR base address each time.
 *
 * Return: number of valid GICR frames (at least 1, up to PLATFORM_CORE_COUNT)
 ******************************************************************************/
unsigned int gicv3_rdistif_get_number_frames(const uintptr_t gicr_frame)
{
	uintptr_t rdistif_base = gicr_frame;
	unsigned int count;

	for (count = 1U; count < MAX_PCPU_NUM; count++) {
		uint64_t typer_val = gicr_read_typer(rdistif_base);

		if ((typer_val & TYPER_LAST_BIT) != 0U) {
			break;
		}
		rdistif_base += gicv3_redist_size(typer_val);
	}

	return count;
}

unsigned int gicv3_get_component_partnum(const uintptr_t gic_frame)
{
	unsigned int part_id;

	/*
	 * The lower 8 bits of PIDR0, complemented by the lower 4 bits of
	 * PIDR1 contain a part number identifying the GIC component at a
	 * particular base address.
	 */
	part_id = mmio_read_32(gic_frame + GICD_PIDR0_GICV3) & 0xff;
	part_id |= (mmio_read_32(gic_frame + GICD_PIDR1_GICV3) << 8) & 0xf00;

	return part_id;
}

/*******************************************************************************
 * Helper function to return product ID and revision of GIC
 * @gicd_base:   base address of the GIC distributor
 * @gic_prod_id: retrieved product id of GIC
 * @gic_rev:     retrieved revision of GIC
 ******************************************************************************/
void gicv3_get_component_prodid_rev(const uintptr_t gicd_base,
									unsigned int *gic_prod_id,
									uint8_t *gic_rev)
{
	unsigned int gicd_iidr;
	uint8_t gic_variant;

	gicd_iidr = gicd_read_iidr(gicd_base);
	*gic_prod_id = gicd_iidr >> IIDR_PRODUCT_ID_SHIFT;
	*gic_prod_id &= IIDR_PRODUCT_ID_MASK;

	gic_variant = gicd_iidr >> IIDR_VARIANT_SHIFT;
	gic_variant &= IIDR_VARIANT_MASK;

	*gic_rev = gicd_iidr >> IIDR_REV_SHIFT;
	*gic_rev &= IIDR_REV_MASK;

	/*
	 * pack gic variant and gic_rev in 1 byte
	 * gic_rev = gic_variant[7:4] and gic_rev[0:3]
	 */
	*gic_rev = *gic_rev | gic_variant << 0x4;

}
