/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* attr[0] = 0x00(Device-nGnRnE), attr[1] = 0x44(Normal-Non-Cacheable) */
#define MAIR_LOW     0x00004400
/* attr[4] = 0x04(Device-nGnRE), attr[5] = 0xff(Normal-Write-Back-RW allocate) */
#define MAIR_HIGH    0x0000ff04
#define MMU_MAIR     (MAIR_LOW | MAIR_HIGH << 32)
/* bit0-MMU ON bit2-Enable D-cache*/
#define MMU_ON       0x5

/* RES1:1, PS:000 32bits,  TG0:00 4KB, SH0:11 Inner Shareable */
/* ORGN0:01 Normal memory, Outer Write-Back Read-Allocate Write-Allocate Cacheable */
/* IRGN0:01 Normal memory, Inner Write-Back Read-Allocate Write-Allocate Cacheable */
/* T0SZ:10000 */
#define TCR_EL2_DEFAULT    0x80803510

/* RES1:11, EE:0 little-endian, I:0 Non-cacheable, C:0 Non-cacheable */
/* A:0 alignment fault checking diabled, M:0 MMU disabled */
#define SCTLR_EL2_BASE     0x30c50830

/* Table entry bits[11:0]:
   11 | 10 | 9 8 | 7 6 | 5  | 4  3  2  | 1           | 0
   nG | AF | SH  |  AP | NS | AttrIndx | Table/block | valid */
#define DESC_TAB    0b111101110111
#define DESC_RAM    0b111101110101
#define DESC_DEV    0b111001110001

#define PGT_ENTRY_MASK 0x1ff

.pushsection	.header, "awx"
	.global __start
__start:
	nop
	b	0f 		/* code0/code1 */
	.quad	0x80000 	/* image load offset */
	.quad	0 		/* image size */
	.quad	0x08 		/* flags */
	.quad	0 		/* reserved */
	.quad	0 		/* reserved */
	.quad	0 		/* reserved */
	.ascii	"ARM\x64" 	/* magic number */
	.byte	0, 0, 0, 0 	/* reserved */
0:
	b	armv8_primary_start
.popsection

	.global armv8_primary_start
armv8_primary_start:
	mov	x28, #0		/* x28: cpu id */
	b	common_start

	.global armv8_secondary_start
armv8_secondary_start:
	/*
	 * Get CPU ID from MPIDR
	 * cpu_id = (mpidr & 0xF) + ((mpidr >> 8) & 0x3) + (((mpidr >> 16) & 0x3) << 2)
	 */
	mrs	x0, mpidr_el1
	and	x3, x0, #0xf
	lsr	x1, x0, #8
	and	x1, x1, #0x3
	lsr	x2, x0, #16
	and	x2, x2, #0x3
	lsl	x2, x2, #2
	add	x28, x1, x2
	add	x28, x28, x3

common_start:
	/* Disable all exception signals */
	msr	DAIFSet, 0xf

	/* Set CPU ID */
	msr	TPIDR_EL2, x28

	/* Primary CPU do bss clear*/
	cbnz	x28, after_clear_bss

	ldr	x0, =__bss_start
	ldr	x1, =__bss_end
	mov	x2, #0
clear_bss:
	str	x2, [x0], #8
	cmp	x0, x1
	ble	clear_bss

after_clear_bss:
	/* Setup initial stack for each CPU */
	ldr	x0, =__cpu_stacks
	mov	x1, #0x1000
	mul     x1, x1, x28
	add	x0, x0, x1
	add	x0, x0, #0x1000
	mov	SP, x0

	/* Setup SP selection mode */
	msr	spsel, #1

	ldr	x0, =MMU_MAIR
	msr	mair_el2, x0

	/* Set TCR_EL2 with rarange */
	mrs	x0, ID_AA64MMFR0_EL1
	ldr	x1, =TCR_EL2_DEFAULT
	bfi	x1, x0, #16, #3
	msr	tcr_el2, x1

	ldr	x0, =SCTLR_EL2_BASE
	msr	SCTLR_EL2, x0

	/* Make page table */
	ldr	x18, =__start		 /* absolute address */
	adr	x19, __start		 /* runtime address */
	sub	x20, x19, x18		 /* offset */
	ldr	x18, =__boot_pgtable
	add	x18, x18, x20		/* boot pgtable level 0 base addr*/
	msr	TTBR0_EL2, x18		/* Set TTBR0_EL2 with __boot_pgtable*/

	cbnz	x28, turn_on_mmu	/* Secondary CPU not to setup page table */

	ldr	x1, =__boot_first	/* boot pgtable level 1 base addr */
	add	x1, x1, x20		/* x1 := paddr (boot_first) */
	mov	x2, #DESC_TAB		/* x2 := table map */
	orr	x3, x1, x2		/* pgtable level 1 addr is PAGE_SIZE(0x1000) ALIGN */
	str	x3, [x18, #0]		/* store level1 addr to slot 0 */

	/*map __start - __start+1G addr map */
	lsr	x2, x19, #30		/* __start addr index  */
	and	x4, x2, #PGT_ENTRY_MASK
	lsl	x4, x4, #3		/* pgtable level 1 offset */
	lsl	x2, x2, #30		/* x2 := paddr of page table entry */
	mov	x3, #DESC_RAM
	orr	x2, x2, x3		/* Add flags of page table entry */
	str	x2, [x1, x4]		/* Store memory map to pgtable level 1 */

	/*map 0x0 - 0x80000000 addr device map */
	mov	x2, #0x0		/* Map from IO addr 0x0000_0000 for uart*/
	mov	x3, #DESC_DEV
	orr	x2, x2, x3		/* Add flags of page table entry */
	str	x2, [x1, #0]		/* Create mapping */

	b	turn_on_mmu

turn_on_mmu:
	dsb	sy
	isb
	mrs	x0, SCTLR_EL2
	mov	x1, #MMU_ON
	orr	x0, x0, x1
	msr	SCTLR_EL2, x0

	dsb	sy
	isb

	ldr	x2, =mmu_enabled
	br	x2

mmu_enabled:
	tlbi	alle2
	ic	iallu
	dsb	sy
	isb

	/* Jump to C world */
	cbnz	x28, init_secondary_pcpu
	b	init_primary_pcpu
1:	wfe
	b	1b
