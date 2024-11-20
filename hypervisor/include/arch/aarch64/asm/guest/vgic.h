/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file vgic.h
 *
 * @brief public APIs for vcpu operations
 */

#ifndef __AARCH64__VGIC_H__
#define __AARCH64__VGIC_H__
#include <types.h>
#include <asm/guest/vcpu.h>
#include <asm/cpu.h>
#include <asm/vm_config.h>

struct acrn_vgic {
	uint64_t    vgicr_addr;
	uint64_t    vgicd_addr;
	struct acrn_vm *vm;
};

#define VGIC_MSG_DATA(VM_ID, VGICRID, INT_ID, REG, VAL)                 \
    (((uint64_t)(VM_ID) << 48) | (((uint64_t)(VGICRID)&0xffff) << 32) | \
     (((INT_ID)&0x7fff) << 16) | (((REG)&0xff) << 8) | ((VAL)&0xff))

enum VGIC_EVENTS { VGIC_UPDATE_ENABLE, VGIC_ROUTE, VGIC_INJECT, VGIC_SET_REG };

void vgic_inject(struct acrn_vcpu *vcpu, uint64_t id, uint64_t source);

void vgic_send_msg(struct acrn_vcpu *vcpu, uint16_t eventid, uint64_t data);

void vgic_set_hw(struct acrn_vm *vm, uint64_t id);

void vcpu_vgic_msg_handler(struct acrn_vcpu *vcpu);

bool vgic_emulate(struct acrn_vcpu *vcpu, struct intr_excp_ctx *regs, union sysreg_esr esr);

void vgic_cpu_init(struct acrn_vcpu *vcpu);

void vgic_init(struct acrn_vm *vm, struct vgic_config vgic_cfg);

#endif
