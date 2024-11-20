/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/init.h>
#include <console.h>
#include <asm/per_cpu.h>
#include <shell.h>
#include <asm/guest/vm.h>
#include <logmsg.h>
#include <asm/sysreg.h>
#include <boot.h>

#define SWITCH_TO(rsp, to)				\
{							\
	asm volatile ("mov sp,%0; b "			\
	#to : : "r" (rsp) : "memory");	\
}

/*TODO: move into debug module */
static void init_debug_pre(void)
{
	/* Initialize console */
	console_init();

	/* Enable logging */
	init_logmsg();
}

/*TODO: move into debug module */
static void init_debug_post(uint16_t pcpu_id)
{
	if (pcpu_id == BSP_CPU_ID) {
		/* Initialize the shell */
		shell_init();
		console_setup_timer();
	}
}

/*TODO: move into guest-vcpu module */
__unused static void init_guest_mode(uint16_t pcpu_id)
{
	launch_vms(pcpu_id);
}

void init_pcpu_comm_post(void)
{
	uint16_t pcpu_id;

	pcpu_id = get_pcpu_id();

	init_pcpu_post(pcpu_id);
	init_debug_post(pcpu_id);
	/* init_guest_mode(pcpu_id); */
	run_idle_thread();
}

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void init_primary_pcpu(void)
{
	uint64_t stackbottom, stacktop;

	init_debug_pre();

	init_pcpu_pre(true);

	/* Switch to run-time stack */
	stackbottom = (uint64_t)(&get_cpu_var(stack)[0]);
	stacktop = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	stacktop &= ~(CPU_STACK_ALIGN - 1UL);
	pr_info("cpu%u stack: [0x%lx ~ 0x%lx]", get_pcpu_id(), stackbottom, stacktop);
	SWITCH_TO(stacktop, init_pcpu_comm_post);
}

void init_secondary_pcpu(void)
{
	uint64_t stackbottom, stacktop;

	init_pcpu_pre(false);

	/* Switch to run-time stack */
	stackbottom = (uint64_t)(&get_cpu_var(stack)[0]);
	stacktop = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	stacktop &= ~(CPU_STACK_ALIGN - 1UL);
	pr_info("cpu%u stack: [0x%lx ~ 0x%lx]", get_pcpu_id(), stackbottom, stacktop);
	SWITCH_TO(stacktop, init_pcpu_comm_post);
}
