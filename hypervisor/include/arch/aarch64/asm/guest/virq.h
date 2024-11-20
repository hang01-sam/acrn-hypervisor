/*
 * Copyright (C) 2021-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_VIRQ_H__
#define __AARCH64_VIRQ_H__

struct acrn_vcpu;
struct acrn_vm;

/**
 * @brief virtual IRQ
 *
 * @addtogroup acrn_virq ACRN vIRQ
 * @{
 */


/**
 * @brief Inject external interrupt to guest.
 *
 * @param[in] vcpu Pointer to vCPU.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_extint(struct acrn_vcpu *vcpu);

/**
 * @brief Inject general protection exeception(GP) to guest.
 *
 * @param[in] vcpu     Pointer to vCPU.
 * @param[in] err_code Error Code to be injected.
 *
 * @pre vcpu != NULL
 */
void vcpu_inject_gp(struct acrn_vcpu *vcpu, uint32_t err_code);

void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid);

/*
 * @pre vcpu != NULL
 */
int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu);

void vm_assign_virq(struct acrn_vm *vm, uint64_t id);
void interrupts_vm_inject(uint64_t id);
/**
 * @}
 */
/* End of acrn_virq */


#endif /* __AARCH64_VIRQ_H__ */
