// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Enclustra GmbH
 * <info@enclustra.com>
 */

#include <i2c.h>
#include <linux/delay.h>
#include "si5338_config.h"
#include "Si5338-RevB-Registers.h"

int i2c_write_simple(struct udevice *dev, u8 addr, u8 data)
{
        unsigned char buf[1];
        buf[0] = data;
        if (dm_i2c_write(dev, addr, buf, 1) != 0) {
                printf("SI5338 I2C write failed: %02x, %02x\n", addr, data);
                return -1;
        }
        return 0;
}

int i2c_write_masked(struct udevice *dev, u8 addr, u8 data, u8 mask)
{
        if (mask == 0x00) return 0;
        if (mask == 0xff) return i2c_write_simple(dev, addr, data);

        unsigned char buf[1];
        if (dm_i2c_read(dev, addr, buf, 1) != 0) {
                printf("SI5338 I2C read failed\n");
                return -1;
        }
        buf[0] &= ~mask;
        buf[0] |= data & mask;
        if (dm_i2c_write(dev, addr, buf, 1) != 0) {
                printf("SI5338 I2C write failed: %02x, %02x, %02x\n", addr, data, mask);
                return -1;
        }
        return 0;
}

int si5338_init(void)
{
        unsigned char buf[1];
        struct udevice *dev;

        if (i2c_get_chip_for_busnum(0, 0x70, 1, &dev) != 0) {
                printf("SI5338 I2C init failed\n");
                return -1;
        }

        // set page to 0
        if (i2c_write_simple(dev, 255, 0x00)) {
                return -1;
        }

        // disable outputs
        if (i2c_write_masked(dev, 230, 0x10, 0x10)) {
                return -1;
        }

        // pause lol
        if (i2c_write_masked(dev, 241, 0x80, 0x80)) {
                return -1;
        }

        // write new configuration
        for (int i=0; i<NUM_REGS_MAX; i++) {
                if (i2c_write_masked(dev, Reg_Store[i].Reg_Addr, Reg_Store[i].Reg_Val, Reg_Store[i].Reg_Mask)) {
                        return -1;
                }
        }

        // validate input clock status
        do {
                if (dm_i2c_read(dev, 218, buf, 1) != 0) {
                        printf("SI5338 I2C read failed\n");
                        return -1;
                }
        }
        while ((buf[0] & 0x04) != 0);

        // configure PLL for locking
        if (i2c_write_masked(dev, 49, 0, 0x80)) {
                return -1;
        }

        // initiate locking of PLL
        if (i2c_write_simple(dev, 246, 0x02)) {
                return -1;
        }

        // wait 25ms (100ms to be on the safe side)
        mdelay(100);

        // restart lol
        if (i2c_write_masked(dev, 241, 0x65, 0xff)) {
                return -1;
        }

        // confirm PLL lock status
        int try = 0;
        do {
                if (dm_i2c_read(dev, 218, buf, 1) != 0) {
                        printf("SI5338 I2C read failed\n");
                        return -1;
                }
                mdelay(100);
                try++;
                if (try > 10) {
                    printf("SI5338 PLL is not locking\n");
                    return -1;
                }
        }
        while ((buf[0] & 0x15) != 0);

        // copy fcal values to active registers
        if (dm_i2c_read(dev, 237, buf, 1) != 0) {
                printf("SI5338 I2C failed\n");
                return -1;
        }
        if (i2c_write_masked(dev, 47, buf[0], 0x03)) {
                return -1;
        }

        if (dm_i2c_read(dev, 236, buf, 1) != 0) {
                printf("SI5338 I2C failed\n");
                return -1;
        }
        if (i2c_write_masked(dev, 46, buf[0], 0xFF)) {
                return -1;
        }

        if (dm_i2c_read(dev, 235, buf, 1) != 0) {
                printf("SI5338 I2C failed\n");
                return -1;
        }
        if (i2c_write_simple(dev, 45, buf[0])) {
                return -1;
        }

        if (i2c_write_masked(dev, 47, 0x14, 0xFC)) {
                return -1;
        }

        // set PLL to use FCAL values
        if (i2c_write_masked(dev, 49, 0x80, 0x80)) {
                return -1;
        }

        // enable outputs
        if (i2c_write_simple(dev, 230, 0x00)) {
                return -1;
        }

        printf("SI5338 init successful\n");

        return 0;
};

EXPORT_SYMBOL_GPL(si5338_init);
