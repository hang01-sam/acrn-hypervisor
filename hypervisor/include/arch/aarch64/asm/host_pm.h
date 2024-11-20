/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	__AARCH64_HOST_PM_H__
#define	__AARCH64_HOST_PM_H__

#include <acrn_common.h>

#define	BIT_SLP_TYPx	10U
#define	BIT_SLP_EN	13U
#define	BIT_WAK_STS	15U

struct cpu_state_info {
	uint8_t			 	px_cnt;	/* count of all Px states */
	const struct acrn_pstate_data	*px_data;
	uint8_t			 	cx_cnt;	/* count of all Cx entries */
	const struct acrn_cstate_data	*cx_data;
};

struct cpu_state_table {
	char			model_name[64];
	struct cpu_state_info	state_info;
};

void shutdown_system(void);

#endif	/* __AARCH64_HOST_PM_H__ */
