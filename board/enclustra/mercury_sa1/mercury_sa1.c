// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Enclustra GmbH
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
#define ENCLUSTRA_MAC               0xF7B020

/* Default MAC address */
#define ENCLUSTRA_ETHADDR_DEFAULT "20:B0:F7:01:02:03"
#define ENCLUSTRA_ETH1ADDR_DEFAULT "20:B0:F7:01:02:04"

static struct eeprom_mem eeproms[] = {
	{ .mac_reader = atsha204_get_mac },
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

void release_eth_reset(void)
{
	const unsigned int gpio_nr_eth_reset_n = 44;

	if (gpio_request(gpio_nr_eth_reset_n, "eth_reset_n"))
	{
		printf("ERROR: ETH reset GPIO request failed\n");
		return;
	}

	gpio_direction_output(gpio_nr_eth_reset_n, 1);
}

void release_usb_reset(void)
{
	const unsigned int gpio_nr_usb_reset_n = 0;

	if (gpio_request(gpio_nr_usb_reset_n, "usb_reset_n"))
	{
		printf("ERROR: USB reset GPIO request failed\n");
		return;
	}

	gpio_direction_output(gpio_nr_usb_reset_n, 1);
}

int board_early_init_r(void)
{
	release_eth_reset();
	release_usb_reset();
	udelay(100);

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

