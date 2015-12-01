/*
 * Altera 10/100/1000 triple speed ethernet mac driver
 *
 * Copyright (C) 2008 Altera Corporation.
 * Copyright (C) 2010 Thomas Chou <thomas@wytron.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdt_support.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <asm/cache.h>
#include <asm/dma-mapping.h>
#include <asm/io.h>
#include "altera_tse.h"

DECLARE_GLOBAL_DATA_PTR;

static inline void alt_sgdma_construct_descriptor(
	struct alt_sgdma_descriptor *desc,
	struct alt_sgdma_descriptor *next,
	void *read_addr,
	void *write_addr,
	unsigned short length_or_eop,
	int generate_eop,
	int read_fixed,
	int write_fixed_or_sop)
{
	unsigned char val;

	/*
	 * Mark the "next" descriptor as "not" owned by hardware. This prevents
	 * The SGDMA controller from continuing to process the chain.
	 */
	next->descriptor_control = next->descriptor_control &
		~ALT_SGDMA_DESCRIPTOR_CONTROL_OWNED_BY_HW_MSK;

	memset(desc, 0, sizeof(struct alt_sgdma_descriptor));
	desc->source = virt_to_phys(read_addr);
	desc->destination = virt_to_phys(write_addr);
	desc->next = virt_to_phys(next);
	desc->bytes_to_transfer = length_or_eop;

	/*
	 * Set the descriptor control block as follows:
	 * - Set "owned by hardware" bit
	 * - Optionally set "generate EOP" bit
	 * - Optionally set the "read from fixed address" bit
	 * - Optionally set the "write to fixed address bit (which serves
	 *   serves as a "generate SOP" control bit in memory-to-stream mode).
	 * - Set the 4-bit atlantic channel, if specified
	 *
	 * Note this step is performed after all other descriptor information
	 * has been filled out so that, if the controller already happens to be
	 * pointing at this descriptor, it will not run (via the "owned by
	 * hardware" bit) until all other descriptor has been set up.
	 */
	val = ALT_SGDMA_DESCRIPTOR_CONTROL_OWNED_BY_HW_MSK;
	if (generate_eop)
		val |= ALT_SGDMA_DESCRIPTOR_CONTROL_GENERATE_EOP_MSK;
	if (read_fixed)
		val |= ALT_SGDMA_DESCRIPTOR_CONTROL_READ_FIXED_ADDRESS_MSK;
	if (write_fixed_or_sop)
		val |= ALT_SGDMA_DESCRIPTOR_CONTROL_WRITE_FIXED_ADDRESS_MSK;
	desc->descriptor_control = val;
}

static int alt_sgdma_wait_transfer(struct alt_sgdma_registers *regs)
{
	int status;
	ulong ctime;

	/* Wait for the descriptor (chain) to complete */
	ctime = get_timer(0);
	while (1) {
		status = readl(&regs->status);
		if (!(status & ALT_SGDMA_STATUS_BUSY_MSK))
			break;
		if (get_timer(ctime) > ALT_TSE_SGDMA_BUSY_TIMEOUT) {
			status = -ETIMEDOUT;
			debug("sgdma timeout\n");
			break;
		}
	}

	/* Clear Run */
	writel(0, &regs->control);
	/* Clear status */
	writel(0xff, &regs->status);

	return status;
}

static int alt_sgdma_start_transfer(struct alt_sgdma_registers *regs,
				    struct alt_sgdma_descriptor *desc)
{
	unsigned int val;

	/* Point the controller at the descriptor */
	writel(virt_to_phys(desc), &regs->next_descriptor_pointer);

	/*
	 * Set up SGDMA controller to:
	 * - Disable interrupt generation
	 * - Run once a valid descriptor is written to controller
	 * - Stop on an error with any particular descriptor
	 */
	val = ALT_SGDMA_CONTROL_RUN_MSK | ALT_SGDMA_CONTROL_STOP_DMA_ER_MSK;
	writel(val, &regs->control);

	return 0;
}

static void tse_adjust_link(struct altera_tse_priv *priv,
			    struct phy_device *phydev)
{
	struct alt_tse_mac *mac_dev = priv->mac_dev;
	unsigned int refvar;

	if (!phydev->link) {
		debug("%s: No link.\n", phydev->dev->name);
		return;
	}

	refvar = readl(&mac_dev->command_config);

	if (phydev->duplex)
		refvar |= ALTERA_TSE_CMD_HD_ENA_MSK;
	else
		refvar &= ~ALTERA_TSE_CMD_HD_ENA_MSK;

