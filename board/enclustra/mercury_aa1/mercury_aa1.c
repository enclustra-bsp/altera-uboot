// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Enclustra GmbH
 * <info@enclustra.com>
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <init.h>
#include <dm/uclass.h>
#include <asm-generic/gpio.h>
#include <asm/io.h>

/* Enclustra vendor ID */
#define ENCLUSTRA_MAC               0xF7B020

/* Default MAC address */
#define ENCLUSTRA_ETHADDR_DEFAULT "20:B0:F7:01:02:03"
#define ENCLUSTRA_ETH1ADDR_DEFAULT "20:B0:F7:01:02:04"

/* Pin muxing */
#if !defined(CONFIG_SPL_BUILD)

#define ALTERA_NONE 0
#define ALTERA_MMC 1
#define ALTERA_QSPI 2
#define ALTERA_EMMC 3
#define MMC_CLK_DIV 0x9
#define QSPI_CLK_DIV 0x384
#define ALTERA_PINMUX_OFFS 0xffd07200
#define ALTERA_CLKMGR_MAINPLL_CNTR6CLK_BASE 0xFFD04078

static int altera_current_storage = ALTERA_NONE;

#endif

int configure_mac(void)
{
	int i;
	u8 hwaddr[6] = {0, 0, 0, 0, 0, 0};
	u32 hwaddr_h;
	char hwaddr_str[18];
	bool hwaddr_set = false;

	return 0;
}

int board_late_init(void)
{
	int ret;
	ret = configure_mac();
	return ret;
}

#if !defined(CONFIG_SPL_BUILD)

static void set_mux_mmc (void)
{
	u32 pinmux_arr[] = {0x0c, 0x8,  // IO4 connected to SDMMC
                        0x10, 0x8,  // IO5
                        0x14, 0x8,  // IO6
                        0x18, 0x8,  // IO7
                        0x1c, 0x8,  // IO8
                        0x20, 0x8,  // IO9
                        0x24, 0xf,  // IO10 connected to GPIO
                        0x28, 0xf,  // IO11
                        0x2c, 0xf,  // IO12
                        0x30, 0xf,  // IO13
                        0x34, 0xf,  // IO14
                        0x38, 0xf}; // IO15
	u32 len, i, offset, value;
	len = sizeof(pinmux_arr)/sizeof(u32);
	for (i=0; i<len; i+=2) {
		offset = pinmux_arr[i];
		value = pinmux_arr[i+1];
		writel(value, ALTERA_PINMUX_OFFS + offset);
	}
}

static void set_mux_emmc (void)
{
	u32 pinmux_arr[] = {0x0c, 0x8,  // IO4
                        0x10, 0x8,  // IO5
                        0x14, 0x8,  // IO6
                        0x18, 0x8,  // IO7
                        0x1c, 0x8,  // IO8
                        0x20, 0x8,  // IO9
                        0x24, 0xf,  // IO10
                        0x28, 0xf,  // IO11
                        0x2c, 0x8,  // IO12
                        0x30, 0x8,  // IO13
                        0x34, 0x8,  // IO14
                        0x38, 0x8}; // IO15
	u32 len, i, offset, value;
	len = sizeof(pinmux_arr)/sizeof(u32);
	for (i=0; i<len; i+=2) {
		offset = pinmux_arr[i];
		value = pinmux_arr[i+1];
		writel(value, ALTERA_PINMUX_OFFS + offset);
	}
}

static void set_mux_qspi (void)
{
	u32 pinmux_arr[] = {0x0c, 0x4,  // IO4 connected to QSPI
                        0x10, 0x4,  // IO5
                        0x14, 0x4,  // IO6
                        0x18, 0x4,  // IO7
                        0x1c, 0x4,  // IO8
                        0x20, 0x4,  // IO9
                        0x24, 0xf,  // IO10
                        0x28, 0xf,  // IO11
                        0x2c, 0xf,  // IO12
                        0x30, 0xf,  // IO13
                        0x34, 0xf,  // IO14
                        0x38, 0xf}; // IO15
	u32 len, i, offset, value;
	len = sizeof(pinmux_arr)/sizeof(u32);
	for (i=0; i<len; i+=2) {
		offset = pinmux_arr[i];
		value = pinmux_arr[i+1];
		writel(value, ALTERA_PINMUX_OFFS + offset);
	}
}

void altera_set_storage (int store)
{
	if (store == altera_current_storage)
		return;

	unsigned int gpio_flash_sel;
	unsigned int gpio_flash_oe;

	if (gpio_lookup_name("portb5", NULL, NULL, &gpio_flash_oe))
	{
		printf("ERROR: GPIO not found\n");
		return;
	}

	if (gpio_request(gpio_flash_oe, "flash_oe"))
	{
		printf("ERROR: GPIO request failed\n");
		return;
	}

	if (gpio_lookup_name("portc6", NULL, NULL, &gpio_flash_sel))
	{
		printf("ERROR: GPIO not found\n");
		return;
	}

	if (gpio_request(gpio_flash_sel, "flash_sel"))
	{
		printf("ERROR: GPIO request failed\n");
		return;
	}

	switch (store)
	{
		case ALTERA_MMC:
			set_mux_mmc();
			gpio_direction_output(gpio_flash_sel, 0);
			gpio_direction_output(gpio_flash_oe, 0);
			altera_current_storage = ALTERA_MMC;
			writel(MMC_CLK_DIV, ALTERA_CLKMGR_MAINPLL_CNTR6CLK_BASE);
			break;
		case ALTERA_EMMC:
			set_mux_emmc();
			gpio_direction_output(gpio_flash_sel, 1);
			gpio_direction_output(gpio_flash_oe, 1);
			altera_current_storage = ALTERA_EMMC;
			writel(MMC_CLK_DIV, ALTERA_CLKMGR_MAINPLL_CNTR6CLK_BASE);
			break;
		case ALTERA_QSPI:
			set_mux_qspi();
			gpio_direction_output(gpio_flash_sel, 1);
			gpio_direction_output(gpio_flash_oe, 0);
			altera_current_storage = ALTERA_QSPI;
			writel(QSPI_CLK_DIV, ALTERA_CLKMGR_MAINPLL_CNTR6CLK_BASE );
			break;
		default:
			altera_current_storage = ALTERA_NONE;
			break;
	}

	gpio_free(gpio_flash_sel);
	gpio_free(gpio_flash_oe);
}

int altera_set_storage_cmd(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	if(argc != 2)
		return CMD_RET_USAGE;
	if(!strcmp(argv[1], "MMC"))
		altera_set_storage(ALTERA_MMC);
	else if (!strcmp(argv[1], "QSPI"))
		altera_set_storage(ALTERA_QSPI);
	else if (!strcmp(argv[1], "EMMC"))
		altera_set_storage(ALTERA_EMMC);
	else return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(altera_set_storage, 2, 0, altera_set_storage_cmd, "Set non volatile memory access", "<MMC|QSPI|EMMC> - Set access for the selected memory device");

#endif
