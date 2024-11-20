/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vmexit.h>

int32_t vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	return 0;
}