	switch (phydev->speed) {
	case 1000:
		refvar |= ALTERA_TSE_CMD_ETH_SPEED_MSK;
		refvar &= ~ALTERA_TSE_CMD_ENA_10_MSK;
		break;
	case 100:
		refvar &= ~ALTERA_TSE_CMD_ETH_SPEED_MSK;
		refvar &= ~ALTERA_TSE_CMD_ENA_10_MSK;
		break;
	case 10:
		refvar &= ~ALTERA_TSE_CMD_ETH_SPEED_MSK;
		refvar |= ALTERA_TSE_CMD_ENA_10_MSK;
		break;
	}
	writel(refvar, &mac_dev->command_config);
}

static int altera_tse_send(struct udevice *dev, void *packet, int length)
{
	struct altera_tse_priv *priv = dev_get_priv(dev);
	struct alt_sgdma_descriptor *tx_desc = priv->tx_desc;
	unsigned long tx_buf = (unsigned long)packet;

	flush_dcache_range(tx_buf, tx_buf + length);
	alt_sgdma_construct_descriptor(
		tx_desc,
		tx_desc + 1,
		packet,	/* read addr */
		NULL,	/* write addr */
		length,	/* length or EOP ,will change for each tx */
		1,	/* gen eop */
		0,	/* read fixed */
		1	/* write fixed or sop */
		);

	/* send the packet */
	alt_sgdma_start_transfer(priv->sgdma_tx, tx_desc);
	alt_sgdma_wait_transfer(priv->sgdma_tx);
	debug("sent %d bytes\n", tx_desc->actual_bytes_transferred);

	return tx_desc->actual_bytes_transferred;
}

static int altera_tse_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct altera_tse_priv *priv = dev_get_priv(dev);
	struct alt_sgdma_descriptor *rx_desc = priv->rx_desc;
	int packet_length;

	if (rx_desc->descriptor_status &
	    ALT_SGDMA_DESCRIPTOR_STATUS_TERMINATED_BY_EOP_MSK) {
		packet_length = rx_desc->actual_bytes_transferred;
		debug("recv %d bytes\n", packet_length);
		*packetp = priv->rx_buf;

		return packet_length;
	}

	return -EAGAIN;
}

static int altera_tse_free_pkt(struct udevice *dev, uchar *packet,
			       int length)
{
	struct altera_tse_priv *priv = dev_get_priv(dev);
	struct alt_sgdma_descriptor *rx_desc = priv->rx_desc;
	unsigned long rx_buf = (unsigned long)priv->rx_buf;

	alt_sgdma_wait_transfer(priv->sgdma_rx);
	invalidate_dcache_range(rx_buf, rx_buf + PKTSIZE_ALIGN);
	alt_sgdma_construct_descriptor(
		rx_desc,
		rx_desc + 1,
		NULL,	/* read addr */
		priv->rx_buf, /* write addr */
		0,	/* length or EOP */
		0,	/* gen eop */
		0,	/* read fixed */
		0	/* write fixed or sop */
		);

	/* setup the sgdma */
	alt_sgdma_start_transfer(priv->sgdma_rx, rx_desc);
	debug("recv setup\n");

	return 0;
}

static void altera_tse_stop(struct udevice *dev)
{
	struct altera_tse_priv *priv = dev_get_priv(dev);
	struct alt_tse_mac *mac_dev = priv->mac_dev;
	struct alt_sgdma_registers *rx_sgdma = priv->sgdma_rx;
	struct alt_sgdma_registers *tx_sgdma = priv->sgdma_tx;
	struct alt_sgdma_descriptor *rx_desc = priv->rx_desc;
	unsigned int status;
	int ret;
	ulong ctime;

	/* clear rx desc & wait for sgdma to complete */
	rx_desc->descriptor_control = 0;
	writel(0, &rx_sgdma->control);
	ret = alt_sgdma_wait_transfer(rx_sgdma);
	if (ret == -ETIMEDOUT)
		writel(ALT_SGDMA_CONTROL_SOFTWARERESET_MSK,
		       &rx_sgdma->control);

	writel(0, &tx_sgdma->control);
	ret = alt_sgdma_wait_transfer(tx_sgdma);
	if (ret == -ETIMEDOUT)
		writel(ALT_SGDMA_CONTROL_SOFTWARERESET_MSK,
		       &tx_sgdma->control);

	/* reset the mac */
	writel(ALTERA_TSE_CMD_SW_RESET_MSK, &mac_dev->command_config);
	ctime = get_timer(0);
	while (1) {
		status = readl(&mac_dev->command_config);
		if (!(status & ALTERA_TSE_CMD_SW_RESET_MSK))
			break;
		if (get_timer(ctime) > ALT_TSE_SW_RESET_TIMEOUT) {
			debug("Reset mac timeout\n");
			break;
		}
	}
}

