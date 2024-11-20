/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __AARCH64_ARM_GICV3_COMMON_H__
#define __AARCH64_ARM_GICV3_COMMON_H__

/*******************************************************************************
 * GIC500/GIC600 Re-distributor interface registers & constants
 ******************************************************************************/

/* GICR_WAKER implementation-defined bit definitions */
#define	WAKER_SL_SHIFT		0
#define	WAKER_QSC_SHIFT		31

#define WAKER_SL_BIT		(1U << WAKER_SL_SHIFT)
#define WAKER_QSC_BIT		(1U << WAKER_QSC_SHIFT)

#define IIDR_MODEL_ARM_GIC_600		U(0x0200043b)
#define IIDR_MODEL_ARM_GIC_600AE	U(0x0300043b)
#define IIDR_MODEL_ARM_GIC_700		U(0x0400043b)

#define PIDR_COMPONENT_ARM_DIST		U(0x492)
#define PIDR_COMPONENT_ARM_REDIST	U(0x493)
#define PIDR_COMPONENT_ARM_ITS		U(0x494)

#endif /* __AARCH64_ARM_GICV3_COMMON_H__ */
