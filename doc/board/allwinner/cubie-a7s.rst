.. SPDX-License-Identifier: GPL-2.0+

Radxa Cubie A7S
================

This port builds U-Boot proper as the AArch32 BL33 payload for the Allwinner
A733 (sun60iw2). It deliberately does not duplicate the low-level firmware
already maintained in the adjacent ``boot0-A7S`` and TF-A trees.

Boot chain
----------

The expected chain is::

  BROM -> boot0 -> FIP -> BL31 + AR100S/SCP -> U-Boot BL33

The stage ownership is:

* ``boot0-A7S``: PMIC, LPDDR5 training, FIP loading and the initial pin setup;
* ``boot0-A7S/ar100s``: SCP services;
* ``arm-trusted-firmware``: EL3, GIC-600 and PSCI;
* this tree: U-Boot command, storage and OS handoff services.

U-Boot is linked and entered at ``0x4a000000``. The initial stack is at
``0x49f00000``. U-Boot proper manages the low 2 GiB of DRAM so that its
AArch32 address arithmetic cannot wrap at 4 GiB. The devicetree can still
describe all 6 GiB to an AArch64 operating system.

Build and package
-----------------

Build out of tree with an AArch32 toolchain::

  $ make O=/tmp/u-boot-a733 CROSS_COMPILE=arm-none-eabi- cubie_a7s_defconfig
  $ make O=/tmp/u-boot-a733 CROSS_COMPILE=arm-none-eabi- -j$(nproc)

Use the raw ``u-boot.bin`` as BL33. The binman output exists to keep the
normal U-Boot build targets intact, but is not the image consumed by the A733
FIP loader. Package it with the existing boot-chain script::

  $ ../boot0-A7S/build_boot.sh \
        --uboot /tmp/u-boot-a733/u-boot.bin

When reusing already verified boot0, SCP and BL31 products, add
``--skip-boot0 --skip-scp --skip-bl31``.

Support status
--------------

The smallest bootable service set is intentionally separated from optional
recovery and media features.

========================  ===============  ===================================
Function                  Status           Notes
========================  ===============  ===================================
BL33 entry and relocation Built            Entry is ``0x4a000000``
ARM generic timer         Built            24 MHz counter inherited from TF-A
UART0 console             Built            PB9/PB10 settings match boot0
SD card on SMHC0          Built            A733 clock/reset layout is supported
FAT/ext4/GPT              Built            Standard U-Boot commands
bootstd/extlinux/EFI/FIT  Built            Standard U-Boot boot framework
System reset              Built            PSCI service provided by BL31
DRAM/PMIC                 External         Owned by boot0
GIC-600/EL3               External         Owned by BL31
AR100S services           External         Owned by ``boot0-A7S/ar100s``
eMMC/UFS                  Not enabled      Board wiring and tuning are pending
USB/network recovery      Not enabled      A733 PHY/MAC integration is pending
HDMI 2.0                  Not ported       Present in the vendor DRM stack
MIPI DSI/LVDS/panels      Not ported       Present in the vendor DRM stack
MIPI CSI/camera/ISP       Not implemented  No A733 camera driver in vendor U-Boot
GPU                       Not implemented  Runtime driver belongs in the OS
NPU                       Handoff only     Vendor code only has clock/SRAM hooks
========================  ===============  ===================================

The vendor ``u-boot-aw2501`` display implementation is based on U-Boot
2018.07 and contains about 115,000 lines under its private DRM subtree. It
depends on vendor clock, sys_config, power-management and old driver-model
interfaces. Copying it into this tree would create a second U-Boot framework
and make upstream updates difficult. A real display port should therefore be
implemented as independent A733 clock/reset, display-engine, TCON and output
bridge drivers using current U-Boot video APIs. Until those drivers and their
board pin/power descriptions are complete and hardware-tested, no display
option is enabled in ``cubie_a7s_defconfig``.

Validation boundary
-------------------

A successful host build and FIP inspection validate image format, size and
stage addresses. They do not validate UART output, SD timing, display signal,
Linux boot, DVFS or suspend on the physical board. Keep those claims tied to
the exact built and flashed images and to captured board logs.
