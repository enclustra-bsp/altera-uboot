/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2023 Enclustra GmbH
 * <info@enclustra.com>
 */

#ifndef __CONFIG_SOCFGPA_MERCURY_AA1_H__
#define __CONFIG_SOCFGPA_MERCURY_AA1_H__

#include <asm/arch/base_addr_a10.h>

/*
 * U-Boot general configurations
 */

/* Memory configurations  */
#define PHYS_SDRAM_1_SIZE		0x80000000

/*
 * Serial / UART configurations
 */
#define CONFIG_SYS_NS16550_MEM32
#define CONFIG_SYS_BAUDRATE_TABLE {4800, 9600, 19200, 38400, 57600, 115200}
#define CONFIG_SYS_NAND_U_BOOT_OFFS	0x100000

/* U-Boot environment */
#define SOCFPGA_BOOT_SETTINGS \
    "qspi_offset_addr_spl=0x0\0" \
    "qspi_offset_addr_u-boot=0x100000\0" \
    "qspi_offset_addr_u-boot-env=0x180000\0" \
    "qspi_offset_addr_boot-script=0x200000\0" \
    "qspi_offset_addr_devicetree=0x280000\0" \
    "qspi_offset_addr_bitstream=0x300000\0" \
    "qspi_offset_addr_kernel=0x1000000\0" \
    "qspi_offset_addr_rootfs=0x2000000\0" \
    "size_spl=0x100000\0" \
    "size_u-boot=0x80000\0" \
    "size_u-boot-env=0x80000\0" \
    "size_boot-script=0x80000\0" \
    "size_devicetree=0x80000\0" \
    "size_bitstream=0xD00000\0" \
    "size_kernel=0x1000000\0" \
    "size_rootfs=0x2000000\0" \
    "ram_addr_rootfs=0x12000000\0" \
    "ram_addr_kernel=0x11000000\0" \
    "ram_addr_devicetree=0x10000000\0" \
    "ram_addr_boot-script=0x10800000\0" \
    "bootm_size=0x0a000000\0" \
    "bootargs-qspi=earlycon console=ttyS0,115200 rw root=/dev/ram0\0" \
    "bootargs=earlycon console=ttyS0,115200 rw root=/dev/mmcblk0p3\0" \
    "qspiload=setenv bootargs ${bootargs-qspi}; sf probe; sf read ${ram_addr_boot-script} ${qspi_offset_addr_boot-script} ${size_boot-script}\0" \
    "qspiboot=source ${ram_addr_boot-script}\0"

/*
 * L4 OSC1 Timer 0
 */
/* reload value when timer count to zero */
#define TIMER_LOAD_VAL			0xFFFFFFFF

/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

/*
 * L4 Watchdog
 */
#ifdef CONFIG_HW_WATCHDOG
#undef CONFIG_DW_WDT_BASE
#define CONFIG_DW_WDT_BASE		SOCFPGA_L4WD1_ADDRESS
#endif

#endif	/* __CONFIG_SOCFGPA_MERCURY_AA1_H__ */
