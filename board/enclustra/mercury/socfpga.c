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

#define ATSHA204_COMMAND	0x03
#define ATSHA204_READ_CMD 	0x02

#define ATSHA204_CONFIG_ZONE 	0x00
#define ATSHA204_OTP_ZONE	0x01
#define ATSHA204_DATA_ZONE	0x02

#define ATSHA204_READ32_BYTES_FLAG	(1<<7)

u16 atsha204_crc16(const u8 *buf, const u8 len)
{
        u8 i;
        u16 crc16 = 0;

        for (i = 0; i < len; i++) {
                u8 shift;

                for (shift = 0x01; shift > 0x00; shift <<= 1) {
                        u8 data_bit = (buf[i] & shift) ? 1 : 0;
                        u8 crc_bit = crc16 >> 15;

                        crc16 <<= 1;

                        if ((data_bit ^ crc_bit) != 0)
                                crc16 ^= 0x8005;
                }
        }

        return crc16;
}

struct __attribute__((packed, aligned(1))) atsha204_read_command {
	u8 count;
	u8 opcode;
	u8 param1;
	u16 param2;
	u16 checksum;
};


int atsha204_send_read_cmd(u8 i2c_address, u8 zone, u16 address, u8 read_block) {

	int ret;
	u16 crc;
	struct atsha204_read_command packet;

	packet.count = sizeof(struct atsha204_read_command);
	packet.opcode = ATSHA204_READ_CMD;
	packet.param1 = zone;
	if(read_block) packet.param1 |= ATSHA204_READ32_BYTES_FLAG;
	packet.param2 = address;

	crc = atsha204_crc16((u8*)(&packet), sizeof(struct atsha204_read_command) - 2);
	packet.checksum = crc;

	ret = i2c_write(i2c_address,
		  ATSHA204_COMMAND,
		  1,
		  (u8*)&packet,
		  sizeof(struct atsha204_read_command));
	if(ret) {
		printf("Writing failed \n");
		return ret;
	}
	/* reading may take up to 4 ms */
	udelay(4000);

	return 0;
}

int atsha204_wakeup(u8 i2c_address) {

	u8 wake_cmd = 0x0;

	return i2c_write(i2c_address,
		  0,
		  0,
		  &wake_cmd,
		  1);

}

int atsha204_read_data(u8 i2c_address, u8* buffer, u8 len) {

	int ret = 0;
	u8 first_word[4];
	u8 msg_len;
	u8 i;

	if (len < 4) return -ENOMEM;
	/* read the first 4 bytes from the device*/
	ret = i2c_read(i2c_address,
			0,
			0,
			first_word,
			4);

	if(ret) return ret;

	/* the first transferred byte is total length of the msg */
	msg_len = first_word[0];

	for(i = 0; i < 3; i++) buffer[i] = first_word[i+1];

	msg_len -= 4;

	if(!msg_len) {
		buffer[3] = 0xff;
		return 4;
	}

	if( (len-3) < msg_len ) return -ENOMEM;

	/* receive the rest of the data */

	ret = i2c_read(i2c_address,
			0,
			0,
			(u8*)(buffer + 3),
			msg_len);
	if(ret) return ret;
	return msg_len + 3;
}


int atsha204_read_otp_register(u8 i2c_address, u8 reg, u8* buffer) {

	u8 data[8];
	u8 i;
	int ret;

	ret = atsha204_wakeup(i2c_address);
	if(ret) return ret;

	ret = atsha204_send_read_cmd(i2c_address, ATSHA204_OTP_ZONE, reg, 0);
	if(ret) return ret;

	/* Attempt to read the register */

	ret = atsha204_read_data(i2c_address, data, 8);
	if(ret < 0) return ret;

	for(i = 0; i < 4; i++) buffer[i] = data[i];

	return 0;
}

int atsha204_get_mac(u8 i2c_address, u8* buffer) {

	int ret;
	u8 data[4];
	u8 i;

	ret = atsha204_read_otp_register(i2c_address, 4, data);
	if(ret)	return ret;
	else for(i = 0; i < 4; i++) buffer[i] = data[i];

	ret = atsha204_read_otp_register(i2c_address, 5, data);
	if(ret)	return ret;
	else {
		buffer[4] = data[0];
		buffer[5] = data[1];
	}
	return 0;
}

int ds28_get_mac(u8 i2c_address, u8* buffer) {

	return i2c_read(i2c_address,
	       0x10,
	       1,
	       buffer,
	       6);

}

struct eeprom_mem {
	u8 i2c_addr;
	int (*mac_reader)(u8 i2c_address, u8* buffer);
	int (*wakeup)(u8 i2c_address);
};

static struct eeprom_mem eeproms[] = {
	{ .i2c_addr = 0x64,
	  .mac_reader = atsha204_get_mac,
	  .wakeup = atsha204_wakeup,
	},
	{ .i2c_addr = 0x5C,
	  .mac_reader = ds28_get_mac,
	  .wakeup = NULL,}
};

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
	int i;
	u8 hwaddr[6] = {0, 0, 0, 0, 0, 0};
	u32 hwaddr_h;
	char hwaddr_str[16];
	bool hwaddr_set;

	hwaddr_set = false;

	if (getenv("ethaddr") == NULL) {
		/* Init i2c */
		i2c_init(0, 0);
		i2c_set_bus_num(0);

		for (i = 0; i < ARRAY_SIZE(eeproms); i++) {

			if(eeproms[i].wakeup)
				eeproms[i].wakeup(eeproms[i].i2c_addr);

			/* Probe the chip */
			if(i2c_probe(eeproms[i].i2c_addr))
				continue;

			if(eeproms[i].mac_reader(eeproms[i].i2c_addr, hwaddr))
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

			/* Check if the value is a valid mac registered for
			 * Enclustra  GmbH */
			hwaddr_h = hwaddr[0] | hwaddr[1] << 8 | hwaddr[2] << 16;
			if ((hwaddr_h & 0xFFFFFF) != ENCLUSTRA_MAC)
				continue;

			/* Set the actual env variable */
			setenv("ethaddr", hwaddr_str);
			hwaddr_set = true;
			break;
		}
		if (!hwaddr_set)
			setenv("ethaddr", ENCLUSTRA_ETHADDR_DEFAULT);
	}
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
