/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/reset_manager.h>
#include <asm/io.h>

#include <usb.h>
#include <usb/s3c_udc.h>
#include <usb_mass_storage.h>

#include <micrel.h>
#include <netdev.h>
#include <phy.h>

#include <i2c.h>

DECLARE_GLOBAL_DATA_PTR;

void s_init(void) {}

/*
 * Miscellaneous platform dependent initialisations
 */
int board_init(void)
{
	/* Address of boot parameters for ATAG (if ATAG is used) */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	return 0;
}

int board_late_init(void) {
#if defined(ENCLUSTRA_EEPROM_ADDR_TAB) && defined(ENCLUSTRA_EEPROM_HWMAC_REG)
	u8 chip_addr_tab[] = ENCLUSTRA_EEPROM_ADDR_TAB;
	int i, ret;
	u8 hwaddr[6];
	u32 hwaddr_h;
	char hwaddr_str[16];

	if (getenv("ethaddr") == NULL) {
		/* Init i2c */
		i2c_init(0, 0);
		i2c_set_bus_num(0);

		for (i = 0; i < ARRAY_SIZE(chip_addr_tab); i++) {
			/* Probe the chip */
			if (i2c_probe(chip_addr_tab[i]) != 0)
				continue;

			/* Attempt to read the mac address */
			ret = i2c_read(chip_addr_tab[i],
				       ENCLUSTRA_EEPROM_HWMAC_REG,
				       1,
				       hwaddr,
				       6);

			/* Do not continue if read failed */
			if (ret)
				continue;

			/* Check if the value is a valid mac registered for
			 * Enclustra GmbH */
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
			setenv("ethaddr", hwaddr_str);
			break;
		}
	}
#endif
  return 0;
}

/*
 * PHY configuration
 */
#ifdef CONFIG_PHY_MICREL_KSZ9021
int board_phy_config(struct phy_device *phydev)
{
	int ret;
	/*
	 * These skew settings for the KSZ9021 ethernet phy is required for ethernet
	 * to work reliably on most flavors of cyclone5 boards.
	 */
	ret = ksz9021_phy_extended_write(phydev,
					 MII_KSZ9021_EXT_RGMII_RX_DATA_SKEW,
					 0x0);
	if (ret)
		return ret;

	ret = ksz9021_phy_extended_write(phydev,
					 MII_KSZ9021_EXT_RGMII_TX_DATA_SKEW,
					 0x0);
	if (ret)
		return ret;

	ret = ksz9021_phy_extended_write(phydev,
					 MII_KSZ9021_EXT_RGMII_CLOCK_SKEW,
					 0xf0f0);
	if (ret)
		return ret;

	if (phydev->drv->config)
		return phydev->drv->config(phydev);

	return 0;
}
#endif

#ifdef CONFIG_USB_GADGET
struct s3c_plat_otg_data socfpga_otg_data = {
	.regs_otg	= CONFIG_USB_DWC2_REG_ADDR,
	.usb_gusbcfg	= 0x1417,
};

int board_usb_init(int index, enum usb_init_type init)
{
	return s3c_udc_probe(&socfpga_otg_data);
}

int g_dnl_board_usb_cable_connected(void)
{
	return 1;
}
#endif
