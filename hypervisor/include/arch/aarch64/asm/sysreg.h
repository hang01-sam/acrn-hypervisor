/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_SYSREG_H__
#define __AARCH64_SYSREG_H__

#include <types.h>

/*******************************************************************************
 * Definitions for CPU system register interface to GICv3
 ******************************************************************************/
#define ICC_IGRPEN1_EL1           S3_0_C12_C12_7
#define ICC_SGI1R                 S3_0_C12_C11_5
#define ICC_ASGI1R                S3_0_C12_C11_6
#define ICC_SRE_EL1               S3_0_C12_C12_5
#define ICC_SRE_EL2               S3_4_C12_C9_5
#define ICC_CTLR_EL1              S3_0_C12_C12_4
#define ICC_PMR_EL1               S3_0_C4_C6_0
#define ICC_RPR_EL1               S3_0_C12_C11_3
#define ICC_IGRPEN0_EL1           S3_0_c12_c12_6
#define ICC_HPPIR0_EL1            S3_0_c12_c8_2
#define ICC_HPPIR1_EL1            S3_0_c12_c12_2
#define ICC_IAR0_EL1              S3_0_c12_c8_0
#define ICC_IAR1_EL1              S3_0_c12_c12_0
#define ICC_EOIR0_EL1             S3_0_c12_c8_1
#define ICC_EOIR1_EL1             S3_0_c12_c12_1
#define ICC_SGI0R_EL1             S3_0_c12_c11_7
#define ICC_BPR1_EL1              S3_0_C12_C12_3
#define ICC_DIR_EL1               S3_0_C12_C11_1

/*******************************************************************************
 * Definitions for hypervisor system register interface to GICv3
 ******************************************************************************/
#define ICH_VSEIR_EL2             S3_4_C12_C9_4
#define ICH_HCR_EL2               S3_4_C12_C11_0
#define ICH_VTR_EL2               S3_4_C12_C11_1
#define ICH_MISR_EL2              S3_4_C12_C11_2
#define ICH_EISR_EL2              S3_4_C12_C11_3
#define ICH_ELRSR_EL2             S3_4_C12_C11_5
#define ICH_VMCR_EL2              S3_4_C12_C11_7

/* DAIF bits definition */
#define DAIF_FIQ_BIT        (1U << 0)
#define DAIF_IRQ_BIT        (1U << 1)
#define DAIF_ABT_BIT        (1U << 2)
#define DAIF_DBG_BIT        (1U << 3)

/* Hypervisor Configuration Register */
#define HCR_FIEN         (1UL << 47)
#define HCR_FWB          (1UL << 46)
#define HCR_NV2          (1UL << 45)
#define HCR_AT           (1UL << 44)
#define HCR_NV1          (1UL << 43)
#define HCR_NV           (1UL << 42)
#define HCR_API          (1UL << 41)
#define HCR_APK          (1UL << 40)
#define HCR_MIOCNCE      (1UL << 38)
#define HCR_TEA          (1UL << 37)
#define HCR_TERR         (1UL << 36)
#define HCR_TLOR         (1UL << 35)
#define HCR_E2H          (1UL << 34)
#define HCR_ID           (1UL << 33)
#define HCR_CD           (1UL << 32)
#define HCR_RW           (1UL << 31)
#define HCR_TRVM         (1UL << 30)
#define HCR_HCD          (1UL << 29)
#define HCR_TDZ          (1UL << 28)
#define HCR_TGE          (1UL << 27)
#define HCR_TVM          (1UL << 26)
#define HCR_TTLB         (1UL << 25)
#define HCR_TPU          (1UL << 24)
#define HCR_TPCP         (1UL << 23)
#define HCR_TSW          (1UL << 22)
#define HCR_TACR         (1UL << 21)
#define HCR_TIDCP        (1UL << 20)
#define HCR_TSC          (1UL << 19)
#define HCR_TID3         (1UL << 18)
#define HCR_TID2         (1UL << 17)
#define HCR_TID1         (1UL << 16)
#define HCR_TID0         (1UL << 15)
#define HCR_TWE          (1UL << 14)
#define HCR_TWI          (1UL << 13)
#define HCR_DC           (1UL << 12)
#define HCR_BSU_NO_EFFECT     (0UL << 10)
#define HCR_BSU_INNER_SH      (1UL << 10)
#define HCR_BSU_OUTER_SH      (2UL << 10)
#define HCR_BSU_FULL_SYS      (3UL << 10)
#define HCR_FB           (1UL << 9)
#define HCR_VSE          (1UL << 8)
#define HCR_VI           (1UL << 7)
#define HCR_VF           (1UL << 6)
#define HCR_AMO          (1UL << 5)
#define HCR_IMO          (1UL << 4)
#define HCR_FMO          (1UL << 3)
#define HCR_PTW          (1UL << 2)
#define HCR_SWIO         (1UL << 1)
#define HCR_VM           (1UL << 0)

