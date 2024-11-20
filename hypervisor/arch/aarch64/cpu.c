/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/arch.h>
#include <asm/lib/bits.h>
#include <asm/page.h>
#include <asm/e820.h>
#include <asm/mmu.h>
#include <asm/guest/ept.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/per_cpu.h>
#include <asm/cpu_caps.h>
#include <version.h>
#include <logmsg.h>
#include <uart16550.h>
#include <reloc.h>
#include <asm/tsc.h>
#include <ticks.h>
#include <delay.h>
#include <asm/psci.h>

#define CPU_UP_TIMEOUT		1000U
#define CPU_DOWN_TIMEOUT	1000U

struct per_cpu_region per_cpu_data[MAX_PCPU_NUM] __aligned(PAGE_SIZE);
static uint64_t pcpu_sync = 0UL;

/* physical cpu active bitmap, support up to 64 cpus */
static volatile uint64_t pcpu_active_bitmap = 0UL;

static void print_hv_banner(void);
static uint64_t start_tick;

uint8_t __cpu_stacks[MAX_PCPU_NUM][CONFIG_STACK_SIZE]
__attribute__((__section__(".bss.cpu_stack")))
__attribute__((aligned(PAGE_SIZE)));

static void pcpu_set_current_state(uint16_t pcpu_id, enum pcpu_boot_state state)
{
	/* Set state for the specified CPU */
	per_cpu(boot_state, pcpu_id) = state;
}

/*
 * @post return MAX_PCPU_NUM
 */
uint16_t get_pcpu_nums(void)
{
	return MAX_PCPU_NUM;
}

bool is_pcpu_active(uint16_t pcpu_id)
{
	return bitmap_test(pcpu_id, &pcpu_active_bitmap);
}

uint64_t get_active_pcpu_bitmap(void)
{
	return pcpu_active_bitmap;
}

void init_pcpu_pre(bool is_bsp)
{
	uint16_t pcpu_id;

	if (is_bsp) {
		pcpu_id = BSP_CPU_ID;
		start_tick = cpu_ticks();

		/* Get CPU capabilities thru CPUID, including the physical address bit
		 * limit which is required for initializing paging.
		 */
		init_pcpu_capabilities();

		init_e820();

		/* reserve ppt buffer from e820 */
		allocate_ppt_pages();

		/* Initialize the hypervisor paging */
		init_paging();

	} else {
		/* Switch this CPU to use the same page tables set-up by the
		 * primary/boot CPU
		 */
		enable_paging();

		pcpu_id = get_pcpu_id();
		if (pcpu_id >= MAX_PCPU_NUM) {
			panic("Invalid pCPU ID!");
		}
	}

	bitmap_set_lock(pcpu_id, &pcpu_active_bitmap);

	/* Set state for this CPU to initializing */
	pcpu_set_current_state(pcpu_id, PCPU_STATE_INITIALIZING);
}

void init_pcpu_post(uint16_t pcpu_id)
{
	traps_init();

	if (pcpu_id == BSP_CPU_ID) {
		/* Print Hypervisor Banner */
		print_hv_banner();

		/* Calibrate TSC Frequency */
		calibrate_tsc();

		pr_acrnlog("HV: %s-%s-%s %s%s%s%s %s@%s build by %s, start time %luus",
				   HV_BRANCH_VERSION, HV_COMMIT_TIME, HV_COMMIT_DIRTY, HV_BUILD_TYPE,
				   (sizeof(HV_COMMIT_TAGS) > 1) ? "(tag: " : "", HV_COMMIT_TAGS,
				   (sizeof(HV_COMMIT_TAGS) > 1) ? ")" : "", HV_BUILD_SCENARIO,
				   HV_BUILD_BOARD, HV_BUILD_USER, ticks_to_us(start_tick));

		pr_dbg("Core %hu is up", BSP_CPU_ID);

		/* Initialize interrupts */
		init_interrupt(BSP_CPU_ID);
		CPU_IRQ_ENABLE_ON_CONFIG();
		timer_init();

		/*
		 * Reserve memory from platform E820 for EPT 4K pages for all VMs
		 */
		reserve_buffer_for_ept_pages();

		pcpu_sync = ALL_CPUS_MASK;
		/* Start all secondary cores */
		if (!start_pcpus(AP_MASK)) {
			panic("Failed to start all secondary cores!");
		}

		ASSERT(get_pcpu_id() == BSP_CPU_ID, "");

	} else {
		pr_info("Core %hu is up", pcpu_id);
		pr_warn("Skipping VM configuration check which should be done before building HV binary.");

		/* Initialize secondary processor interrupts. */
		init_interrupt(pcpu_id);
		CPU_IRQ_ENABLE_ON_CONFIG();

		timer_init();
	}

	init_sched(pcpu_id);

	bitmap_clear_lock(pcpu_id, &pcpu_sync);
	/* Waiting for each pCPU has done its initialization before to continue */
	wait_sync_change(&pcpu_sync, 0UL);
}

