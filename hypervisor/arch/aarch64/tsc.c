/*
 * Copyright (C) 2021 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <asm/cpu_caps.h>
#include <asm/io.h>
#include <asm/tsc.h>
#include <asm/cpu.h>
#include <logmsg.h>
#include <acpi.h>

static uint32_t tsc_khz;

void calibrate_tsc(void)
{
	uint64_t tsc_hz;

	tsc_hz = read_cntfrq_el0();
	tsc_khz = (uint32_t)(tsc_hz / 1000UL);
	pr_acrnlog("%s: tsc_khz = %ld", __func__, tsc_khz);
}

uint32_t get_tsc_khz(void)
{
	return tsc_khz;
}