/* Exception Class */
#define ESR_EC_WFI_WFE         0b000001
#define ESR_EC_PA              0b001001
#define ESR_EC_SVC_AARCH32     0b010001
#define ESR_EC_HVC_AARCH32     0b010010
#define ESR_EC_SMC_AARCH32     0b010011
#define ESR_EC_SVC_AARCH64     0b010101
#define ESR_EC_HVC_AARCH64     0b010110
#define ESR_EC_SMC_AARCH64     0b010111
#define ESR_EC_SYS_INST        0b011000
#define ESR_EC_ERET            0b011010
#define ESR_EC_INBT_LOWER_EL   0b100000
#define ESR_EC_INBT_CUR_EL     0b100001
#define ESR_EC_PC              0b100010
#define ESR_EC_DABT_LOWER_EL   0b100100
#define ESR_EC_DABT_CUR_EL     0b100101
#define ESR_EC_SP              0b100110
#define ESR_EC_SERROR_INT      0b101111

/* ISS encoding for traps exception class ESR_EC_SYS_INST */
#define ESR_SYS_INST_OP0_SHIFT (20UL)
#define ESR_SYS_INST_OP2_SHIFT (17UL)
#define ESR_SYS_INST_OP1_SHIFT (14UL)
#define ESR_SYS_INST_CRN_SHIFT (10UL)
#define ESR_SYS_INST_RT_BIT    (5UL)
#define ESR_SYS_INST_CRM_SHIFT (1UL)
#define ESR_SYS_INST_DIR_SHIFT (0UL)

/* MASK = 0x3FFC1E */
#define ESR_SYS_INST_MASK ((0b11   << ESR_SYS_INST_OP0_SHIFT) | \
							(0b111  << ESR_SYS_INST_OP2_SHIFT) | \
							(0b111  << ESR_SYS_INST_OP1_SHIFT) | \
							(0b1111 << ESR_SYS_INST_CRN_SHIFT) | \
							(0b1111 << ESR_SYS_INST_CRM_SHIFT))

#define ESR_SYS_INST(op0, op1, crn, crm, op2) \
    ((op0 << ESR_SYS_INST_OP0_SHIFT) | (op1 << ESR_SYS_INST_OP1_SHIFT) | \
     (crn << ESR_SYS_INST_CRN_SHIFT) | (crm << ESR_SYS_INST_CRM_SHIFT) | \
     (op2 << ESR_SYS_INST_OP2_SHIFT))


/* trapped ICC instructions*/
#define ESR_SYS_INST_ICC_SGI1R_EL1          ESR_SYS_INST(3, 0, 12, 11, 5)
#define ESR_SYS_INST_ICC_ASGI1R_EL1         ESR_SYS_INST(3, 1, 12, 11, 6)
#define ESR_SYS_INST_ICC_SGI0R_EL1          ESR_SYS_INST(3, 2, 12, 11, 7)
#define ESR_SYS_INST_ICC_SRE_EL1            ESR_SYS_INST(3, 0, 12, 12, 5)

/*ISS encoding for an exception from a Data Abort*/
#define ESR_DABT_DFSC_SHIFT        (0UL)
#define ESR_DABT_DFSC_MASK         GENMASK(5, 0)
#define ESR_DABT_WnR_SHIFT         (6UL)
#define ESR_DABT_WnR_MASK          BIT_64(ESR_DABT_WnR_SHIFT)
#define ESR_DABT_S1PTW_SHIFT       (7UL)
#define ESR_DABT_S1PTW_MASK        BIT_64(ESR_DABT_S1PTW_SHIFT)
#define ESR_DABT_CM_SHIFT          (8UL)
#define ESR_DABT_CM_MASK           BIT_64(ESR_DABT_CM_SHIFT)
#define ESR_DABT_EA_SHIFT          (9UL)
#define ESR_DABT_EA_MASK           BIT_64(ESR_DABT_EA_SHIFT)
#define ESR_DABT_FnV_SHIFT         (10UL)
#define ESR_DABT_FnV_MASK          BIT_64(ESR_DABT_FnV_SHIFT)
#define ESR_DABT_SET_SHIFT         (11UL)
#define ESR_DABT_SET_MASK          GENMASK(12, 11)
#define ESR_DABT_VNCR_SHIFT        (13UL)
#define ESR_DABT_VNCR_MASK         BIT_64(ESR_DABT_VNCR_SHIFT)
#define ESR_DABT_AR_SHIFT          (14UL)
#define ESR_DABT_AR_MASK           BIT_64(ESR_DABT_AR_SHIFT)
#define ESR_DABT_SF_SHIFT          (15UL)
#define ESR_DABT_SF_MASK           BIT_64(ESR_DABT_SF_SHIFT)
#define ESR_DABT_SRT_SHIFT         (16UL)
#define ESR_DABT_SRT_MASK          GENMASK(20, 16)
#define ESR_DABT_SSE_SHIFT         (21UL)
#define ESR_DABT_SSE_MASK          BIT_64(ESR_DABT_SSE_SHIFT)
#define ESR_DABT_SAS_SHIFT         (22UL)
#define ESR_DABT_SAS_MASK          GENMASK(23, 22)
#define ESR_DABT_ISV_SHIFT         (24UL)
#define ESR_DABT_ISV_MASK          BIT_64(ESR_DABT_ISV_SHIFT)