/**
 * @brief Start all cpus if the bit is set in mask except itself
 *
 * @param[in] mask bits mask of cpus which should be started
 *
 * @return true if all cpus set in mask are started
 * @return false if there are any cpus set in mask aren't started
 */
bool start_pcpus(uint64_t mask)
{
	uint32_t cpu, timeout;

	pr_info("Secondary cores up process start.");
	/* Boot secondary cores */
	for (cpu = 1; cpu < MAX_PCPU_NUM; cpu++) {
		psci_cpu_on(cpu);
	}

	/* Waiting them boot done */
	for (timeout = CPU_UP_TIMEOUT; timeout != 0; timeout -= 10) {
		invalidate_cacheline(&pcpu_active_bitmap);
		if ((pcpu_active_bitmap & mask) == mask) {
			pr_info("All the cpus already up!");
			break;
		}
		/* Delay 1ms */
		udelay(1000);
	}
	return ((pcpu_active_bitmap & mask) == mask);
}

void make_pcpu_offline(uint16_t pcpu_id)
{
	bitmap_set_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));

	if (get_pcpu_id() != pcpu_id) {
		kick_pcpu(pcpu_id);
	}
}

bool need_offline(uint16_t pcpu_id)
{
	return bitmap_test_and_clear_lock(NEED_OFFLINE, &per_cpu(pcpu_flag, pcpu_id));
}

void wait_pcpus_offline(uint64_t mask)
{
	uint32_t timeout;

	timeout = CPU_DOWN_TIMEOUT * 1000U;
	while (((pcpu_active_bitmap & mask) != 0UL) && (timeout != 0U)) {
		udelay(10U);
		timeout -= 10U;
	}
}

void stop_pcpus(void)
{
	uint16_t pcpu_id;
	uint64_t mask = 0UL;

	for (pcpu_id = 0U; pcpu_id < get_pcpu_nums(); pcpu_id++) {
		if (get_pcpu_id() == pcpu_id) {	/* avoid offline itself */
			continue;
		}

		bitmap_set_nolock(pcpu_id, &mask);
		make_pcpu_offline(pcpu_id);
	}

	/**
	 * Timeout never occurs here:
	 *   If target cpu received a NMI and panic, it has called cpu_dead and make_pcpu_offline success.
	 *   If target cpu is running, an IPI will be delivered to it and then call cpu_dead.
	 */
	wait_pcpus_offline(mask);
}

void cpu_do_idle(void)
{
	asm_pause();
}

/**
 * only run on current pcpu
 */
void cpu_dead(void)
{
	/* For debug purposes, using a stack variable in the while loop enables
	 * us to modify the value using a JTAG probe and resume if needed.
	 */
	int32_t halt = 1;
	uint16_t pcpu_id = get_pcpu_id();

	deinit_sched(pcpu_id);
	if (bitmap_test(pcpu_id, &pcpu_active_bitmap)) {

		flush_cache_range((const volatile void *)CONFIG_HV_RAM_START, (uint64_t)CONFIG_HV_RAM_SIZE);

		/* Set state to show CPU is dead */
		pcpu_set_current_state(pcpu_id, PCPU_STATE_DEAD);
		bitmap_clear_lock(pcpu_id, &pcpu_active_bitmap);

		/* Halt the CPU */
		do {
			asm_hlt();
		} while (halt != 0);
	} else {
		pr_err("pcpu%hu already dead", pcpu_id);
	}
}

static void print_hv_banner(void)
{
	const char *boot_msg = "ACRN Hypervisor\n\r";

	/* Print the boot message */
	printf(boot_msg);
}

/* wait until *sync == wake_sync */
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync)
{
	while ((*sync) != wake_sync) {
		mb();
		isb();
	}
}

/*
 * Get MPIDR from CPU ID
 */
uint64_t cpu_id_to_mpidr(uint16_t cpuid)
{
	return (read_mpidr_el1() & MPIDR_SMP_MT_MASK) | cpuinfo_list[cpuid].hwid;
}

/*
 * Get CPU ID from MPIDR
 */
uint16_t mpidr_to_cpu_id(uint64_t mpidr)
{
	uint16_t cpuid;
	uint64_t hwid = mpidr & (~MPIDR_SMP_MT_MASK);

	for (cpuid = 0; cpuid < MAX_PCPU_NUM; cpuid++)
		if (cpuinfo_list[cpuid].hwid == hwid) {
			break;
		}

	if (cpuid == MAX_PCPU_NUM) {
		panic("get cpuid ERROR!");
	}

	return cpuid;
}

