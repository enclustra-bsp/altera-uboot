/*
 * Copyright (C) 2014 Marek Vasut <marex@denx.de>
 * Copyright (C) 2015 Antmicro Ltd
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __CONFIG_SOCFPGA_CYCLONE5_H__
#define __CONFIG_SOCFPGA_CYCLONE5_H__

#include <asm/arch/base_addr_ac5.h>

#define ENCLUSTRA_MAC               0xF7B020

/* Default MAC address */
#define ENCLUSTRA_ETHADDR_DEFAULT "00:07:ED:00:00:01"

/* Memory configurations */
#define PHYS_SDRAM_1_SIZE		0x40000000	/* 1GiB */

/* Booting Linux */
#define CONFIG_LOADADDR		0x8000
#define CONFIG_SYS_LOAD_ADDR	CONFIG_LOADADDR

/* Ethernet on SoC (EMAC) */
#if defined(CONFIG_CMD_NET)
#define CONFIG_PHY_INTERFACE_MODE	PHY_INTERFACE_MODE_RGMII
#define CONFIG_PHY_MICREL
#define CONFIG_PHY_MICREL_KSZ9021
#define CONFIG_PHY_MICREL_KSZ9031
#endif

/* USB */
#ifdef CONFIG_CMD_USB
#define CONFIG_USB_DWC2_REG_ADDR	SOCFPGA_USB1_ADDRESS
#endif
#define CONFIG_G_DNL_MANUFACTURER      "Altera"

/* QSPI Flash Memory Map */
#define QSPI_PRELOADER_OFFSET		0x00000000  // Storage for Preloader
#define QSPI_PRELOADER_SIZE		0x00040000  // size 256 KiB
#define QSPI_UBOOT_ERASE_ADDR		0x00040000  // We can erase only page aligned regions
#define QSPI_UBOOT_ERASE_SIZE		0x00080000  // We can erase only page aligned regions
#define QSPI_UBOOT_OFFSET		0x00060000  // Storage for U-Boot image
#define QSPI_UBOOT_SIZE			0x00040000  // size 256 KiB
#define QSPI_BITSTREAM_OFFSET		0x00100000  // Storage for FPGA bitstream
#define QSPI_BITSTREAM_SIZE		0x00700000  // size 7MiB
#define QSPI_ENV_OFFSET			0x00800000  // Storage for Uboot Environment
#define QSPI_ENV_SIZE			0x00040000  // size 256 KiB
#define QSPI_DTB_OFFSET			0x00840000  // Storage for Linux Devicetree
#define QSPI_DTB_SIZE			0x00040000  // size 256 KiB
#define QSPI_BOOTSCRIPT_OFFSET		0x00880000  // Storage for Uboot boot script
#define QSPI_BOOTSCRIPT_SIZE		0x00040000  // size 256 KiB
#define QSPI_LINUX_OFFSET		0x008C0000  // Storage for Linux Kernel
#define QSPI_LINUX_SIZE			0x00740000  // size 7,6 MB
#define QSPI_ROOTFS_OFFSET		0x01000000  // Storage for Linux Root FS (JFFS)
#define QSPI_ROOTFS_SIZE		0x03000000  // size 48 MiB
#define QSPI_RAMDISK_SIZE		0x1000000  // size 16MB MiB

