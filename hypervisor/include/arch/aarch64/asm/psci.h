/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __AARCH64_PSCI_H__
#define __AARCH64_PSCI_H__

#include <asm/smc.h>

enum PSCI_FN_TYPE {
	PSCI_FN_VERSION,
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_OFF,
	PSCI_FN_CPU_ON,
	PSCI_FN_AFFINITY_INFO,
	PSCI_FN_MIGRATE,
	PSCI_FN_MIGRATE_INFO_TYPE,
	PSCI_FN_MIGRATE_INFO_UP_CPU,
	PSCI_FN_SYSTEM_OFF,
	PSCI_FN_SYSTEM_RESET,
	PSCI_FN_PSCI_FEATURES,
	PSCI_FN_CPU_FREEZE,
	PSCI_FN_CPU_DEFAULT_SUSPEND,
	PSCI_FN_NODE_HW_STATE,
	PSCI_FN_SYSTEM_SUSPEND,
	PSCI_FN_PSCI_SET_SUSPEND_MODE,
	PSCI_FN_PSCI_STAT_RESIDENCY,
	PSCI_FN_PSCI_STAT_COUNT,
	PSCI_FN_SYSTEM_RESET2,
	PSCI_FN_MEM_PROTECT,
	PSCI_FN_MEM_PROTECT_CHECK_RANGE
};

#define PSCI_VERSION_0_2	((0U<<16) | 2U)
#define PSCI_VERSION_1_0	((1U<<16) | 0U)
#define PSCI_VERSION_1_1	((1U<<16) | 1U)

#define PSCI_MIGRATE_INFO_0	0U /* Uniprocessor migrate capable Trusted OS. */
#define PSCI_MIGRATE_INFO_1	1U /* Uniprocessor not migrate capable Trusted OS. */
#define PSCI_MIGRATE_INFO_2	2U /* Trusted OS is either not present or does not require migration. */

#define PSCI_SUCCESS		0
#define PSCI_NOT_SUPPORTED	(-1)
#define PSCI_INVALID_PARAMS	(-2)
#define PSCI_DENIED		(-3)
#define PSCI_ALREADY_ON		(-4)
#define PSCI_ON_PENDING		(-5)
#define PSCI_INTERNAL_FAILURE	(-6)
#define PSCI_NOT_PRESENT	(-7)
#define PSCI_DISABLED		(-8)
#define PSCI_INVALID_ADDRESS	(-9)

int32_t psci_cpu_on(uint32_t cpu);

#endif /* __AARCH64_PSCI_H__ */
