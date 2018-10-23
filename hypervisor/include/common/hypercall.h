/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file hypercall.h
 *
 * @brief public APIs for hypercall
 */

#ifndef HYPERCALL_H
#define HYPERCALL_H

struct vhm_request;

bool is_hypercall_from_ring0(void);

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief offline vcpu from SOS
 *
 * The function offline specific vcpu from SOS.
 *
 * @param vm Pointer to VM data structure
 * @param lapicid lapic id of the vcpu which wants to offline
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_sos_offline_cpu(struct vm *vm, uint64_t lapicid);

/**
 * @brief Get hypervisor api version
 *
 * The function only return api version information when VM is VM0.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical memory address. The api version returned
 *              will be copied to this gpa
 *
 * @pre Pointer vm shall point to VM0 
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_api_version(struct vm *vm, uint64_t param);

/**
 * @brief create virtual machine
 *
 * Create a virtual machine based on parameter, currently there is no
 * limitation for calling times of this function, will add MAX_VM_NUM
 * support later.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical memory address. This gpa points to
 *              struct acrn_create_vm
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vm(struct vm *vm, uint64_t param);

/**
 * @brief destroy virtual machine
 *
 * Destroy a virtual machine, it will pause target VM then shutdown it.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_destroy_vm(uint16_t vmid);

/**
 * @brief reset virtual machine
 *
 * Reset a virtual machine, it will make target VM rerun from
 * pre-defined entry. Comparing to start vm, this function reset
 * each vcpu state and do some initialization for guest.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_vm(uint16_t vmid);

/**
 * @brief start virtual machine
 *
 * Start a virtual machine, it will schedule target VM's vcpu to run.
 * The function will return -1 if the target VM does not exist or the
 * IOReq buffer page for the VM is not ready.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_start_vm(uint16_t vmid);

/**
 * @brief pause virtual machine
 *
 * Pause a virtual machine, if the VM is already paused, the function
 * will return 0 directly for success.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_pause_vm(uint16_t vmid);

/**
 * @brief create vcpu
 *
 * Create a vcpu based on parameter for a VM, it will allocate vcpu from
 * freed physical cpus, if there is no available pcpu, the function will
 * return -1.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_create_vcpu
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vcpu(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief set vcpu regs
 *
 * Set the vcpu regs. It will set the vcpu init regs from DM. Now,
 * it's only applied to BSP. AP always uses fixed init regs.
 * The function will return -1 if the targat VM or BSP doesn't exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_vcpu_regs
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vcpu_regs(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief assert IRQ line
 *
 * Assert a virtual IRQ line for a VM, which could be from ISA or IOAPIC,
 * normally it will active a level IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_irqline
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assert_irqline(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief deassert IRQ line
 *
 * Deassert a virtual IRQ line for a VM, which could be from ISA or IOAPIC,
 * normally it will deactive a level IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_irqline
 *
 * @pre @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassert_irqline(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief trigger a pulse on IRQ line
 *
 * Trigger a pulse on a virtual IRQ line for a VM, which could be from ISA
 * or IOAPIC, normally it triggers an edge IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_irqline
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_pulse_irqline(struct vm *vm, uint16_t vmid, uint64_t param);


/**
 * @brief set or clear IRQ line
 *
 * Set or clear a virtual IRQ line for a VM, which could be from ISA
 * or IOAPIC, normally it triggers an edge IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param ops request command for IRQ set or clear
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_irqline(const struct vm *vm, uint16_t vmid,
				const struct acrn_irqline_ops *ops);
/**
 * @brief inject MSI interrupt
 *
 * Inject a MSI interrupt for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_msi_entry
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_inject_msi(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief set ioreq shared buffer
 *
 * Set the ioreq share buffer for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_set_ioreq_buffer
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ioreq_buffer(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief notify request done
 *
 * Notify the requestor VCPU for the completion of an ioreq.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 * @param vcpu_id vcpu ID of the requestor
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_notify_ioreq_finish(uint16_t vmid, uint16_t vcpu_id);

/**
 * @brief setup ept memory mapping
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct vm_set_memmap
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vm_memory_region(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief setup ept memory mapping for multi regions
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical address. This gpa points to
 *              struct set_memmaps
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vm_memory_regions(struct vm *vm, uint64_t param);

/**
 * @brief change guest memory page write permission
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param wp_gpa guest physical address. This gpa points to
 *              struct wp_data
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_write_protect_page(struct vm *vm, uint16_t vmid, uint64_t wp_gpa);

/**
 * @brief remap PCI MSI interrupt
 *
 * Remap a PCI MSI interrupt from a VM's virtual vector to native vector.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_vm_pci_msix_remap
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_remap_pci_msix(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief translate guest physical address to host physical address
 *
 * Translate guest physical address to host physical address for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct vm_gpa2hpa
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_gpa_to_hpa(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief Assign one passthrough dev to VM.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              physical BDF of the assigning ptdev
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_ptdev(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief Deassign one passthrough dev from VM.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              physical BDF of the deassigning ptdev
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_ptdev(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief Set interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ptdev_intr_info(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @brief Clear interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_ptdev_intr_info(struct vm *vm, uint16_t vmid,
	uint64_t param);

/**
 * @brief Setup a share buffer for a VM.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical address. This gpa points to
 *              struct sbuf_setup_param
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_setup_sbuf(struct vm *vm, uint64_t param);

/**
  * @brief Setup the hypervisor NPK log.
  *
  * @param vm Pointer to VM data structure
  * @param param guest physical address. This gpa points to
  *              struct hv_npk_log_param
  *
  * @pre Pointer vm shall point to VM0
  * @return 0 on success, non-zero on error.
  */
