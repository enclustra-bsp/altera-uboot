/*
 * Copyright (C) 2022 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:    GPL-2.0
 */
#ifndef _FS_LOADER_H_
#define _FS_LOADER_H_

struct udevice;

/**
 * struct phandle_part - A place for storing phandle of node and its partition
 *
 * This holds information about a phandle of the block device, and its
 * partition where the firmware would be loaded from.
 *
 * @phandle: Phandle of storage device node
 * @partition: Partition of block device
 */
struct phandle_part {
	u32 phandle;
	u32 partition;
};

/**
 * struct sf_config - A place for storing serial flash configuration
 *
 * This holds information about bus, chip-select, and speed and mode of a serial
 * flash configuration.
 *
 * @bus: SPI bus number.
 * @cs: SPI chip selection.
 */
struct sf_config {
	u32 bus;
	u32 cs;
};

/**
 * enum data_flags - Flag to indicate data as RAW or as filesystem
 *
 * DATA_RAW: Data stored as RAW.
 * DATA_FS: DATA stored as filesystem.
 */
enum data_flags {
	DATA_RAW, /* Stored in raw */
	DATA_FS,  /* Stored within a file system */
};

/**
 * enum storage_types - Storage type to indicate block device, SPI device
 *			and NAND device.
 *
 * BLOCK_DEV: BLock device.
 * SPI_DEV: SPI/NOR flash.
 * NAND_DEV: NAND flash.
 */
enum storage_types {
	BLOCK_DEV, /* BLock device such as SDMMC */
	SPI_DEV,  /* SPI/NOR flash */
	NAND_DEV,  /* NAND flash */
};

/**
 * struct phandle_part - A place for storing all supported storage devices
 *
 * This holds information about all supported storage devices for driver use.
 *
 * @phandlepart: Attribute data for block device.
 * @mtdpart: MTD partition for ubi partition.
 * @ubivol: UBI volume-name for ubifsmount.
 * @enum data_flags: Data type (RAW or filesystem).
 * @struct sf_config: Serial flash configuration.
 * @struct spi_flash: Information about a SPI flash.
 */
struct device_plat {
	struct phandle_part phandlepart;
	char *mtdpart;
	char *ubivol;
	enum data_flags data_type;
	enum storage_types storage_type;
	struct sf_config sfconfig;
	struct udevice *flash;
};

/**
 * request_firmware_into_buf - Load firmware into a previously allocated buffer.
 * @dev: An instance of a driver.
 * @name: Name of firmware file.
 * @buf: Address of buffer to load firmware into.
 * @size: Size of buffer.
 * @offset: Offset of a file for start reading into buffer.
 *
 * The firmware is loaded directly into the buffer pointed to by @buf.
 *
 * Return: Size of total read, negative value when error.
 */
int request_firmware_into_buf(struct udevice *dev,
			      const char *name,
			      void *buf, size_t size, u32 offset);
#endif
