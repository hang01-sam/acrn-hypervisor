/*
 * Copyright (c) 2016-2018, ARM Limited and Contributors. All rights reserved.
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __AARCH64_ARCH_H__
#define __AARCH64_ARCH_H__

#include <asm/sysreg.h>

typedef unsigned long int	uintptr_t;
typedef long register_t;
typedef unsigned long u_register_t;

#define ID_AA64PFR0_GIC_MASK                  ULL(0xf)
#define ID_AA64PFR0_GIC_SHIFT                 U(24)

#define SCTLR_C_BIT		(ULL(1) << 2)
#define SCR_NS_BIT		(UL(1) << 0)

/**********************************************************************
 * Macros which create inline functions to read or write CPU system
 * registers
 *********************************************************************/

#define _DEFINE_SYSREG_READ_FUNC(_name, _reg_name)		\
static inline u_register_t read_ ## _name(void)			\
{								\
	u_register_t v;						\
	__asm__ volatile ("mrs %0, " #_reg_name : "=r" (v));	\
	return v;						\
}

#define _DEFINE_SYSREG_READ_FUNC_NV(_name, _reg_name)		\
static inline u_register_t read_ ## _name(void)			\
{								\
	u_register_t v;						\
	__asm__ ("mrs %0, " #_reg_name : "=r" (v));		\
	return v;						\
}

#define _DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)			\
static inline void write_ ## _name(u_register_t v)			\
{									\
	__asm__ volatile ("msr " #_reg_name ", %0" : : "r" (v));	\
}

#define SYSREG_WRITE_CONST(reg_name, v)				\
	__asm__ volatile ("msr " #reg_name ", %0" : : "i" (v))

/* Define read function for system register */
#define DEFINE_SYSREG_READ_FUNC(_name) 			\
	_DEFINE_SYSREG_READ_FUNC(_name, _name)

/* Define read & write function for system register */
#define DEFINE_SYSREG_RW_FUNCS(_name)			\
	_DEFINE_SYSREG_READ_FUNC(_name, _name)		\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _name)

/* Define read & write function for renamed system register */
#define DEFINE_RENAME_SYSREG_RW_FUNCS(_name, _reg_name)	\
	_DEFINE_SYSREG_READ_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)

/* Define read function for renamed system register */
#define DEFINE_RENAME_SYSREG_READ_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_READ_FUNC(_name, _reg_name)

/* Define write function for renamed system register */
#define DEFINE_RENAME_SYSREG_WRITE_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)

/* Define read function for ID register (w/o volatile qualifier) */
#define DEFINE_IDREG_READ_FUNC(_name)			\
	_DEFINE_SYSREG_READ_FUNC_NV(_name, _name)

/* Define read function for renamed ID register (w/o volatile qualifier) */
#define DEFINE_RENAME_IDREG_READ_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_READ_FUNC_NV(_name, _reg_name)

/**********************************************************************
 * Macros to create inline functions for system instructions
 *********************************************************************/

/* Define function for simple system instruction */
#define DEFINE_SYSOP_FUNC(_op)				\
static inline void _op(void)				\
{							\
	__asm__ (#_op : : : "memory");					\
}

/* Define function for system instruction with register parameter */
#define DEFINE_SYSOP_PARAM_FUNC(_op)			\
static inline void _op(uint64_t v)			\
{							\
	 __asm__ (#_op "  %0" : : "r" (v));		\
}

/* Define function for system instruction with type specifier */
#define DEFINE_SYSOP_TYPE_FUNC(_op, _type)		\
static inline void _op ## _type(void)			\
{							\
	__asm__ (#_op " " #_type : : : "memory");			\
}

/* Define function for system instruction with register parameter */
#define DEFINE_SYSOP_TYPE_PARAM_FUNC(_op, _type)	\
static inline void _op ## _type(uint64_t v)		\
{							\
	 __asm__ (#_op " " #_type ", %0" : : "r" (v));	\
}

#define write_daifclr(val) SYSREG_WRITE_CONST(daifclr, val)
#define write_daifset(val) SYSREG_WRITE_CONST(daifset, val)

DEFINE_SYSREG_READ_FUNC(mpidr_el1)
DEFINE_IDREG_READ_FUNC(id_aa64pfr0_el1)
DEFINE_IDREG_READ_FUNC(id_aa64mmfr0_el1)
DEFINE_IDREG_READ_FUNC(id_aa64isar1_el1)

DEFINE_RENAME_SYSREG_RW_FUNCS(par_el1, PAR_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(hpfar_el2, HPFAR_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(far_el2, FAR_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(hcr_el2, HCR_EL2)
DEFINE_SYSREG_RW_FUNCS(tpidr_el2)
DEFINE_SYSREG_RW_FUNCS(elr_el2)
DEFINE_SYSREG_RW_FUNCS(spsr_el2)
DEFINE_SYSREG_RW_FUNCS(esr_el2)
DEFINE_SYSREG_RW_FUNCS(sp_el1)
DEFINE_SYSREG_RW_FUNCS(sp_el0)

DEFINE_RENAME_SYSREG_RW_FUNCS(vttbr_el2, VTTBR_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(vtcr_el2, VTCR_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(sctlr_el1, SCTLR_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(vmpidr_el2, VMPIDR_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(vbar_el2, VBAR_EL2)

/* GICv3 System Registers */
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_sre_el1, ICC_SRE_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_sre_el2, ICC_SRE_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_pmr_el1, ICC_PMR_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_ctlr_el1, ICC_CTLR_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_bpr1_el1, ICC_BPR1_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_rpr_el1, ICC_RPR_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_igrpen1_el1, ICC_IGRPEN1_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_igrpen0_el1, ICC_IGRPEN0_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_hppir0_el1, ICC_HPPIR0_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_hppir1_el1, ICC_HPPIR1_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_iar0_el1, ICC_IAR0_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_iar1_el1, ICC_IAR1_EL1)
DEFINE_RENAME_SYSREG_WRITE_FUNC(icc_eoir0_el1, ICC_EOIR0_EL1)
DEFINE_RENAME_SYSREG_WRITE_FUNC(icc_eoir1_el1, ICC_EOIR1_EL1)
DEFINE_RENAME_SYSREG_WRITE_FUNC(icc_dir_el1, ICC_DIR_EL1)
DEFINE_RENAME_SYSREG_WRITE_FUNC(icc_sgi0r_el1, ICC_SGI0R_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_sgi1r, ICC_SGI1R)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_asgi1r, ICC_ASGI1R)

/* Timer System Registers */
DEFINE_RENAME_SYSREG_READ_FUNC(cntpct_el0, CNTPCT_EL0)
DEFINE_RENAME_SYSREG_READ_FUNC(cntvct_el0, CNTVCT_EL0)
DEFINE_RENAME_SYSREG_RW_FUNCS(cntvoff_el2, CNTVOFF_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(cnthctl_el2, CNTHCTL_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(cnthp_ctl_el2, CNTHP_CTL_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(cnthp_cval_el2, CNTHP_CVAL_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(cntfrq_el0, CNTFRQ_EL0)
DEFINE_RENAME_SYSREG_RW_FUNCS(cntp_ctl_el0, CNTP_CTL_EL0)
DEFINE_RENAME_SYSREG_RW_FUNCS(cntv_ctl_el0, CNTV_CTL_EL0)

/* System instructons */
DEFINE_SYSOP_FUNC(wfi)
DEFINE_SYSOP_FUNC(wfe)
DEFINE_SYSOP_FUNC(sev)
DEFINE_SYSOP_FUNC(nop)
DEFINE_SYSOP_TYPE_FUNC(dsb, sy)
DEFINE_SYSOP_TYPE_FUNC(dmb, sy)
DEFINE_SYSOP_TYPE_FUNC(dmb, st)
DEFINE_SYSOP_TYPE_FUNC(dmb, ld)
DEFINE_SYSOP_TYPE_FUNC(dsb, ish)
DEFINE_SYSOP_TYPE_FUNC(dsb, osh)
DEFINE_SYSOP_TYPE_FUNC(dsb, nsh)
DEFINE_SYSOP_TYPE_FUNC(dsb, ishst)
DEFINE_SYSOP_TYPE_FUNC(dsb, oshst)
DEFINE_SYSOP_TYPE_FUNC(dmb, oshld)
DEFINE_SYSOP_TYPE_FUNC(dmb, oshst)
DEFINE_SYSOP_TYPE_FUNC(dmb, osh)
DEFINE_SYSOP_TYPE_FUNC(dmb, nshld)
DEFINE_SYSOP_TYPE_FUNC(dmb, nshst)
DEFINE_SYSOP_TYPE_FUNC(dmb, nsh)
DEFINE_SYSOP_TYPE_FUNC(dmb, ishld)
DEFINE_SYSOP_TYPE_FUNC(dmb, ishst)
DEFINE_SYSOP_TYPE_FUNC(dmb, ish)
DEFINE_SYSOP_FUNC(isb)
DEFINE_SYSOP_FUNC(esb)

#define mb()            dsbsy()
#define rmb()           dsbld()
#define wmb()           dsbst()
#define smp_mb()        dmbish()
#define smp_wmb()       dmbishst()
#define smp_rmb()       dmbishld()

static inline void asm_pause(void)
{
    dsbsy();
    wfi();
}

static inline void asm_hlt(void)
{
hlt:  wfe();

    goto hlt;
}

static inline void stac(void)
{
    nop();
}

static inline void clac(void)
{
    nop();
}

/* Synchronizes all write accesses to memory */
static inline void cpu_write_memory_barrier(void)
{
    smp_wmb();
}

/* Synchronizes all read and write accesses to/from memory */
static inline void cpu_memory_barrier(void)
{
    smp_mb();
}

#define COMPILER_BARRIER() __asm__ volatile ("" ::: "memory")

static inline void enable_irq(void)
{
	/*
	 * The compiler memory barrier will prevent the compiler from
	 * scheduling non-volatile memory access after the write to the
	 * register.
	 *
	 * This could happen if some initialization code issues non-volatile
	 * accesses to an area used by an interrupt handler, in the assumption
	 * that it is safe as the interrupts are disabled at the time it does
	 * that (according to program order). However, non-volatile accesses
	 * are not necessarily in program order relatively with volatile inline
	 * assembly statements (and volatile accesses).
	 */
	COMPILER_BARRIER();
	write_daifclr(DAIF_IRQ_BIT);
	isb();
}

static inline void enable_fiq(void)
{
	COMPILER_BARRIER();
	write_daifclr(DAIF_FIQ_BIT);
	isb();
}

static inline void enable_serror(void)
{
	COMPILER_BARRIER();
	write_daifclr(DAIF_ABT_BIT);
	isb();
}

static inline void enable_debug_exceptions(void)
{
	COMPILER_BARRIER();
	write_daifclr(DAIF_DBG_BIT);
	isb();
}

static inline void disable_irq(void)
{
	COMPILER_BARRIER();
	write_daifset(DAIF_IRQ_BIT);
	isb();
}

static inline void disable_fiq(void)
{
	COMPILER_BARRIER();
	write_daifset(DAIF_FIQ_BIT);
	isb();
}

static inline void disable_serror(void)
{
	COMPILER_BARRIER();
	write_daifset(DAIF_ABT_BIT);
	isb();
}

static inline void disable_debug_exceptions(void)
{
	COMPILER_BARRIER();
	write_daifset(DAIF_DBG_BIT);
	isb();
}

static inline void flush_dcache_range(__unused uintptr_t addr, __unused size_t size)
{
	// TODO
}

#endif /* __AARCH64_ARCH_H__ */
