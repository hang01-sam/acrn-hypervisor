/*
 * Copyright (C) 2019-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VM_RESET_H__
#define __AARCH64_VM_RESET_H__

#include <acrn_common.h>

void register_reset_port_handler(struct acrn_vm *vm);
void shutdown_vm_from_idle(uint16_t pcpu_id);

#endif /* __AARCH64_VM_RESET_H__ */
