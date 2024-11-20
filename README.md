# ACRN - AARCH64

## Overview
The aim of the project is to enable ACRN for the AARCH64 architecture. This project is currently under development and is not yet ready for production. Once this support is implemented and has sufficient quality, it is intended for this port to become a part of the upstream ACRN project and to continue further development there.

## Current State
The current development is based on QEMU AARCH64 virtual platform. The ACRN hypervisor has bootup successfully to ACRN console with the support of SMP, MMU, GIC. 
The basic virtualization functions are under development. Now it supports VM lifecycle management, vCPU context switch, memory virtualization via stage-2 page-table translation partially.
Other functions such as IO virtualization, IRQ virtualization are to be developed.

Contact dh.zhou@samsung.com for any further questions.

## How to compile
make BOARD=qemu-aarch64 SCENARIO=partitioned ARCH=aarch64

## How to run
Prepare aarch64 qemu version and uboot binary. Run command below:
qemu-system-aarch64 -machine virt,gic-version=3,virtualization=true -cpu cortex-a57 -nographic -smp 8 -m 16348M -bios u-boot.bin -device loader,file=acrn.bin,force-raw=on,addr=0x80080000

After enter into uboot console, input command:
booti 0x80080000 - 0x40000000

## License
This project is released under the terms of BSD-3-Clause license.
