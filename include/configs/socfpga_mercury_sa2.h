/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Enclustra GmbH
 * <info@enclustra.com>
 */

#ifndef __CONFIG_SOCFGPA_MERCURY_SA2_H__
#define __CONFIG_SOCFGPA_MERCURY_SA2_H__

#include <asm/arch/base_addr_ac5.h>

/* Memory configurations */
#define PHYS_SDRAM_1_SIZE		0x40000000

/* U-Boot environment */
#define SOCFPGA_BOOT_SETTINGS \
    "fdtfile=devicetree.dtb\0" \
    "qspi_offset_addr_spl=0x0\0" \
    "qspi_offset_addr_u-boot=0x100000\0" \
    "qspi_offset_addr_u-boot-env=0x180000\0" \
    "qspi_offset_addr_boot-script=0x200000\0" \
    "qspi_offset_addr_devicetree=0x280000\0" \
    "qspi_offset_addr_dtoverlay=0x2c0000\0" \
    "qspi_offset_addr_bitstream=0x300000\0" \
    "qspi_offset_addr_kernel=0x1000000\0" \
    "qspi_offset_addr_rootfs=0x2000000\0" \
    "size_spl=0x100000\0" \
    "size_u-boot=0x80000\0" \
    "size_u-boot-env=0x80000\0" \
    "size_boot-script=0x80000\0" \
    "size_devicetree=0x40000\0" \
    "size_dtoverlay=0x40000\0" \
    "size_bitstream=0xD00000\0" \
    "size_kernel=0x1000000\0" \
    "size_rootfs=0x2000000\0" \
    "ram_addr_bitstream=0x13000000\0" \
    "ram_addr_rootfs=0x12000000\0" \
    "ram_addr_kernel=0x11000000\0" \
    "ram_addr_devicetree=0x10000000\0" \
    "ram_addr_devicetree_overlay=0x100c0000\0" \
    "ram_addr_boot-script=0x10800000\0" \
    "fdtofile=socfpga_enclustra_mercury_XXX_overlay.dtbo\0" \
    "fdtload=load ${devtype} ${devnum}:${distro_bootpart} ${ram_addr_devicetree} ${bootdir}/${fdtfile}; load ${devtype} ${devnum}:${distro_bootpart} ${ram_addr_devicetree_overlay} ${bootdir}/${fdtofile}; fdt addr $ram_addr_devicetree; fdt resize 8192; fdt apply $ram_addr_devicetree_overlay\0" \
    "fdtload_qspi=sf probe; sf read ${ram_addr_devicetree} ${qspi_offset_addr_devicetree} ${size_devicetree}; sf read ${ram_addr_devicetree_overlay} ${qspi_offset_addr_dtoverlay} ${size_dtoverlay}; fdt addr ${ram_addr_devicetree}; fdt resize 8192; fdt apply ${ram_addr_devicetree_overlay}\0" \
    "bootm_size=0x0a000000\0" \
    "bootargs-qspi=earlycon console=ttyS0,115200 rw root=/dev/ram0\0" \
    "bootargs=earlycon console=ttyS0,115200 rw rootwait root=/dev/mmcblk0p3\0" \
    "qspiload=setenv bootargs ${bootargs-qspi}; sf probe; sf read ${ram_addr_boot-script} ${qspi_offset_addr_boot-script} ${size_boot-script}\0" \
    "qspiboot=source ${ram_addr_boot-script}\0"

/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

#endif	/* __CONFIG_SOCFGPA_MERCURY_SA2_H__ */
