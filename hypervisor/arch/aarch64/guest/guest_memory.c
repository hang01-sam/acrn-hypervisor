/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/guest/guest_memory.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/mmu.h>
#include <asm/guest/ept.h>
#include <logmsg.h>

struct page_walk_info {
	uint64_t top_entry;	/* Top level paging structure entry */
	uint32_t level;
	uint32_t width;
	bool is_user_mode_access;
	bool is_write_access;
	bool is_inst_fetch;
	bool pse;		/* CR4.PSE for 32bit paing,
				 * true for PAE/4-level paing */
	bool wp;		/* CR0.WP */
	bool nxe;		/* MSR_IA32_EFER_NXE_BIT */

	bool is_smap_on;
	bool is_smep_on;
};

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu)
{
	enum vm_paging_mode ret = PAGING_MODE_0_LEVEL;	/* non-paging */

	if (is_paging_enabled(vcpu)) {
		if (is_pae(vcpu)) {
			if (is_long_mode(vcpu)) {
				ret = PAGING_MODE_4_LEVEL;	/* 4-level paging */
			} else {
				ret = PAGING_MODE_3_LEVEL;	/* PAE paging */
			}
		} else {
			ret = PAGING_MODE_2_LEVEL;	/* 32-bit paging */
		}
	}

	return ret;
}

/* Refer to SDM Vol.3A 6-39 section 6.15 for the format of paging fault error
 * code.
 *
 * Caller should set the contect of err_code properly according to the address
 * usage when calling this function:
 * - If it is an address for write, set PAGE_FAULT_WR_FLAG in err_code.
 * - If it is an address for instruction featch, set PAGE_FAULT_ID_FLAG in
 *   err_code.
 * Caller should check the return value to confirm if the function success or
 * not.
 * If a protection volation detected during page walk, this function still will
 * give the gpa translated, it is up to caller to decide if it need to inject a
 * #PF or not.
 * - Return 0 for success.
 * - Return -EINVAL for invalid parameter.
 * - Return -EFAULT for paging fault, and refer to err_code for paging fault
 *   error code.
 */
int32_t gva2gpa(__unused struct acrn_vcpu *vcpu, __unused uint64_t gva, uint64_t *gpa,
				uint32_t *err_code)
{
	int32_t ret = 0;

	if ((gpa == NULL) || (err_code == NULL)) {
		ret = -EINVAL;
	} else {
		*gpa = 0UL;
	}

	return ret;
}

static inline uint32_t local_copy_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa,
									  uint32_t size, uint32_t fix_pg_size, bool cp_from_vm)
{
	uint64_t hpa;
	uint32_t offset_in_pg, len, pg_size;
	void *g_ptr;

	hpa = local_gpa2hpa(vm, gpa, &pg_size);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping",
			   __func__, vm->vm_id, gpa);
		len = 0U;
	} else {

		if (fix_pg_size != 0U) {
			pg_size = fix_pg_size;
		}

		offset_in_pg = (uint32_t)gpa & (pg_size - 1U);
		len = (size > (pg_size - offset_in_pg)) ? (pg_size - offset_in_pg) : size;

		g_ptr = hpa2hva(hpa);

		stac();
		if (cp_from_vm) {
			(void)memcpy_s(h_ptr, len, g_ptr, len);
		} else {
			(void)memcpy_s(g_ptr, len, h_ptr, len);
		}
		clac();
	}

	return len;
}

static inline int32_t copy_gpa(struct acrn_vm *vm, void *h_ptr_arg, uint64_t gpa_arg,
							   uint32_t size_arg, bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint32_t len;
	uint64_t gpa = gpa_arg;
	uint32_t size = size_arg;
	int32_t err = 0;

	while (size > 0U) {
		len = local_copy_gpa(vm, h_ptr, gpa, size, 0U, cp_from_vm);
		if (len == 0U) {
			err = -EINVAL;
			break;
		}
		gpa += len;
		h_ptr += len;
		size -= len;
	}

	return err;
}

/*
 * @pre vcpu != NULL && err_code != NULL && h_ptr_arg != NULL
 */
static inline int32_t copy_gva(struct acrn_vcpu *vcpu, void *h_ptr_arg, uint64_t gva_arg,
							   uint32_t size_arg, uint32_t *err_code, uint64_t *fault_addr,
							   bool cp_from_vm)
{
	void *h_ptr = h_ptr_arg;
	uint64_t gpa = 0UL;
	int32_t ret = 0;
	uint32_t len;
	uint64_t gva = gva_arg;
	uint32_t size = size_arg;

	while ((size > 0U) && (ret == 0)) {
		ret = gva2gpa(vcpu, gva, &gpa, err_code);
		if (ret >= 0) {
			len = local_copy_gpa(vcpu->vm, h_ptr, gpa, size, PAGE_SIZE_4K, cp_from_vm);
			if (len != 0U) {
				gva += len;
				h_ptr += len;
				size -= len;
			} else {
				ret =  -EINVAL;
			}
		} else {
			*fault_addr = gva;
			pr_err("error[%d] in GVA2GPA, err_code=0x%x", ret, *err_code);
		}
	}

	return ret;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	int32_t ret = 0;

	ret = copy_gpa(vm, h_ptr, gpa, size, 1);
	if (ret != 0) {
		pr_err("Unable to copy GPA 0x%llx from VM%d to HPA 0x%llx\n", gpa, vm->vm_id, (uint64_t)h_ptr);
	}

	return ret;
}

/* @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	int32_t ret = 0;

	ret = copy_gpa(vm, h_ptr, gpa, size, 0);
	if (ret != 0) {
		pr_err("Unable to copy HPA 0x%llx to GPA 0x%llx in VM%d\n", (uint64_t)h_ptr, gpa, vm->vm_id);
	}

	return ret;
}

int32_t copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
					  uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	return copy_gva(vcpu, h_ptr, gva, size, err_code, fault_addr, 1);
}

int32_t copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
					uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	return copy_gva(vcpu, h_ptr, gva, size, err_code, fault_addr, false);
}

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{
	uint64_t hpa = gpa2hpa(vm, x);
	return (hpa == INVALID_HPA) ? NULL : hpa2hva(hpa);
}
