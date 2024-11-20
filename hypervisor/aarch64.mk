#
# acrn-hypervisor/hypervisor/Makefile
#
CROSS_COMPILE ?= aarch64-linux-gnu-
API_MAJOR_VERSION=1
API_MINOR_VERSION=0

GCC_MAJOR=$(shell echo __GNUC__ | $(CC) -E -x c - | tail -n 1)
GCC_MINOR=$(shell echo __GNUC_MINOR__ | $(CC) -E -x c - | tail -n 1)

BASEDIR := $(shell pwd)
HV_OBJDIR ?= $(CURDIR)/build
HV_MODDIR ?= $(HV_OBJDIR)/modules
HV_FILE := acrn

LIB_MOD = $(HV_MODDIR)/lib_mod.a
BOOT_MOD = $(HV_MODDIR)/boot_mod.a
HW_MOD = $(HV_MODDIR)/hw_mod.a
VP_BASE_MOD = $(HV_MODDIR)/vp_base_mod.a
VP_DM_MOD = $(HV_MODDIR)/vp_dm_mod.a
VP_TRUSTY_MOD = $(HV_MODDIR)/vp_trusty_mod.a
VP_X86_TEE_MOD = $(HV_MODDIR)/vp_x86_tee_mod.a
VP_HCALL_MOD = $(HV_MODDIR)/vp_hcall_mod.a
LIB_DEBUG = $(HV_MODDIR)/libdebug.a
LIB_RELEASE = $(HV_MODDIR)/librelease.a
SYS_INIT_MOD = $(HV_MODDIR)/sys_init_mod.a

# initialize the flags we used
CFLAGS := -D__aarch64__
ASFLAGS := -D__aarch64__
LDFLAGS :=
ARFLAGS :=
ARCH_CFLAGS := -march=armv8.2-a
ARCH_ASFLAGS :=
ARCH_ARFLAGS :=
ARCH_LDFLAGS :=

include scripts/makefile/config.mk

BOARD_INFO_DIR := $(HV_CONFIG_DIR)/boards
SCENARIO_CFG_DIR := $(HV_CONFIG_DIR)/scenarios/$(SCENARIO)
BOARD_CFG_DIR := $(SCENARIO_CFG_DIR)

include ../paths.make

LD_IN_TOOL = scripts/genld.sh
BASH = $(shell which bash)

ARFLAGS += crs

CFLAGS += -Wall -W
CFLAGS += -ffreestanding
CFLAGS += -nostdinc -nostdlib -fno-common

ASFLAGS += -nostdinc -nostdlib -D__ASSEMBLY__
ifeq (y, $(CONFIG_MULTIBOOT2))
ASFLAGS += -DCONFIG_MULTIBOOT2
endif

CFLAGS += -DCONFIG_AARCH64
ifeq (y, $(CONFIG_RELOC))
ASFLAGS += -DCONFIG_RELOC
endif

LDFLAGS += -nostartfiles -nostdlib
LDFLAGS += -Wl,-n,-z,max-page-size=0x1000
LDFLAGS += -Wl,--no-dynamic-linker

ifeq (y, $(CONFIG_RELOC))
# on X86_64, when build with "-pie", GCC fails on linking R_X86_64_32
# relocations with "recompile with fPIC" error, because it may cause
# run-time relocation overflow if it runs at address above 4GB.
# We know it's safe because Hypervisor runs under 4GB. "noreloc-overflow"
# is used to avoid the compile error
LDFLAGS += -pie -z noreloc-overflow
else
LDFLAGS += -static
endif

ifeq (y, $(CONFIG_RELEASE))
LDFLAGS += -s
endif

ARCH_ASFLAGS += -D__ASSEMBLY__
ARCH_ASFLAGS += -DCONFIG_AARCH64
ARCH_CFLAGS += -gdwarf-2
ARCH_ASFLAGS += -gdwarf-2 -DASSEMBLER=1
ARCH_ARFLAGS +=
ARCH_LDFLAGS +=

