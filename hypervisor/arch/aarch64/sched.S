/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Function schedule() will finally call arch_switch_to here, which use
 * the pointer of previous & next thread_obj->host_sp as the input parameters.
 */
   .align       8
   .global      arch_switch_to
arch_switch_to:
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!

/*
 TODO: we need vcpu proc_regs save/restore here
 but there is issue that, the original pcpu idle thread has no vcpu context
 so if save vcpu here for the first switch of pcpu idle thread==> vcpu thread
 it would occur error
*/

	/* bl vgic_switch */ /* TODO */

	mov	x27, sp
	str	x27, [x0] //save old sp
	ldr	x28, [x1]
	mov	sp, x28   //restore new sp

	/* msr tpidr_el2, ** */ /* TODO: do we need current_vcpu? */

	ldp	x19, x20, [sp], #16
	ldp	x21, x22, [sp], #16
	ldp	x23, x24, [sp], #16
	ldp	x25, x26, [sp], #16
	ldp	x27, x28, [sp], #16
	ldp	x29, x30, [sp], #16

	mov 	x0, x1
	mov	x1, #0x30 /* offset of host_sp to therad_obj */
	sub	x0, x0, x1 /* get thread_obj addr as 1st para */

	ret

	.type	arch_switch_to, function
	.size	arch_switch_to, . - arch_switch_to