/* TODO: other trap instructions */

/* MPIDR_EL1 - Multiprocessor Affinity Register */
#define MPIDR_SMP_MT_MASK 0xFF000000
#define MPIDR_AFF0_MAX 4    /* thread */
#define MPIDR_AFF1_MAX 4    /* core */
#define MPIDR_AFF2_MAX 4    /* cluster */
#define MPIDR_AFF3_MAX 4    /* SoC */

#define MPIDR_AFFINITY_MASK (0xff00ffffffULL)
#define MPIDR_AFFLVL_MASK   (0xffULL)
#define MPIDR_AFF0_SHIFT    (0U)
#define MPIDR_AFF1_SHIFT    (8U)
#define MPIDR_AFF2_SHIFT    (16U)
#define MPIDR_AFF3_SHIFT    (32U)

#define MPIDR_AFFLVL0_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL1_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL2_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL3_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF3_SHIFT) & MPIDR_AFFLVL_MASK)

/* SCTLR_EL1 - System Control Register (EL1) */
#define SCTLR_EOS  (1UL << 11)
#define SCTLR_ENRCTX  (1UL << 10)
#define SCTLR_SA  (1UL << 3)
#define SCTLR_C   (1UL << 2)
#define SCTLR_A   (1UL << 1)
#define SCTLR_M   (1UL << 0)

/* VTCR - Virtualization Translation Control Register */
#define VTCR_RES1 (1UL << 31)
#define VTCR_VS_8BIT_VMID (0)
#define VTCR_VS_16BIT_VMID (1UL << 19)
#define VTCR_PS_OFFSET (16)
#define VTCR_PS_MASK (0x7 << VTCR_PS_OFFSET)
#define VTCR_PS_32BITS (0 << 16)
#define VTCR_PS_36BITS (1 << 16)
#define VTCR_PS_40BITS (2 << 16)
#define VTCR_PS_42BITS (3 << 16)
#define VTCR_PS_44BITS (4 << 16)
#define VTCR_PS_48BITS (5 << 16)
#define VTCR_PS_52BITS (6 << 16)
#define VTCR_PS_56BITS (7 << 16)
#define VTCR_TG0_MASK (0x3 << 14)
#define VTCR_TG0_4KB  (0 << 14)
#define VTCR_TG0_64KB (1 << 14)
#define VTCR_TG0_16KB (2 << 14)
#define VTCR_SH0_MASK (0x3 << 12)
#define VTCR_SH0_NS (0 << 12)
#define VTCR_SH0_OS (2 << 12)
#define VTCR_SH0_IS (3 << 12)
#define VTCR_ORGN0_MASK (0x3 << 10)
#define VTCR_ORGN0_NC (0 << 10)
#define VTCR_ORGN0_WBRAWAC (1 << 10)
#define VTCR_ORGN0_WTRANWAC (2 << 10)
#define VTCR_ORGN0_WBRANWAC (3 << 10)
#define VTCR_IRGN0_MASK (0x3 << 8)
#define VTCR_IRGN0_NC (0 << 8)
#define VTCR_IRGN0_WBRAWAC (1 << 8)
#define VTCR_IRGN0_WTRANWAC (2 << 8)
#define VTCR_IRGN0_WBRANWAC (3 << 8)
#define VTCR_SL0_OFFSET (6)
#define VTCR_SL0_MASK (0x3 << 6)
#define VTCR_SL0_LEVEL_23 (0)
#define VTCR_SL0_LEVEL_12 ((0x1UL << VTCR_SL0_OFFSET) & VTCR_SL0_MASK)
#define VTCR_SL0_LEVEL_01 ((0x2UL << VTCR_SL0_OFFSET) & VTCR_SL0_MASK)
#define VTCR_SL0_LEVEL_30 ((0x3UL << VTCR_SL0_OFFSET) & VTCR_SL0_MASK)
#define VTCR_T0SZ_MASK (0x1F << 0)
#define VTCR_T0SZ_OFFSET (0)
#define VTCR_T0SZ(SZ) (((SZ) << VTCR_T0SZ_OFFSET) & VTCR_T0SZ_MASK)

#define PA_RANGE_MASK 0xF /* Mask for PARange in ID_AA64MMFR0_EL1 */
#define PA_RANGE_MAX 0x7
#define PA_RANGE_INVALID 0xF

#define ID_AA64ISAR1_EL1_GPI (0xF << 28)
#define ID_AA64ISAR1_EL1_GPA (0xF << 24)
#define ID_AA64ISAR1_EL1_API (0xF << 8)
#define ID_AA64ISAR1_EL1_APA (0xF << 4)

union sysreg_esr {
	uint64_t value;
	struct {
		uint32_t iss: 25;
		uint32_t il: 1;
		uint32_t ec: 6;
		uint32_t iss2: 5;
		uint32_t pad: 27;
	} __attribute__((__packed__));
};

#endif /* __AARCH64_SYSREG_H__ */
