/*
 * Copyright (c) 2015-2020, ARM Limited and Contributors. All rights reserved.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../common/gic_common.h"
#include "gicv2.h"
#include "../common/gic_common_private.h"
#include "gicv2_private.h"

/*
 * Accessor to read the GIC Distributor ITARGETSR corresponding to the
 * interrupt `id`, 4 interrupt IDs at a time.
 */
unsigned int gicd_read_itargetsr(uintptr_t base, unsigned int id)
{
	unsigned n = id >> ITARGETSR_SHIFT;
	return mmio_read_32(base + GICD_ITARGETSR + (n << 2));
}

/*
 * Accessor to read the GIC Distributor CPENDSGIR corresponding to the
 * interrupt `id`, 4 interrupt IDs at a time.
 */
unsigned int gicd_read_cpendsgir(uintptr_t base, unsigned int id)
{
	unsigned n = id >> CPENDSGIR_SHIFT;
	return mmio_read_32(base + GICD_CPENDSGIR + (n << 2));
}

/*
 * Accessor to read the GIC Distributor SPENDSGIR corresponding to the
 * interrupt `id`, 4 interrupt IDs at a time.
 */
unsigned int gicd_read_spendsgir(uintptr_t base, unsigned int id)
{
	unsigned n = id >> SPENDSGIR_SHIFT;
	return mmio_read_32(base + GICD_SPENDSGIR + (n << 2));
}

/*
 * Accessor to write the GIC Distributor ITARGETSR corresponding to the
 * interrupt `id`, 4 interrupt IDs at a time.
 */
void gicd_write_itargetsr(uintptr_t base, unsigned int id, unsigned int val)
{
	unsigned n = id >> ITARGETSR_SHIFT;
	mmio_write_32(base + GICD_ITARGETSR + (n << 2), val);
}

/*
 * Accessor to write the GIC Distributor CPENDSGIR corresponding to the
 * interrupt `id`, 4 interrupt IDs at a time.
 */
void gicd_write_cpendsgir(uintptr_t base, unsigned int id, unsigned int val)
{
	unsigned n = id >> CPENDSGIR_SHIFT;
	mmio_write_32(base + GICD_CPENDSGIR + (n << 2), val);
}

/*
 * Accessor to write the GIC Distributor SPENDSGIR corresponding to the
 * interrupt `id`, 4 interrupt IDs at a time.
 */
void gicd_write_spendsgir(uintptr_t base, unsigned int id, unsigned int val)
{
	unsigned n = id >> SPENDSGIR_SHIFT;
	mmio_write_32(base + GICD_SPENDSGIR + (n << 2), val);
}

/*******************************************************************************
 * Get the current CPU bit mask from GICD_ITARGETSR0
 ******************************************************************************/
unsigned int gicv2_get_cpuif_id(uintptr_t base)
{
	unsigned int val;

	val = gicd_read_itargetsr(base, 0);
	return val & GIC_TARGET_CPU_MASK;
}

/*******************************************************************************
 * Helper function to configure the default attributes of SPIs.
 ******************************************************************************/
void gicv2_spis_configure_defaults(uintptr_t gicd_base)
{
	unsigned int index, num_ints;

	num_ints = gicd_read_typer(gicd_base);
	num_ints &= TYPER_IT_LINES_NO_MASK;
	num_ints = (num_ints + 1U) << 5;

	/* Setup the default SPI priorities doing four at a time */
	for (index = MIN_SPI_ID; index < num_ints; index += 4U)
		gicd_write_ipriorityr(gicd_base,
							  index,
							  GICD_IPRIORITYR_DEF_VAL);

	/* Treat all SPIs as level triggered by default, 16 at a time */
	for (index = MIN_SPI_ID; index < num_ints; index += 16U) {
		gicd_write_icfgr(gicd_base, index, 0U);
	}

	for (index = MIN_SPI_ID; index < num_ints; index += 4U) {
		gicd_write_itargetsr(gicd_base, index, GICD_ITARGETSR_DEF_VAL);
	}
}

/*******************************************************************************
 * Helper function to configure properties of secure G0 SGIs and PPIs.
 ******************************************************************************/
void gicv2_ppi_sgi_setup(uintptr_t gicd_base)
{
	unsigned int i;

	/*
	 * Disable all SGIs (imp. def.)/PPIs before configuring them. This is a
	 * more scalable approach as it avoids clearing the enable bits in the
	 * GICD_CTLR.
	 */
	gicd_write_icenabler(gicd_base, 0U, ~0U);

	/* Setup the default PPI/SGI priorities doing four at a time */
	for (i = 0U; i < MIN_PPI_ID; i += 4U) {
		gicd_write_ipriorityr(gicd_base, i, GIC_SGI_DEF_PRIORITY);
	}

	for (i = MIN_PPI_ID; i < MIN_SPI_ID; i += 4U) {
		gicd_write_ipriorityr(gicd_base, i, GICD_IPRIORITYR_DEF_VAL);
	}

	/* Enable the Group 1 SGIs */
	gicd_write_isenabler(gicd_base, 0, 0xffffU);
}
