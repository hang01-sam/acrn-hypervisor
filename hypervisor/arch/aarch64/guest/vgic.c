/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file vgic.c
 *
 * @brief public APIs for vcpu operations
 */
#include <types.h>
#include <asm/sysreg.h>
#include <asm/cpu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vgic.h>


void vgic_inject(__unused struct acrn_vcpu *vcpu, __unused uint64_t id, __unused uint64_t source)
{

}
void vgic_send_msg(__unused struct acrn_vcpu *vcpu, __unused uint16_t eventid, __unused uint64_t data)
{

}
void vgic_set_hw(__unused struct acrn_vm *vm, __unused uint64_t id)
{

}
void vcpu_vgic_msg_handler(__unused struct acrn_vcpu *vcpu)
{

}
bool vgic_emulate(__unused struct acrn_vcpu *vcpu, __unused struct intr_excp_ctx *regs, __unused union sysreg_esr esr)
{
	return true;
}
void vgic_cpu_init(__unused struct acrn_vcpu *vcpu)
{

}
void vgic_init(__unused struct acrn_vm *vm, __unused struct vgic_config vgic_cfg)
{

}