ARCH_LDSCRIPT = $(HV_OBJDIR)/link_ram.ld
ARCH_LDSCRIPT_IN = arch/aarch64/link_ram.lds.S

REL_INCLUDE_PATH += include
REL_INCLUDE_PATH += include/lib
REL_INCLUDE_PATH += include/lib/crypto
REL_INCLUDE_PATH += include/common
REL_INCLUDE_PATH += include/debug
REL_INCLUDE_PATH += include/public
REL_INCLUDE_PATH += include/dm
REL_INCLUDE_PATH += include/hw
REL_INCLUDE_PATH += boot/include
#REL_INCLUDE_PATH += boot/include/guest

# TODO: ARCH name should come from generated config.mk.
# Fixed to x86 for now.
ARCH := aarch64
REL_INCLUDE_PATH += include/arch/$(ARCH)

INCLUDE_PATH := $(realpath $(REL_INCLUDE_PATH))
INCLUDE_PATH += $(HV_OBJDIR)/include
INCLUDE_PATH += $(BOARD_INFO_DIR)
INCLUDE_PATH += $(BOARD_CFG_DIR)
INCLUDE_PATH += $(SCENARIO_CFG_DIR)

CC := ${CROSS_COMPILE}gcc
AS := ${CROSS_COMPILE}gcc
AR := ${CROSS_COMPILE}ar
LD := ${CROSS_COMPILE}ld
OBJCOPY := ${CROSS_COMPILE}objcopy
OBJDUMP := ${CROSS_COMPILE}objdump

export CC AS AR LD OBJCOPY
export CFLAGS ASFLAGS ARFLAGS LDFLAGS ARCH_CFLAGS ARCH_ASFLAGS ARCH_ARFLAGS ARCH_LDFLAGS
export HV_OBJDIR HV_MODDIR CONFIG_RELEASE INCLUDE_PATH
export LIB_DEBUG LIB_RELEASE

# library componment
LIB_C_SRCS += lib/string.c
LIB_C_SRCS += lib/sprintf.c
LIB_C_SRCS += arch/aarch64/lib/memory.c

# platform boot component
BOOT_S_SRCS += arch/aarch64/boot/head.S
BOOT_S_SRCS += arch/aarch64/boot/entry.S
BOOT_C_SRCS += arch/aarch64/traps.c
BOOT_C_SRCS += arch/aarch64/psci.c
BOOT_S_SRCS += arch/aarch64/smc.S
BOOT_C_SRCS += boot/boot.c
BOOT_C_SRCS += boot/multiboot/multiboot.c
ifeq ($(CONFIG_MULTIBOOT2),y)
BOOT_C_SRCS += boot/multiboot/multiboot2.c
endif
#BOOT_C_SRCS += boot/reloc.c

# hardware management component
HW_C_SRCS += arch/aarch64/time.c
HW_C_SRCS += arch/aarch64/irq.c
HW_C_SRCS += arch/aarch64/gic.c
HW_C_SRCS += arch/aarch64/pagetable.c
HW_C_SRCS += arch/aarch64/page.c
HW_C_SRCS += arch/aarch64/mmu.c
HW_C_SRCS += arch/aarch64/cpu.c
HW_C_SRCS += arch/aarch64/cpu_caps.c
HW_C_SRCS += arch/aarch64/tsc.c
HW_C_SRCS += arch/aarch64/pm.c
HW_C_SRCS += arch/aarch64/e820.c
HW_S_SRCS += arch/aarch64/sched.S

GIC_VERSION ?= 3

