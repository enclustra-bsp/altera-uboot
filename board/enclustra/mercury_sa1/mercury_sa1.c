// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Enclustra GmbH
 * <info@enclustra.com>
 */

#include <common.h>
#include <command.h>
#include <linux/delay.h>
#include <env.h>
#include <init.h>
#include <dm/uclass.h>
#include <asm-generic/gpio.h>
#include <asm/io.h>
#include <enclustra/eeprom-mac.h>
#include <enclustra/si5338_config.h>

/* Enclustra vendor ID */
#define ENCLUSTRA_MAC 0xF7B020

/* Default MAC address */
#define ENCLUSTRA_ETHADDR_DEFAULT "20:B0:F7:01:02:03"
#define ENCLUSTRA_ETH1ADDR_DEFAULT "20:B0:F7:01:02:04"

/* Pin muxing */
#if !defined(CONFIG_SPL_BUILD)

#define GPIO_NR_USB_RESET_N 0
#define GPIO_NR_ETH_RESET_N 44
#define GPIO_NR_EMMC_EN_N 53

#define ALTERA_NONE 0
#define ALTERA_MMC 1
#define ALTERA_EMMC 2

static int altera_current_storage = ALTERA_NONE;

#endif

static struct eeprom_mem eeproms[] = {
	{ .mac_reader = ds28_get_mac },
};

int configure_mac(void)
{
	int i;
	u8 hwaddr[6] = {0, 0, 0, 0, 0, 0};
	u32 hwaddr_h;
	char hwaddr_str[18];
	bool hwaddr_set = false;

#ifdef CONFIG_ENCLUSTRA_EEPROM_MAC

	if (env_get("ethaddr")) {
		/* Address is already set */
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(eeproms); i++) {
		if (eeproms[i].mac_reader(hwaddr))
			continue;

		/* Check if the value is a valid mac registered for
		 * Enclustra  GmbH
		 */
		hwaddr_h = hwaddr[0] | hwaddr[1] << 8 | hwaddr[2] << 16;
		if ((hwaddr_h & 0xFFFFFF) != ENCLUSTRA_MAC)
			continue;

		/* Format the address using a string */
		sprintf(hwaddr_str,
			"%02X:%02X:%02X:%02X:%02X:%02X",
			hwaddr[0],
			hwaddr[1],
			hwaddr[2],
			hwaddr[3],
			hwaddr[4],
			hwaddr[5]);

		/* Set the actual env variable */
		env_set("ethaddr", hwaddr_str);

		/* increment MAC addr */
		hwaddr_h = (hwaddr[3] << 16) | (hwaddr[4] << 8) | hwaddr[5];
		hwaddr_h = (hwaddr_h + 1) & 0xFFFFFF;
		hwaddr[3] = (hwaddr_h >> 16) & 0xFF;
		hwaddr[4] = (hwaddr_h >> 8) & 0xFF;
		hwaddr[5] = hwaddr_h & 0xFF;

		/* Format the address using a string */
		sprintf(hwaddr_str,
			"%02X:%02X:%02X:%02X:%02X:%02X",
			hwaddr[0],
			hwaddr[1],
			hwaddr[2],
			hwaddr[3],
			hwaddr[4],
			hwaddr[5]);

		/* Set the actual env variable */
		env_set("eth1addr", hwaddr_str);

		hwaddr_set = true;
		break;
	}

	if (!hwaddr_set) {
		env_set("ethaddr", ENCLUSTRA_ETHADDR_DEFAULT);
		env_set("eth1addr", ENCLUSTRA_ETH1ADDR_DEFAULT);
	}

#endif
	return 0;
}

#if !defined(CONFIG_SPL_BUILD)

void release_eth_reset(void)
{
	gpio_direction_output(GPIO_NR_ETH_RESET_N, 1);
}

void release_usb_reset(void)
{
	gpio_direction_output(GPIO_NR_USB_RESET_N, 1);
}

#endif

int board_early_init_r(void)
{
#if !defined(CONFIG_SPL_BUILD)
	if (gpio_request(GPIO_NR_EMMC_EN_N, "emmc_en_n"))
	{
		printf("ERROR: EMMC enable GPIO request failed\n");
		return -1;
	}

	if (gpio_request(GPIO_NR_ETH_RESET_N, "eth_reset_n"))
	{
		printf("ERROR: ETH reset GPIO request failed\n");
		return -1;
	}

	if (gpio_request(GPIO_NR_USB_RESET_N, "usb_reset_n"))
	{
		printf("ERROR: USB reset GPIO request failed\n");
		return -1;
	}

	release_eth_reset();
	release_usb_reset();
	udelay(100);

#endif

	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_SI5338_CONFIGURATION
	si5338_init();
#endif

	int ret;
	ret = configure_mac();

	return ret;
}

#if !defined(CONFIG_SPL_BUILD)

void altera_set_storage (int store)
{
	if (store == altera_current_storage)
		return;

	switch (store)
	{
		case ALTERA_MMC:
			gpio_direction_output(GPIO_NR_EMMC_EN_N, 1);
			altera_current_storage = ALTERA_MMC;
			break;
		case ALTERA_EMMC:
			gpio_direction_output(GPIO_NR_EMMC_EN_N, 0);
			altera_current_storage = ALTERA_EMMC;
			break;
		default:
			altera_current_storage = ALTERA_NONE;
			break;
	}
}

int altera_set_storage_cmd(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	if(argc != 2)
		return CMD_RET_USAGE;
	if(!strcmp(argv[1], "MMC"))
		altera_set_storage(ALTERA_MMC);
	else if (!strcmp(argv[1], "EMMC"))
		altera_set_storage(ALTERA_EMMC);
	else return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(altera_set_storage, 2, 0, altera_set_storage_cmd, "Set non volatile memory access", "<MMC|EMMC> - Set access for the selected memory device");

#endif