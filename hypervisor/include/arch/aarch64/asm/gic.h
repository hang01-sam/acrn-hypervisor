#ifndef __AARCH64_GIC_H__
#define __AARCH64_GIC_H__

#include <types.h>
#include <irq.h>
#include <asm/sysreg.h>
#include <asm/cpu.h>
#include <asm/lib/bits.h>

/*
 * For those constants to be shared between C and other sources, apply a 'U',
 * 'UL', 'ULL', 'L' or 'LL' suffix to the argument only in C, to avoid
 * undefined or unintended behaviour.
 *
 * The GNU assembler and linker do not support these suffixes (it causes the
 * build process to fail) therefore the suffix is omitted when used in linker
 * scripts and assembler files.
*/
#if defined(__ASSEMBLER__)
# define   U(_x)	(_x)
# define  UL(_x)	(_x)
# define ULL(_x)	(_x)
# define   L(_x)	(_x)
# define  LL(_x)	(_x)
#else
# define  U_(_x)	(_x##U)
# define   U(_x)	U_(_x)
# define  UL_(_x)	(_x##UL)
# define  UL(_x)	UL_(_x)
# define  ULL_(_x)	(_x##ULL)
# define  ULL(_x)	ULL_(_x)
# define  L_(_x)	(_x##L)
# define  L(_x)	L_(_x)
# define  LL_(_x)	(_x##LL)
# define  LL(_x)	LL_(_x)

#endif

#define NR_GIC_LOCAL_IRQS  (32)

typedef struct gic_ops {
	void (*init)(uint16_t pcpu_id);
	uint32_t (*ack_irq)(void);
	void (*raise_sgi)(int sgi_num, uint32_t cpumask);
	void (*enable_irq)(uint32_t id);
	void (*disable_irq)(uint32_t id);
	void (*set_irq_pending)(uint32_t id);
	void (*clear_irq_pending)(uint32_t id);
	void (*set_irq_cfg)(uint32_t id, uint32_t cfg);
	void (*set_irq_priority)(uint32_t id, uint32_t priority);
	void (*end_irq)(uint32_t id);
	void (*deactivate_irq)(uint32_t id);
} gic_ops_t;

enum gic_sgi {
	GIC_SGI_WAKEUP,
	GIC_SGI_LIMIT,
};

enum gic_sgi_mode {
	SGI_TARGET_LIST,
	SGI_TARGET_OTHERS,
};

void gic_register_ops(gic_ops_t *ops);
void gic_irq_hdl(void);
void gic_pre_init(void);
void gic_init(uint16_t pcpu_id);
void raise_sgi_mask(uint32_t cpumask, enum gic_sgi sgi);
void raise_sgi_one(uint32_t cpu, enum gic_sgi sgi);
void raise_sgi_self(enum gic_sgi sgi);
void raise_sgi_allbutself(enum gic_sgi sgi);
void gic_set_pending_state(struct irq_desc *irqd, bool pending);
void gic_enable_irq(uint32_t id);
void gic_disable_irq(uint32_t id);
void gic_set_irq_pending(uint32_t id);
void gic_clear_irq_pending(uint32_t id);
void gic_set_irq_cfg(uint32_t id, uint32_t cfg);
void gic_set_irq_priority(uint32_t id, uint32_t priority);
void gic_end_irq(uint32_t id);
void gic_deactivate_irq(uint32_t id);

#endif