ifeq ($(GIC_VERSION),2)
HW_C_SRCS += arch/aarch64/gic/v2/gicv2_main.c
HW_C_SRCS += arch/aarch64/gic/v2/gicv2_helpers.c
HW_C_SRCS += arch/aarch64/gic/v2/gicdv2_helpers.c
else
HW_C_SRCS += arch/aarch64/gic/v3/gicv3_main.c
HW_C_SRCS += arch/aarch64/gic/v3/gicv3_helpers.c
HW_C_SRCS += arch/aarch64/gic/v3/gicdv3_helpers.c
HW_C_SRCS += arch/aarch64/gic/v3/gicrv3_helpers.c
HW_C_SRCS += arch/aarch64/gic/v3/arm_gicv3_common.c
endif
HW_C_SRCS += common/ticks.c
HW_C_SRCS += common/delay.c
HW_C_SRCS += common/timer.c
HW_C_SRCS += common/irq.c
HW_C_SRCS += common/softirq.c
HW_C_SRCS += common/schedule.c
HW_C_SRCS += common/event.c
HW_C_SRCS += common/efi_mmap.c
HW_C_SRCS += common/sbuf.c
ifeq ($(CONFIG_SCHED_NOOP),y)
HW_C_SRCS += common/sched_noop.c
endif
ifeq ($(CONFIG_SCHED_IORR),y)
HW_C_SRCS += common/sched_iorr.c
endif
ifeq ($(CONFIG_SCHED_BVT),y)
HW_C_SRCS += common/sched_bvt.c
endif
ifeq ($(CONFIG_SCHED_PRIO),y)
HW_C_SRCS += common/sched_prio.c
endif
HW_C_SRCS += arch/aarch64/configs/vm_config.c

ifneq ($(CONFIG_RELEASE),y)
BOOT_C_SRCS += debug/hypercall.c
BOOT_C_SRCS += arch/aarch64/dump.c
BOOT_C_SRCS += release/dump.c
BOOT_C_SRCS += debug/logmsg.c
BOOT_C_SRCS += debug/printf.c
BOOT_C_SRCS += debug/console.c
BOOT_C_SRCS += debug/shell.c
BOOT_C_SRCS += debug/string.c
BOOT_C_SRCS += debug/trace.c
BOOT_C_SRCS += release/npk_log.c
BOOT_C_SRCS += debug/dbg_cmd.c
BOOT_C_SRCS += release/profiling.c
BOOT_C_SRCS += debug/sbuf.c
endif

# VM Configuration
VM_CFG_C_SRCS += $(BOARD_INFO_DIR)/board.c
VM_CFG_C_SRCS += $(SCENARIO_CFG_DIR)/vm_configurations.c
VM_CFG_C_SRCS += $(BOARD_CFG_DIR)/pt_intx.c

# virtual platform base component
VP_BASE_C_SRCS += arch/aarch64/guest/instr_emul.c
VP_BASE_C_SRCS += arch/aarch64/guest/vmx_io.c
VP_BASE_C_SRCS += arch/aarch64/guest/vcpu.c
VP_BASE_C_SRCS += arch/aarch64/guest/vm.c
VP_BASE_C_SRCS += arch/aarch64/guest/vm_reset.c
VP_BASE_C_SRCS += arch/aarch64/guest/vmcall.c
VP_BASE_C_SRCS += arch/aarch64/guest/vsmc.c
VP_BASE_C_SRCS += arch/aarch64/guest/vmexit.c
VP_BASE_C_SRCS += arch/aarch64/guest/ve820.c
VP_BASE_C_SRCS += arch/aarch64/guest/guest_memory.c
VP_BASE_C_SRCS += arch/aarch64/guest/ept.c
VP_BASE_C_SRCS += arch/aarch64/guest/vgic.c
#VP_BASE_C_SRCS += arch/aarch64/guest/vgicv3.c
VP_BASE_C_SRCS += arch/aarch64/guest/virq.c
VP_BASE_C_SRCS += arch/aarch64/uart_pl011.c
VP_BASE_C_SRCS += arch/aarch64/guest/vuart.c

ifeq ($(CONFIG_HYPERV_ENABLED),y)
endif
ifeq ($(CONFIG_NVMX_ENABLED),y)
endif
#VP_BASE_C_SRCS += boot/guest/vboot_info.c
ifeq ($(CONFIG_GUEST_KERNEL_BZIMAGE),y)
#VP_BASE_C_SRCS += boot/guest/bzimage_loader.c
endif
VP_BASE_C_SRCS += common/hv_main.c
#VP_BASE_C_SRCS += common/vm_load.c
ifeq ($(CONFIG_SECURITY_VM_FIXUP),y)
VP_BASE_C_SRCS += quirks/security_vm_fixup.c
endif

