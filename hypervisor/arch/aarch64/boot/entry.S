/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/cpu_asm.h>

/* Save state on entry to hypervisor, restore on exit */
.macro	entry
	sub	sp, sp, #ASMOFF_EXCP_SIZE
	stp	x0, x1, [sp, 0x00]
	stp	x2, x3, [sp, 0x10]
	stp	x4, x5, [sp, 0x20]
	stp	x6, x7, [sp, 0x30]
	stp	x8, x9, [sp, 0x40]
	stp	x10, x11, [sp, 0x50]
	stp	x12, x13, [sp, 0x60]
	stp	x14, x15, [sp, 0x70]
	stp	x16, x17, [sp, 0x80]
	stp	x18, x19, [sp, 0x90]
	stp	x20, x21, [sp, 0xa0]
	stp	x22, x23, [sp, 0xb0]
	stp	x24, x25, [sp, 0xc0]
	stp	x26, x27, [sp, 0xd0]
	stp	x28, x29, [sp, 0xe0]
	str	x30, [sp, 0xf0]	/* link register */
.endm

exit:
	ldp	x0, x1, [sp, 0x00]
	ldp	x2, x3, [sp, 0x10]
	ldp	x4, x5, [sp, 0x20]
	ldp	x6, x7, [sp, 0x30]
	ldp	x8, x9, [sp, 0x40]
	ldp	x10, x11, [sp, 0x50]
	ldp	x12, x13, [sp, 0x60]
	ldp	x14, x15, [sp, 0x70]
	ldp	x16, x17, [sp, 0x80]
	ldp	x18, x19, [sp, 0x90]
	ldp	x20, x21, [sp, 0xa0]
	ldp	x22, x23, [sp, 0xb0]
	ldp	x24, x25, [sp, 0xc0]
	ldp	x26, x27, [sp, 0xd0]
	ldp	x28, x29, [sp, 0xe0]
	ldr	x30, [sp, 0xf0]	/* link register */
	add	sp, sp, #ASMOFF_EXCP_SIZE
	eret

.macro	_invoke func
	.align	6	/* align to 0x80 */
	entry
	mov	x0, sp
	bl	\func
	b	exit
.endm

.macro	_invalid func, index
	.align	6	/* align to 0x80 */
	entry
	mov	x0, sp
	mov	x1, \index
	bl	\func
	b	exit
.endm

	.align	11	/* align to 0x800 */
	.global hyp_entry
hyp_entry:
	/* Current Exception level (PSTATE.SP == '0') */
	_invalid	ex_hyp_invalid, #0
	_invalid	ex_hyp_invalid, #1
	_invalid	ex_hyp_invalid, #2
	_invalid	ex_hyp_invalid, #3

	/* Current Exception level (PSTATE.SP == '1') */
	_invoke		ex_hyp_synchronous
	_invoke		ex_hyp_irq
	_invalid	ex_hyp_invalid, #6
	_invoke		ex_hyp_serror

	/* Lower Exception level (AArch64) */
	_invoke		ex_guest_synchronous
	_invoke		ex_guest_irq
	_invalid	ex_guest_invalid, #10
	_invoke		ex_guest_serror

	/* Lower Exception level (AArch32) */
	_invoke		ex_guest_synchronous
	_invoke		ex_guest_irq
	_invalid	ex_guest_invalid, #14
	_invoke		ex_guest_serror

	.global jump_to_vm
jump_to_vm:
	/* 31 general registers, elr_el2 and spsr_el2 are stored at stack. */
	ldr	x1, [x0, #248]
	msr	elr_el2, x1
	ldr	x1, [x0, #256]
	msr	spsr_el2, x1

	ldr	x1, [x0, #8]
	ldp	x2, x3, [x0, #16]
	ldp	x4, x5, [x0, #32]
	ldp	x6, x7, [x0, #48]
	ldp	x8, x9, [x0, #64]
	ldp	x10, x11, [x0, #80]
	ldp	x12, x13, [x0, #96]
	ldp	x14, x15, [x0, #112]
	ldp	x16, x17, [x0, #128]
	ldp	x18, x19, [x0, #144]
	ldp	x20, x21, [x0, #160]
	ldp	x22, x23, [x0, #176]
	ldp	x24, x25, [x0, #192]
	ldp	x26, x27, [x0, #208]
	ldp	x28, x29, [x0, #224]
	ldr	x30, [x0, #240]
	ldr	x0, [x0, #0]

	clrex
	eret

