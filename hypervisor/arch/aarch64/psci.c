/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <types.h>
#include <logmsg.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <asm/board.h>
#include <asm/psci.h>

extern void armv8_secondary_start(void);
static uint64_t psci_smc_call(uint32_t ftype, uint64_t arg1, uint64_t arg2,
							  uint64_t arg3, bool smc64)
{
	struct smc_regs regs = {0};
	regs.x0 = SMC_FAST_CALL | SMC_STD_CALL | ftype;
	if (smc64) {
		regs.x0 |= SMC_64;
	}
	regs.x1 = arg1;
	regs.x2 = arg2;
	regs.x3 = arg3;
	smc_call(&regs);
	return regs.x0;
}

uint32_t psci_version(__unused uint32_t *version)
{
	return (uint32_t)psci_smc_call(PSCI_FN_VERSION, 0, 0, 0, false);
}

int32_t psci_cpu_on(uint32_t cpu)
{
	uint64_t hw_id;
	uint64_t entry_pa = (uint64_t)armv8_secondary_start;
	uint64_t ret;

	ASSERT(cpu < MAX_PCPU_NUM);
	hw_id = cpuinfo_list[cpu].hwid;

	ret = psci_smc_call(PSCI_FN_CPU_ON, hw_id, entry_pa, 0, true);
	if (!ret) {
		pr_dbg("CPU-%d ON 0x%llx,0x%llx", cpu, hw_id, entry_pa);
	} else {
		pr_err("CPU-%d ON 0x%llx,0x%llx Failed: %d", cpu, hw_id,
			   entry_pa, (int32_t)ret);
	}

	return (int32_t)ret;
}

int32_t psci_cpu_off(void)
{
	uint64_t ret;
	ret = psci_smc_call(PSCI_FN_CPU_OFF, 0, 0, 0, false);
	if (!ret) {
		pr_dbg("CPU-%d OFF", get_pcpu_id());
	} else {
		pr_err("CPU-%d OFF Failed: %d", get_pcpu_id(), (int32_t)ret);
	}
	return (int32_t)ret;
}