# virtual platform device model
VP_DM_C_SRCS += dm/io_req.c
#VP_DM_C_SRCS += dm/mmio_dev.c
#VP_DM_C_SRCS += dm/vgpio.c
#VP_DM_C_SRCS += arch/aarch64/guest/vm_reset.c

# virtual platform hypercall
#VP_HCALL_C_SRCS += arch/aarch64/guest/vmcall.c
#VP_HCALL_C_SRCS += common/hypercall.c

# system initialization
SYS_INIT_C_SRCS += arch/aarch64/init.c

LIB_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(LIB_C_SRCS))
LIB_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(LIB_S_SRCS))
BOOT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(BOOT_C_SRCS))
BOOT_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(BOOT_S_SRCS))
HW_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(HW_C_SRCS))
HW_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(HW_S_SRCS))
VM_CFG_C_OBJS := $(patsubst %.c,%.o,$(VM_CFG_C_SRCS))
VP_BASE_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_BASE_C_SRCS))
VP_BASE_S_OBJS := $(patsubst %.S,$(HV_OBJDIR)/%.o,$(VP_BASE_S_SRCS))
VP_DM_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_DM_C_SRCS))
VP_TRUSTY_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_TRUSTY_C_SRCS))
VP_X86_TEE_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_X86_TEE_C_SRCS))
VP_HCALL_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(VP_HCALL_C_SRCS))
SYS_INIT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(SYS_INIT_C_SRCS))

ifneq ($(CONFIG_RELEASE),y)
CFLAGS += -DHV_DEBUG -DPROFILING_ON -fno-omit-frame-pointer
endif

MODULES += $(LIB_MOD)
MODULES += $(BOOT_MOD)
MODULES += $(HW_MOD)
MODULES += $(VP_BASE_MOD)
MODULES += $(VP_DM_MOD)
MODULES += $(VP_HCALL_MOD)
ifeq ($(CONFIG_RELEASE),y)
MODULES += $(LIB_RELEASE)
LIB_BUILD = $(LIB_RELEASE)
LIB_MK = release/Makefile
endif
MODULES += $(SYS_INIT_MOD)

DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')
VERSION := $(HV_OBJDIR)/include/version.h
HEADERS := $(VERSION) $(HV_CONFIG_H) $(HV_CONFIG_TIMESTAMP)

PRE_BUILD_DIR := ../misc/hv_prebuild
PRE_BUILD_CHECKER := $(HV_OBJDIR)/hv_prebuild_check.out
SERIAL_CONF = $(HV_OBJDIR)/serial.conf

.PHONY: all
all: env_check $(HV_ACPI_TABLE_TIMESTAMP) $(SERIAL_CONF) lib-mod $(HV_OBJDIR)/$(HV_FILE).bin

install: $(HV_OBJDIR)/$(HV_FILE).bin
	install -D $(HV_OBJDIR)/$(HV_FILE).bin $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).bin
	@if [ -e "$(HV_OBJDIR)/serial.conf" ];then \
		install -D -b -m 0644 $(HV_OBJDIR)/serial.conf -t $(DESTDIR)$(sysconfdir)/; \
	fi

install-debug: $(HV_OBJDIR)/$(HV_FILE).map $(HV_OBJDIR)/$(HV_FILE).out
	install -D $(HV_OBJDIR)/$(HV_FILE).out $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).out
	install -D $(HV_OBJDIR)/$(HV_FILE).map $(DESTDIR)$(libdir)/acrn/$(HV_FILE).$(BOARD).$(SCENARIO).map

.PHONY: env_check
env_check:

$(PRE_BUILD_CHECKER): $(HV_CONFIG_H) $(HV_CONFIG_TIMESTAMP)
	@echo "Ignore pre-build static check ..."
#	$(MAKE) -C $(PRE_BUILD_DIR) BOARD=$(BOARD) SCENARIO=$(SCENARIO) CHECKER_OUT=$(PRE_BUILD_CHECKER)
#	@$(PRE_BUILD_CHECKER)  <- TODO: aarch64 cann't run on server

