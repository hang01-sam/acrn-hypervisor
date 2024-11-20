/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* void smc_call(struct smc_regs *regs); */
	.global smc_call
smc_call:
	str	x19, [sp, #-8]!
	mov	x19, x0

	/* Parameter regsiters */
	ldp	x0, x1, [x19, #0]
	ldp	x2, x3, [x19, #16]
	ldp	x4, x5, [x19, #32]
	ldp	x6, x7, [x19, #48]

	isb
	dsb     sy
	dmb     sy
	smc	#0

	/* SMC Result registers */
	stp	x0, x1, [x19, #0]
	stp	x2, x3, [x19, #16]

	ldr	x19, [sp], #8
	ret