int32_t hcall_setup_hv_npk_log(struct vm *vm, uint64_t param);

/**
 * @brief Get VCPU Power state.
 *
 * @param vm pointer to VM data structure
 * @param cmd cmd to show get which VCPU power state data
 * @param param VCPU power state data
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */

int32_t hcall_get_cpu_pm_state(struct vm *vm, uint64_t cmd, uint64_t param);

/**
 * @brief Get VCPU a VM's interrupt count data.
 *
 * @param vm pointer to VM data structure
 * @param vmid id of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              acrn_intr_monitor
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_vm_intr_monitor(struct vm *vm, uint16_t vmid, uint64_t param);

/**
 * @defgroup trusty_hypercall Trusty Hypercalls
 *
 * This is a special group that includes all hypercalls
 * related to Trusty
 *
 * @{
 */

/**
 * @brief Switch vCPU state between Normal/Secure World.
 *
 * * The hypervisor uses this hypercall to do the world switch
 * * The hypervisor needs to:
 *   * save current world vCPU contexts, and load the next world
 *     vCPU contexts
 *   * update ``rdi``, ``rsi``, ``rdx``, ``rbx`` to next world
 *     vCPU contexts
 *
 * @param vcpu Pointer to VCPU data structure
 *
 * @return 0 on success, non-zero on error.
 */

int32_t hcall_world_switch(struct vcpu *vcpu);

/**
 * @brief Initialize environment for Trusty-OS on a vCPU.
 *
 * * It is used by the User OS bootloader (``UOS_Loader``) to request ACRN
 *   to initialize Trusty
 * * The Trusty memory region range, entry point must be specified
 * * The hypervisor needs to save current vCPU contexts (Normal World)
 *
 * @param vcpu Pointer to vCPU data structure
 * @param param guest physical address. This gpa points to
 *              trusty_boot_param structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_initialize_trusty(struct vcpu *vcpu, uint64_t param);

/**
 * @brief Save/Restore Context of Secure World.
 *
 * @param vcpu Pointer to VCPU data structure
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_save_restore_sworld_ctx(struct vcpu *vcpu);

/**
 * @}
 */
/* End of trusty_hypercall */

/**
 * @brief set upcall notifier vector
 *
 * This is the API that helps to switch the notifer vecotr. If this API is
 * not called, the hypervisor will use the default notifier vector(0xF7)
 * to notify the SOS kernel.
 *
 * @param vm Pointer to VM data structure
 * @param param the expected notifier vector from guest
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_callback_vector(const struct vm *vm, uint64_t param);

/**
 * @}
 */
/* End of acrn_hypercall */

#endif /* HYPERCALL_H*/
