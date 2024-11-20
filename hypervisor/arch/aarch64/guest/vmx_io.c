/*
 * Copyright (C) 2019-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/atomic.h>
#include <io_req.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/ept.h>
#include <asm/pgtable.h>
#include <trace.h>
#include <logmsg.h>

void arch_fire_hsm_interrupt(void)
{
}

/**
 * @brief General complete-work for port I/O emulation
 *
 * @pre io_req->io_type == ACRN_IOREQ_TYPE_PORTIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding IO
 * request having transferred to the COMPLETE state.
 */
void
emulate_pio_complete(__unused struct acrn_vcpu *vcpu, __unused const struct io_request *io_req)
{
}


/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param vcpu The virtual CPU which triggers the VM exit on I/O instruction
 */
int32_t pio_instr_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	return 0;
}

int32_t ept_violation_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	return 0;
}

/**
 * @brief Allow a VM to access a port I/O range
 *
 * This API enables direct access from the given \p vm to the port I/O space
 * starting from \p port_address to \p port_address + \p nbytes - 1.
 *
 * @param vm The VM whose port I/O access permissions is to be changed
 * @param port_address The start address of the port I/O range
 * @param nbytes The size of the range, in bytes
 */
void allow_guest_pio_access(__unused struct acrn_vm *vm, __unused uint16_t port_address,
							__unused uint32_t nbytes)
{
}

void deny_guest_pio_access(__unused struct acrn_vm *vm, __unused uint16_t port_address,
						   __unused uint32_t nbytes)
{
}
