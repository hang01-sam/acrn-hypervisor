/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/page.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <errno.h>
#include <logmsg.h>

/*
Record all the CPU capabilities here, such as:
- Physical Address range supported(pa_range)
- Pointer Authentication(pauth)
...
*/
static uint32_t pa_range = PA_RANGE_INVALID;
static bool has_pauth_feature = false;

uint32_t get_pa_range(void)
{
	return pa_range;
}

void init_pa_range(void)
{
	if (pa_range == PA_RANGE_INVALID) {
		pa_range = read_id_aa64mmfr0_el1() & PA_RANGE_MASK;
	}

	if (pa_range > PA_RANGE_MAX) {
		pr_err("init parange error! parange value: %d", pa_range);
		pa_range = PA_RANGE_INVALID;
	}
}

static inline void init_pauth(void)
{
	uint64_t reg = read_id_aa64isar1_el1();
	reg &= (ID_AA64ISAR1_EL1_APA | ID_AA64ISAR1_EL1_API |
			ID_AA64ISAR1_EL1_GPA | ID_AA64ISAR1_EL1_GPI);

	has_pauth_feature = (reg != 0 ? true : false);

	pr_dbg("pauth feature: %s", has_pauth_feature ? "ON" : "OFF");
}

void init_pcpu_capabilities(void)
{
	init_pa_range();
	init_pauth();
}

