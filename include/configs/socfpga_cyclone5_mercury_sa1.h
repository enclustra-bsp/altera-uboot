/*
 * Copyright (C) 2014 Marek Vasut <marex@denx.de>
 * Copyright (C) 2015 Antmicro Ltd
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __CONFIG_SOCFPGA_CYCLONE5_H__
#define __CONFIG_SOCFPGA_CYCLONE5_H__

#include <asm/arch/socfpga_base_addrs.h>
#include "../../board/altera/socfpga/pinmux_config.h"
#include "../../board/altera/socfpga/iocsr_config.h"
#include "../../board/altera/socfpga/pll_config.h"

/* U-Boot Commands */
#define CONFIG_SYS_NO_FLASH
#include <config_cmd_default.h>
#define CONFIG_DOS_PARTITION
#define CONFIG_FAT_WRITE
#define CONFIG_HW_WATCHDOG

#define CONFIG_CMD_ASKENV
#define CONFIG_CMD_BOOTZ
#define CONFIG_CMD_CACHE
#define CONFIG_CMD_DFU
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_EXT4
#define CONFIG_CMD_EXT4_WRITE
#define CONFIG_CMD_FAT
#define CONFIG_CMD_FPGA
#define CONFIG_CMD_FS_GENERIC
#define CONFIG_CMD_GREPENV
#define CONFIG_CMD_MII
#define CONFIG_CMD_MMC
#define CONFIG_CMD_PING
#define CONFIG_CMD_USB
#define CONFIG_CMD_USB_MASS_STORAGE


/* Memory configurations */
#define PHYS_SDRAM_1_SIZE		0x40000000	/* 1GiB on SoCDK */

/* Booting Linux */
#define CONFIG_BOOTDELAY	3
/*#define CONFIG_BOOTFILE		"zImage"*/
/*#define CONFIG_BOOTARGS		"console=ttyS0," __stringify(CONFIG_BAUDRATE) */
/*#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_BOOTCOMMAND	"run ramboot"
#else
#define CONFIG_BOOTCOMMAND	"run mmcload; run mmcboot"
#endif*/
#define CONFIG_LOADADDR		0x8000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR

/* Ethernet on SoC (EMAC) */
#if defined(CONFIG_CMD_NET)
#define CONFIG_EMAC_BASE		SOCFPGA_EMAC1_ADDRESS
#define CONFIG_PHY_INTERFACE_MODE	PHY_INTERFACE_MODE_RGMII

/* PHY */
#define CONFIG_PHY_MICREL
#define CONFIG_PHY_MICREL_KSZ9021
#define CONFIG_KSZ9021_CLK_SKEW_ENV	"micrel-ksz9021-clk-skew"
#define CONFIG_KSZ9021_CLK_SKEW_VAL	0xf0f0
#define CONFIG_KSZ9021_DATA_SKEW_ENV	"micrel-ksz9021-data-skew"
#define CONFIG_KSZ9021_DATA_SKEW_VAL	0x0

#define CONFIG_PHY_MICREL_KSZ9031
#define CONFIG_KSZ9031_CLK_SKEW_ENV    "micrel-ksz9031-clk-skew"
#define CONFIG_KSZ9031_CLK_SKEW_VAL    0x3FF
#define CONFIG_KSZ9031_DATA_SKEW_ENV   "micrel-ksz9031-data-skew"
#define CONFIG_KSZ9031_DATA_SKEW_VAL   0x00
#endif

/* USB */
#ifdef CONFIG_CMD_USB
#define CONFIG_USB_DWC2_REG_ADDR	SOCFPGA_USB1_ADDRESS
#endif
#define CONFIG_G_DNL_MANUFACTURER      "Altera"

/* QSPI Flash Memory Map */
#define QSPI_BOOT_OFFSET          0x00000000   // Storage for Bootimage
#define QSPI_BOOT_SIZE            0x00600000   // size 6MB
#define QSPI_LINUX_OFFSET         0x00600000   // Storage for Linux Kernel
#define QSPI_LINUX_SIZE           0x00500000   // size 5MB
#define QSPI_DTB_OFFSET           0x00B00000   // Storage for Linux Devicetree
#define QSPI_DTB_SIZE             0x00080000   // size 512kB
#define QSPI_ENV_OFFSET           0x00B80000   // Storage for Uboot Environment
#define QSPI_ENV_SIZE             0x00080000   // size 512kB
#define QSPI_BOOTSCRIPT_OFFSET    0x00C00000   // Storage for Uboot boot script
#define QSPI_BOOTSCRIPT_SIZE      0x00040000   // size 256kB
#define QSPI_ROOTFS_OFFSET        0x00C40000   // Storage for Linux Root FS
#define QSPI_ROOTFS_SIZE          0x003C0000   // size 3.84MB