static int tse_mdio_read(struct mii_dev *bus, int addr, int devad, int reg)
{
	struct altera_tse_priv *priv = bus->priv;
	struct alt_tse_mac *mac_dev = priv->mac_dev;
	unsigned int value;

	/* set mdio address */
	writel(addr, &mac_dev->mdio_phy1_addr);
	/* get the data */
	value = readl(&mac_dev->mdio_phy1[reg]);

	return value & 0xffff;
}

static int tse_mdio_write(struct mii_dev *bus, int addr, int devad, int reg,
			  u16 val)
{
	struct altera_tse_priv *priv = bus->priv;
	struct alt_tse_mac *mac_dev = priv->mac_dev;

	/* set mdio address */
	writel(addr, &mac_dev->mdio_phy1_addr);
	/* set the data */
	writel(val, &mac_dev->mdio_phy1[reg]);

	return 0;
}

static int tse_mdio_init(const char *name, struct altera_tse_priv *priv)
{
	struct mii_dev *bus = mdio_alloc();

	if (!bus) {
		printf("Failed to allocate MDIO bus\n");
		return -ENOMEM;
	}

	bus->read = tse_mdio_read;
	bus->write = tse_mdio_write;
	snprintf(bus->name, sizeof(bus->name), name);

	bus->priv = (void *)priv;

	return mdio_register(bus);
}

static int tse_phy_init(struct altera_tse_priv *priv, void *dev)
{
	struct phy_device *phydev;
	unsigned int mask = 0xffffffff;

	if (priv->phyaddr)
		mask = 1 << priv->phyaddr;

	phydev = phy_find_by_mask(priv->bus, mask, priv->interface);
	if (!phydev)
		return -ENODEV;

	phy_connect_dev(phydev, dev);

	phydev->supported &= PHY_GBIT_FEATURES;
	phydev->advertising = phydev->supported;

	priv->phydev = phydev;
	phy_config(phydev);

	return 0;
}

static int altera_tse_write_hwaddr(struct udevice *dev)
{
	struct altera_tse_priv *priv = dev_get_priv(dev);
	struct alt_tse_mac *mac_dev = priv->mac_dev;
	struct eth_pdata *pdata = dev_get_platdata(dev);
	u8 *hwaddr = pdata->enetaddr;
	unsigned int mac_lo, mac_hi;

	mac_lo = (hwaddr[3] << 24) | (hwaddr[2] << 16) |
		(hwaddr[1] << 8) | hwaddr[0];
	mac_hi = (hwaddr[5] << 8) | hwaddr[4];
	debug("Set MAC address to 0x%04x%08x\n", mac_hi, mac_lo);

	writel(mac_lo, &mac_dev->mac_addr_0);
	writel(mac_hi, &mac_dev->mac_addr_1);
	writel(mac_lo, &mac_dev->supp_mac_addr_0_0);
	writel(mac_hi, &mac_dev->supp_mac_addr_0_1);
	writel(mac_lo, &mac_dev->supp_mac_addr_1_0);
	writel(mac_hi, &mac_dev->supp_mac_addr_1_1);
	writel(mac_lo, &mac_dev->supp_mac_addr_2_0);
	writel(mac_hi, &mac_dev->supp_mac_addr_2_1);
	writel(mac_lo, &mac_dev->supp_mac_addr_3_0);
	writel(mac_hi, &mac_dev->supp_mac_addr_3_1);

	return 0;
}

static int altera_tse_start(struct udevice *dev)
{
	struct altera_tse_priv *priv = dev_get_priv(dev);
	struct alt_tse_mac *mac_dev = priv->mac_dev;
	unsigned int val;
	int ret;

	/* need to create sgdma */
	debug("Configuring rx desc\n");
	altera_tse_free_pkt(dev, priv->rx_buf, PKTSIZE_ALIGN);
	/* start TSE */
	debug("Configuring TSE Mac\n");
	/* Initialize MAC registers */
	writel(PKTSIZE_ALIGN, &mac_dev->max_frame_length);
	writel(priv->rx_fifo_depth - 16, &mac_dev->rx_sel_empty_threshold);
	writel(0, &mac_dev->rx_sel_full_threshold);
	writel(priv->tx_fifo_depth - 16, &mac_dev->tx_sel_empty_threshold);
	writel(0, &mac_dev->tx_sel_full_threshold);
	writel(8, &mac_dev->rx_almost_empty_threshold);
	writel(8, &mac_dev->rx_almost_full_threshold);
	writel(8, &mac_dev->tx_almost_empty_threshold);
	writel(3, &mac_dev->tx_almost_full_threshold);

	/* NO Shift */
	writel(0, &mac_dev->rx_cmd_stat);
	writel(0, &mac_dev->tx_cmd_stat);

	/* enable MAC */
	val = ALTERA_TSE_CMD_TX_ENA_MSK | ALTERA_TSE_CMD_RX_ENA_MSK;
	writel(val, &mac_dev->command_config);

	/* Start up the PHY */
	ret = phy_startup(priv->phydev);
	if (ret) {
		debug("Could not initialize PHY %s\n",
		      priv->phydev->dev->name);
		return ret;
	}

	tse_adjust_link(priv, priv->phydev);

	if (!priv->phydev->link)
		return -EIO;

	return 0;
}

