/*
 * Copyright (C) 2019-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/io.h>
#include <asm/host_pm.h>
#include <logmsg.h>
#include <asm/per_cpu.h>
#include <asm/guest/vm_reset.h>

void register_reset_port_handler(__unused struct acrn_vm *vm) { }

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	uint16_t vm_id;
	uint64_t *vms = &per_cpu(shutdown_vm_bitmap, pcpu_id);
	struct acrn_vm *vm;

	for (vm_id = fls64(*vms); vm_id < CONFIG_MAX_VM_NUM; vm_id = fls64(*vms)) {
		vm = get_vm_from_vmid(vm_id);
		get_vm_lock(vm);
		if (is_paused_vm(vm)) {
			(void)shutdown_vm(vm);
		}
		put_vm_lock(vm);
		bitmap_clear_nolock(vm_id, vms);
	}
}