/* Extra Environment */
#define CONFIG_HOSTNAME		socfpga_cyclone5
#define CONFIG_BOOTCOMMAND 	"run modeboot"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"loadaddr=0x200000\0"                       \
	"ipaddr=192.168.1.113\0"                    \
	"serverip=192.168.1.188\0"                  \
	"serverpath=/srv/nfs/rootfs\0"               \
	"netmask=255.255.255.0\0"                   \
						\
	"preloader_image=preloader-mkpimage.bin\0"  \
	"bitstream_image=fpga.rbf.img\0"	    \
	"kernel_image=uImage\0"                     \
	"rootfs_image=rootfs.jffs2\0"               \
	"uramdisk_image=uramdisk\0"                 \
	"devicetree_image=devicetree.dtb\0"         \
	"bootscript_image=uboot.scr\0"              \
	"uboot_image=u-boot.img\0"		    \
						\
	"preloader_loadaddr=0x2B00000\0"	\
	"bitstream_loadaddr=0x3600000\0"	\
	"uboot_loadaddr=0x2C00000\0"		\
	"kernel_loadaddr=0x3000000\0"               \
	"devicetree_loadaddr=0x2A00000\0"           \
	"rootfs_loadaddr=0x4000000\0"               \
	"ramdisk_loadaddr=0x4000000\0"               \
	"bootscript_loadaddr=0x1000000\0"           \
	"initrd_high=0x10000000\0" \
						\
	"preloader_size="   __stringify(QSPI_PRELOADER_SIZE) "\0"\
	"rootfs_size="     __stringify(QSPI_ROOTFS_SIZE) "\0"\
	"ramdisk_size="     __stringify(QSPI_RAMDISK_SIZE) "\0"\
	"kernel_size="     __stringify(QSPI_LINUX_SIZE)  "\0"\
	"devicetree_size=" __stringify(QSPI_DTB_SIZE)    "\0"\
	"bootscript_size=" __stringify(QSPI_BOOTSCRIPT_SIZE)   "\0"\
	"bitstream_size="   __stringify(QSPI_BITSTREAM_SIZE)    "\0"\
								\
	"qspi_kernel_offset="     __stringify(QSPI_LINUX_OFFSET) "\0"\
	"qspi_rootfs_offset="     __stringify(QSPI_ROOTFS_OFFSET)"\0"\
	"qspi_ramdisk_offset="     __stringify(QSPI_ROOTFS_OFFSET)"\0"\
	"qspi_devicetree_offset=" __stringify(QSPI_DTB_OFFSET)   "\0"\
	"qspi_bootscript_offset=" __stringify(QSPI_BOOTSCRIPT_OFFSET)  "\0"\
	"qspi_preloader_offset="  __stringify(QSPI_PRELOADER_OFFSET)   "\0"\
	"qspi_uboot_offset="      __stringify(QSPI_UBOOT_OFFSET)       "\0"\
	"qspi_uboot_erase_offset="  __stringify(QSPI_UBOOT_ERASE_ADDR)   "\0"\
	"qspi_uboot_erase_size="  __stringify(QSPI_UBOOT_ERASE_SIZE)   "\0"\
	"qspi_bitstream_offset="  __stringify(QSPI_BITSTREAM_OFFSET)   "\0"\
						 \
	"mmcargs=setenv bootargs console=ttyS0,115200 root=/dev/mmcblk0p3 rw rootwait\0"\
	"usbargs=setenv bootargs console=ttyS0,115200 root=/dev/sda2 rw rootwait\0"\
	"qspiargs=setenv bootargs console=ttyS0,115200 root=/dev/mtdblock1 rootfstype=jffs2 rw rootwait\0"\
	"nfsargs=setenv bootargs console=ttyS0,115200 root=/dev/nfs nfsroot=${serverip}:${serverpath},v3 rw rootwait ip=dhcp\0"\
	"qspiargs=setenv bootargs console=ttyS0,115200 root=/dev/mtdblock1 rootfstype=jffs2 rw rootwait\0"\
	"qspiramdiskargs=setenv bootargs console=ttyS0,115200 earlyprintk rw root=/dev/ram\0"\
	"def_args=console=ttyS0,115200 rw earlyprintk\0"\
	"ramdisk_args=setenv bootargs ${def_args} root=/dev/ram\0"\
	"qspiboot=echo Bootinq on QSPI Flash ...; " \
		"bridge enable && "	                \
		"sf probe && "                          \
		"sf read ${bootscript_loadaddr} ${qspi_bootscript_offset} ${bootscript_size} && "\
		"source ${bootscript_loadaddr}\0"       \
                                                \
	"mmcboot=echo Booting on SD Card ...; "      \
		"bridge enable && "                     \
		"mmc rescan && "                        \
		"load mmc 0 ${bootscript_loadaddr} ${bootscript_image} && "\
		"source ${bootscript_loadaddr}\0"       \
                                                \
	"usbboot=echo Booting on USB ...; "     \
		"bridge enable && "		\
		"usb start && "			\
		"load usb 0 ${bootscript_loadaddr} ${bootscript_image} && " \
		"source ${bootscript_loadaddr} \0"\
                                                \
    "netboot=echo Booting from TFTP/NFS ...; "  \
		"bridge enable && "             \
        "tftpboot ${bootscript_loadaddr} ${bootscript_image} && "\
        "source ${bootscript_loadaddr}\0"       \
                                                \
	"modeboot=setexpr.l bootsel *0xFFD08014 \\\\& 0x7;" \
		"if test ${bootsel} -eq 4 || test ${bootsel} -eq 5;"\
		"then;echo Booting from MMC ...;"\
		"run mmcboot;"\
		"fi;"\
		"if test ${bootsel} -eq 6 || test ${bootsel} -eq 7;"\
		"then;echo Booting from QSPI ...;"\
		"run qspiboot;"\
		"fi\0"

#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_SIZE			QSPI_ENV_SIZE
#define CONFIG_ENV_SECT_SIZE		CONFIG_ENV_SIZE
#define CONFIG_ENV_OFFSET		QSPI_ENV_OFFSET

/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

/* Configure the system clock */
#ifdef CONFIG_SYS_TIMER_RATE
#undef CONFIG_SYS_TIMER_RATE
#endif
#define MERCURY_SA1_SYS_TIMER_OSC	1
#define CONFIG_SYS_TIMER_RATE		cm_get_osc_clk_hz(MERCURY_SA1_SYS_TIMER_OSC)

#endif	/* __CONFIG_SOCFPGA_CYCLONE5_H__ */