$(SERIAL_CONF): $(HV_CONFIG_TIMESTAMP) $(HV_ALLOCATION_XML)
	@echo "generate the serial configuration file for service VM ..."
	python3 ../misc/config_tools/service_vm_config/serial_config.py --allocation $(HV_ALLOCATION_XML) --scenario $(HV_SCENARIO_XML) --out $(SERIAL_CONF)

.PHONY: lib-mod boot-mod hw-mod vp-base-mod vp-dm-mod vp-trusty-mod vp-x86tee-mod vp-hcall-mod sys-init-mod
$(LIB_MOD): $(LIB_C_OBJS) $(LIB_S_OBJS)
	$(AR) $(ARFLAGS) $(LIB_MOD) $(LIB_C_OBJS) $(LIB_S_OBJS)

lib-mod: $(LIB_MOD)

$(BOOT_MOD): $(BOOT_S_OBJS) $(BOOT_C_OBJS)
	$(AR) $(ARFLAGS) $(BOOT_MOD) $(BOOT_S_OBJS) $(BOOT_C_OBJS)

boot-mod: $(BOOT_MOD)

$(HW_MOD): $(HW_S_OBJS) $(HW_C_OBJS) $(VM_CFG_C_OBJS)
	$(AR) $(ARFLAGS) $(HW_MOD) $(HW_S_OBJS) $(HW_C_OBJS) $(VM_CFG_C_OBJS)

hw-mod: $(HW_MOD)

$(VP_BASE_MOD): $(VP_BASE_S_OBJS) $(VP_BASE_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_BASE_MOD) $(VP_BASE_S_OBJS) $(VP_BASE_C_OBJS)

vp-base-mod: $(VP_BASE_MOD)

$(VP_DM_MOD): $(VP_DM_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_DM_MOD) $(VP_DM_C_OBJS)

vp-dm-mod: $(VP_DM_MOD)

$(VP_TRUSTY_MOD): $(VP_TRUSTY_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_TRUSTY_MOD) $(VP_TRUSTY_C_OBJS)

vp-trusty-mod: $(VP_TRUSTY_MOD)

$(VP_X86_TEE_MOD): $(VP_X86_TEE_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_X86_TEE_MOD) $(VP_X86_TEE_C_OBJS)

$(VP_HCALL_MOD): $(VP_HCALL_C_OBJS)
	$(AR) $(ARFLAGS) $(VP_HCALL_MOD) $(VP_HCALL_C_OBJS)

vp-hcall-mod: $(VP_HCALL_MOD)

$(SYS_INIT_MOD): $(SYS_INIT_C_OBJS)
	$(AR) $(ARFLAGS) $(SYS_INIT_MOD) $(SYS_INIT_C_OBJS)

sys-init-mod: $(SYS_INIT_MOD)

.PHONY: lib

$(LIB_BUILD): $(HEADERS)
	$(MAKE) -f $(LIB_MK) MKFL_NAME=$(LIB_MK)

lib: $(LIB_BUILD)

$(HV_OBJDIR)/$(HV_FILE).bin: $(HV_OBJDIR)/$(HV_FILE).out
	$(OBJCOPY) -O binary $< $(HV_OBJDIR)/$(HV_FILE).bin
	rm -f $(UPDATE_RESULT)

$(HV_OBJDIR)/$(HV_FILE).out: $(MODULES) $(ARCH_LDSCRIPT)
	$(CC) -Wl,-Map=$(HV_OBJDIR)/$(HV_FILE).map -o $@ $(LDFLAGS) $(ARCH_LDFLAGS) -T$(ARCH_LDSCRIPT) \
		-Wl,--start-group $(MODULES) -Wl,--end-group

$(ARCH_LDSCRIPT): $(ARCH_LDSCRIPT_IN)
	#cp $< $@
	$(CC) -E -P $(patsubst %, -I%, $(INCLUDE_PATH))  $(ARCH_ASFLAGS) -MMD -MT $@ -o $@ $<


