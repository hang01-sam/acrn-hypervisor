#ifndef __RISCV_DEFCONFIG_H__
#define __RISCV_DEFCONFIG_H__

//********** board specific area *************/
#define RUN_ON_QEMU
#define BOARD_FILE "qemu.h"
/*********************end********************/

#define CONFIG_HAS_DEVICE_TREE 1
#define CONFIG_RISCV64 1
#define CONFIG_SCHED_IORR 1
#define CONFIG_HAS_FAST_MULTIPLY 1
#define CONFIG_CC_HAS_VISIBILITY_ATTRIBUTE 1
#define CONFIG_DEBUG_LOCKS 1
#define CONFIG_DEBUG 1
#define CONFIG_TIMER_IRQ 26
#define CONFIG_SERIAL_BASE 0x10000000
#define CONFIG_LOG_LEVEL 5
#define CONFIG_TEXT_SIZE 0x4000000
#define CONFIG_STACK_SIZE 0x10000
#ifdef CONFIG_MACRN
#define CONFIG_MSTACK_SIZE 0x10000
#else
#define CONFIG_MSTACK_SIZE 0x1000
#endif
#define CONFIG_UPCALL_IRQ 40
#define CONFIG_UOS_VIRTIO_BLK_IRQ 79
#define CONFIG_UOS_VIRTIO_NET_IRQ 80

#define CONFIG_GUEST_ADDRESS_SPACE_SIZE  0x100000000
#define CONFIG_MAX_EMULATED_MMIO_REGIONS 32

#endif /* __RISCV_DEFCONFIG_H__ */
