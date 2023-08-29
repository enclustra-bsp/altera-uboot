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