.PHONY: clean
clean:
	rm -f $(VERSION)
	rm -rf $(HV_OBJDIR)

.PHONY: distclean
distclean:
	rm -f $(DISTCLEAN_OBJS)
	rm -f $(VERSION)
	rm -rf $(HV_OBJDIR)
	rm -f tags TAGS cscope.files cscope.in.out cscope.out cscope.po.out GTAGS GPATH GRTAGS GSYMS

PHONY: (VERSION)
$(VERSION): $(HV_CONFIG_H)
	touch $(VERSION)
	@if [ "$(BUILD_VERSION)"x = x ];then \
		COMMIT=`git rev-parse --verify --short HEAD 2>/dev/null`;\
		DIRTY=`git diff-index --name-only HEAD`;\
		if [ -n "$$DIRTY" ];then PATCH="$$COMMIT-dirty";else PATCH="$$COMMIT";fi;\
	else \
		PATCH="$(BUILD_VERSION)"; \
	fi; \
	COMMIT_TAGS=$$(git tag --points-at HEAD|tr -s "\n" " "); \
	COMMIT_TAGS=$$(eval echo $$COMMIT_TAGS);\
	COMMIT_TIME=$$(git log -1 --date=format:"%Y-%m-%d-%T" --format=%cd); \
	TIME=$$(date -u -d "@$${SOURCE_DATE_EPOCH:-$$(date +%s)}" "+%F %T"); \
	USER="$${USER:-$$(id -u -n)}"; \
	if [ x$(CONFIG_RELEASE) = "xy" ];then BUILD_TYPE="REL";else BUILD_TYPE="DBG";fi;\
	echo "/*" > $(VERSION); \
	sed 's/^/ * /' ../LICENSE >> $(VERSION); \
	echo " */" >> $(VERSION); \
	echo "" >> $(VERSION); \
	echo "#ifndef VERSION_H" >> $(VERSION); \
	echo "#define VERSION_H" >> $(VERSION); \
	echo "#define HV_API_MAJOR_VERSION $(API_MAJOR_VERSION)U" >> $(VERSION);\
	echo "#define HV_API_MINOR_VERSION $(API_MINOR_VERSION)U" >> $(VERSION);\
	echo "#define HV_BRANCH_VERSION "\"$(BRANCH_VERSION)\""" >> $(VERSION);\
	echo "#define HV_COMMIT_DIRTY "\""$$PATCH"\""" >> $(VERSION);\
	echo "#define HV_COMMIT_TAGS "\"$$COMMIT_TAGS\""" >> $(VERSION);\
	echo "#define HV_COMMIT_TIME "\"$$COMMIT_TIME\""" >> $(VERSION);\
	echo "#define HV_BUILD_TYPE "\""$$BUILD_TYPE"\""" >> $(VERSION);\
	echo "#define HV_BUILD_TIME "\""$$TIME"\""" >> $(VERSION);\
	echo "#define HV_BUILD_USER "\""$$USER"\""" >> $(VERSION);\
	echo "#define HV_BUILD_SCENARIO "\"$(SCENARIO)\""" >> $(VERSION);\
	echo "#define HV_BUILD_BOARD "\"$(BOARD)\""" >> $(VERSION);\
	echo "#endif" >> $(VERSION)

-include $(C_OBJS:.o=.d)
-include $(S_OBJS:.o=.d)

$(HV_OBJDIR)/%.o: %.c $(HEADERS) $(PRE_BUILD_CHECKER)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@

$(VM_CFG_C_SRCS): %.c: $(HV_CONFIG_TIMESTAMP)

$(VM_CFG_C_OBJS): %.o: %.c $(HEADERS) $(PRE_BUILD_CHECKER)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. -c $(CFLAGS) $(ARCH_CFLAGS) $< -o $@ -MMD -MT $@

$(HV_OBJDIR)/%.o: %.S $(HEADERS) $(PRE_BUILD_CHECKER)
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) -I. $(ASFLAGS) $(ARCH_ASFLAGS) -c $< -o $@ -MMD -MT $@

.DEFAULT_GOAL := all