static int altera_tse_probe(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_platdata(dev);
	struct altera_tse_priv *priv = dev_get_priv(dev);
	const void *blob = gd->fdt_blob;
	int node = dev->of_offset;
	const char *list, *end;
	const fdt32_t *cell;
	void *base, *desc_mem = NULL;
	unsigned long addr, size;
	int len, idx;
	int ret;

	/*
	 * decode regs, assume address-cells and size-cells are both one.
	 * there are multiple reg tuples, and they need to match with
	 * reg-names.
	 */
	list = fdt_getprop(blob, node, "reg-names", &len);
	if (!list)
		return -ENOENT;
	end = list + len;
	cell = fdt_getprop(blob, node, "reg", &len);
	if (!cell)
		return -ENOENT;
	idx = 0;
	while (list < end) {
		addr = fdt_translate_address((void *)blob,
					     node, cell + idx);
		size = fdt_addr_to_cpu(cell[idx + 1]);
		base = ioremap(addr, size);
		len = strlen(list);
		if (strcmp(list, "control_port") == 0)
			priv->mac_dev = base;
		else if (strcmp(list, "rx_csr") == 0)
			priv->sgdma_rx = base;
		else if (strcmp(list, "tx_csr") == 0)
			priv->sgdma_tx = base;
		else if (strcmp(list, "s1") == 0)
			desc_mem = base;
		idx += 2;
		list += (len + 1);
	}
	/* decode fifo depth */
	priv->rx_fifo_depth = fdtdec_get_int(blob, node,
		"rx-fifo-depth", 0);
	priv->tx_fifo_depth = fdtdec_get_int(blob, node,
		"tx-fifo-depth", 0);
	/* decode phy */
	addr = fdtdec_get_int(blob, node,
			      "phy-handle", 0);
	addr = fdt_node_offset_by_phandle(blob, addr);
	priv->phyaddr = fdtdec_get_int(blob, addr,
		"reg", 0);
	/* init desc */
	len = sizeof(struct alt_sgdma_descriptor) * 4;
	if (!desc_mem) {
		desc_mem = dma_alloc_coherent(len, &addr);
		if (!desc_mem)
			return -ENOMEM;
	}
	memset(desc_mem, 0, len);
	priv->tx_desc = desc_mem;
	priv->rx_desc = priv->tx_desc + 2;
	/* allocate recv packet buffer */
	priv->rx_buf = malloc_cache_aligned(PKTSIZE_ALIGN);
	if (!priv->rx_buf)
		return -ENOMEM;

	/* stop controller */
	debug("Reset TSE & SGDMAs\n");
	altera_tse_stop(dev);

	/* start the phy */
	priv->interface = pdata->phy_interface;
	tse_mdio_init(dev->name, priv);
	priv->bus = miiphy_get_dev_by_name(dev->name);

	ret = tse_phy_init(priv, dev);

	return ret;
}

static int altera_tse_ofdata_to_platdata(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_platdata(dev);
	const char *phy_mode;

	pdata->phy_interface = -1;
	phy_mode = fdt_getprop(gd->fdt_blob, dev->of_offset, "phy-mode", NULL);
	if (phy_mode)
		pdata->phy_interface = phy_get_interface_by_name(phy_mode);
	if (pdata->phy_interface == -1) {
		debug("%s: Invalid PHY interface '%s'\n", __func__, phy_mode);
		return -EINVAL;
	}

	return 0;
}

static const struct eth_ops altera_tse_ops = {
	.start		= altera_tse_start,
	.send		= altera_tse_send,
	.recv		= altera_tse_recv,
	.free_pkt	= altera_tse_free_pkt,
	.stop		= altera_tse_stop,
	.write_hwaddr	= altera_tse_write_hwaddr,
};

static const struct udevice_id altera_tse_ids[] = {
	{ .compatible = "altr,tse-1.0", },
	{ }
};

U_BOOT_DRIVER(altera_tse) = {
	.name	= "altera_tse",
	.id	= UCLASS_ETH,
	.of_match = altera_tse_ids,
	.ops	= &altera_tse_ops,
	.ofdata_to_platdata = altera_tse_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct eth_pdata),
	.priv_auto_alloc_size = sizeof(struct altera_tse_priv),
	.probe	= altera_tse_probe,
};