/* Extra Environment */
#define CONFIG_HOSTNAME		socfpga_cyclone5
#define CONFIG_BOOTCOMMAND 	"run modeboot"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"loadaddr=0x200000\0"                       \
	"ethaddr=00:07:ED:00:01:23\0"               \
	"ipaddr=192.168.1.113\0"                    \
	"serverip=192.168.1.188\0"                  \
	"netmask=255.255.255.0\0"                   \
						\
	"kernel_image=uimage\0"                     \
	"ramdisk_image=uramdisk\0"                  \
	"devicetree_image=devicetree.dtb\0"         \
	"bootscript_image=uboot.scr\0"              \
						\
	"kernel_loadaddr=0x3000000\0"               \
	"devicetree_loadaddr=0x2A00000\0"           \
	"ramdisk_loadaddr=0x2000000\0"              \
	"bootscript_loadaddr=0x1000000\0"           \
						\
	"ramdisk_size="    __stringify(QSPI_ROOTFS_SIZE) "\0"   \
	"kernel_size="     __stringify(QSPI_LINUX_SIZE)  "\0"   \
	"devicetree_size=" __stringify(QSPI_DTB_SIZE)    "\0"   \
	"bootscript_size=" __stringify(QSPI_BOOTSCRIPT_SIZE)   "\0"   \
								\
	"qspi_kernel_offset="     __stringify(QSPI_LINUX_OFFSET) "\0"\
	"qspi_ramdisk_offset="    __stringify(QSPI_ROOTFS_OFFSET)"\0"\
	"qspi_devicetree_offset=" __stringify(QSPI_DTB_OFFSET)   "\0"\
	"qspi_bootscript_offset=" __stringify(QSPI_BOOTSCRIPT_OFFSET)  "\0"\
	"qspi_bootimage_offset="  __stringify(QSPI_BOOT_OFFSET)        "\0"\
						 \
	"mmcargs=setenv bootargs console=ttyS0,115200 root=/dev/mmcblk0p2 rw rootwait\0"\
	"qspiboot=echo Bootinq on QSPI Flash ...; " \
		"sf probe && "                          \
		"sf read ${bootscript_loadaddr} ${qspi_bootscript_offset} ${bootscript_size} && "\
		"source ${bootscript_loadaddr}\0"       \
                                                \
	"mmcboot=echo Booting on SD Card ...; "      \
		"mmc rescan && "                        \
		"load mmc 0 ${bootscript_loadaddr} ${bootscript_image} && "\
		"source ${bootscript_loadaddr}\0"       \
                                                \
	"usbboot=echo Booting on USB ...; "     \
		"usb start && "			\
		"load usb 0 ${bootscript_loadaddr} ${bootscript_image} && " \
		"source ${bootscript_loadaddr} \0"\
						\
	"modeboot=setexpr.l bootsel *0xFFD08014 \\\\& 0x7;" \
		"if test ${bootsel} -eq 4; || test ${bootsel} -eq 5;"\
		"then;echo Booting from MMC ...;"\
		"run mmcboot;"\
		"fi;"\
		"if test ${bootsel} -eq 6; || test ${bootsel} -eq 7;"\
		"then;echo Booting from QSPI ...;"\
		"run qspiboot;"\
		"fi\0"

#define CONFIG_SYS_FLASH_BASE		0xFFA00000
#define CONFIG_SYS_FLASH_SIZE		(16 * 1024 * 1024)
#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_SPI_FLASH_BAR
#define CONFIG_ENV_SIZE			QSPI_ENV_SIZE
#define CONFIG_ENV_SECT_SIZE		CONFIG_ENV_SIZE
#define CONFIG_ENV_OFFSET		QSPI_ENV_OFFSET
#define CONFIG_CMD_SAVEENV
#define CONFIG_CMD_ENV
/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

#endif	/* __CONFIG_SOCFPGA_CYCLONE5_H__ */
