/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VMEXIT_H__
#define __AARCH64_VMEXIT_H__

int ex_hvc64_handler(struct acrn_vcpu *vcpu,
						  struct intr_excp_ctx *regs, union sysreg_esr esr);

int32_t vmexit_handler(struct acrn_vcpu *vcpu);

#endif /* __AARCH64_VMEXIT_H__ */
