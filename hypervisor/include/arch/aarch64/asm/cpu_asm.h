/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_CPU_ASM_H__
#define __AARCH64_CPU_ASM_H__

/* Offset in struct intr_excp_ctx */
#define ASMOFF_EXCP_SP		(8*29)
#define ASMOFF_EXCP_LR		(8*30)
#define ASMOFF_EXCP_PC		(ASMOFF_EXCP_LR + 8)
#define ASMOFF_EXCP_SPSR	(ASMOFF_EXCP_PC + 8)
#define ASMOFF_EXCP_ESR		(ASMOFF_EXCP_SPSR + 8)
#define ASMOFF_EXCP_SPSR_EL1	(ASMOFF_EXCP_ESR + 8)
#define ASMOFF_EXCP_SP_EL0	(ASMOFF_EXCP_SPSR_EL1 + 8)
#define ASMOFF_EXCP_SP_EL1	(ASMOFF_EXCP_SP_EL0 + 8)
#define ASMOFF_EXCP_ELR_EL1	(ASMOFF_EXCP_SP_EL1 + 8)
#define ASMOFF_EXCP_SIZE	(ASMOFF_EXCP_ELR_EL1 + 8)
#endif /* __AARCH64_CPU_ASM_H__ */
