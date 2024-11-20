/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VSMC_H__
#define __AARCH64_VSMC_H__

int32_t ex_smc64_handler(struct acrn_vcpu *vcpu,
							  struct intr_excp_ctx *regs, union sysreg_esr esr);

#endif /* __AARCH64_VSMC_H__ */
