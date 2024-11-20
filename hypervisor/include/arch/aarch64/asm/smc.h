/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __AARCH64_SMC_H__
#define __AARCH64_SMC_H__

#define SMC_FAST_CALL		0x80000000
#define SMC_64			0x40000000
#define SMC_ARM_CALL		0x00000000
#define SMC_CPU_CALL		0x01000000
#define SMC_SIP_CALL		0x02000000
#define SMC_OEM_CALL		0x03000000
#define SMC_STD_CALL		0x04000000

#define SMC_FASTCALL_MASK	(1U << 31)
#define SMC_64_MASK		(1U << 30)
#define SMC_OWNER_SHIFT		24
#define SMC_OWNER_MASK		(0x3FU << SMC_OWNER_SHIFT)
#define SMC_FUNCTION_MASK	(0xFFFF)
#define SMC_EXISTING_APIS_END	0x0100FFFFU

enum SMC_OWNER_TYPE {
	SMC_OWNER_ARM = 0,
	SMC_OWNER_CPU = 1,
	SMC_OWNER_SIP = 2,
	SMC_OWNER_OEM = 3,
	SMC_OWNER_STANDARD_SMC = 4,
	SMC_OWNER_STANDARD_HVC = 5,
	SMC_OWNER_VENDOR_HVC = 6,
	SMC_OWNER_RES_START = 7,
	SMC_OWNER_RES_END = 47,
	SMC_OWNER_TRUST_APP_START = 48,
	SMC_OWNER_TRUST_APP_END = 49,
	SMC_OWNER_TRUST_OS_START = 50,
	SMC_OWNER_TRUST_OS_END = 63
};

struct smc_regs {
	uint64_t x0;	/* [in] SMC Funtion ID */
	/* [out] SMC Result register */
	uint64_t x1;	/* [in] Parameter register */
	/* [out] SMC Result register*/
	uint64_t x2;	/* [in] Parameter register */
	/* [out] SMC Result register*/
	uint64_t x3;	/*  [in] Parameter register */
	/* [out] SMC Result register*/
	uint64_t x4;	/* [in] Parameter register */
	uint64_t x5;	/* [in] Parameter register */
	uint64_t x6;	/* [in] Parameter register, Optional Session ID register */
	uint64_t x7;	/* [in] Hypervisor Client ID register */
};

extern void smc_call(struct smc_regs *regs);

#endif /* __AARCH64_SMC_H__ */
