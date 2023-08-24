// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Socionext Inc.
 *   Author: Masahiro Yamada <yamada.masahiro@socionext.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <asm/arch/clock_manager.h>
#include <asm/global_data.h>
#include <dm/device_compat.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#include <linux/libfdt.h>
#include <mmc.h>
#include <reset-uclass.h>
#include <sdhci.h>

/* General define */
#define SD_MIN_CLK 400000

/* HRS - Host Register Set (specific to Cadence) */
#define SDHCI_CDNS_HRS04		0x10		/* PHY access port */
#define SDHCI_CDNS_HRS05		0x14		/* PHY data access port */
#define   SDHCI_CDNS_HRS04_ACK			BIT(26)
#define   SDHCI_CDNS_HRS04_RD			BIT(25)
#define   SDHCI_CDNS_HRS04_WR			BIT(24)
#define   SDHCI_CDNS_HRS04_RDATA		GENMASK(23, 16)
#define   SDHCI_CDNS_HRS04_WDATA		GENMASK(15, 8)
#define   SDHCI_CDNS_HRS04_ADDR			GENMASK(5, 0)

#define SDHCI_CDNS_HRS06		0x18		/* eMMC control */
#define   SDHCI_CDNS_HRS06_TUNE_UP		BIT(15)
#define   SDHCI_CDNS_HRS06_TUNE			GENMASK(13, 8)
#define   SDHCI_CDNS_HRS06_MODE			GENMASK(2, 0)
#define   SDHCI_CDNS_HRS06_MODE_SD		0x0
#define   SDHCI_CDNS_HRS06_MODE_MMC_SDR		0x2
#define   SDHCI_CDNS_HRS06_MODE_MMC_DDR		0x3
#define   SDHCI_CDNS_HRS06_MODE_MMC_HS200	0x4
#define   SDHCI_CDNS_HRS06_MODE_MMC_HS400	0x5
#define   SDHCI_CDNS_HRS06_MODE_MMC_HS400ES	0x6

/* SRS - Slot Register Set (SDHCI-compatible) */
#define SDHCI_CDNS_SRS_BASE		0x200

/* PHY */
#define SDHCI_CDNS_PHY_DLY_SD_HS	0x00
#define SDHCI_CDNS_PHY_DLY_SD_DEFAULT	0x01
#define SDHCI_CDNS_PHY_DLY_UHS_SDR12	0x02
#define SDHCI_CDNS_PHY_DLY_UHS_SDR25	0x03
#define SDHCI_CDNS_PHY_DLY_UHS_SDR50	0x04
#define SDHCI_CDNS_PHY_DLY_UHS_DDR50	0x05
#define SDHCI_CDNS_PHY_DLY_EMMC_LEGACY	0x06
#define SDHCI_CDNS_PHY_DLY_EMMC_SDR	0x07
#define SDHCI_CDNS_PHY_DLY_EMMC_DDR	0x08
#define SDHCI_CDNS_PHY_DLY_SDCLK	0x0b
#define SDHCI_CDNS_PHY_DLY_HSMMC	0x0c
#define SDHCI_CDNS_PHY_DLY_STROBE	0x0d

/*
 * The tuned val register is 6 bit-wide, but not the whole of the range is
 * available.  The range 0-42 seems to be available (then 43 wraps around to 0)
 * but I am not quite sure if it is official.  Use only 0 to 39 for safety.
 */
#define SDHCI_CDNS_MAX_TUNING_LOOP	40

struct sdhci_cdns_plat {
	struct mmc_config cfg;
	struct mmc mmc;
	void __iomem *hrs_addr;
	struct udevice *udev;
	struct phy phy_dev;
	bool phy_enabled;
	struct reset_ctl softreset_ctl;
};

/* socfpga implementation specific driver private data */
struct sdhci_socfpga_priv_data {
	struct sdhci_host host;
	struct phy phy;
};

struct sdhci_cdns_phy_cfg {
	const char *property;
	u8 addr;
};

static const struct sdhci_cdns_phy_cfg sdhci_cdns_phy_cfgs[] = {
	{ "cdns,phy-input-delay-sd-highspeed", SDHCI_CDNS_PHY_DLY_SD_HS, },
	{ "cdns,phy-input-delay-legacy", SDHCI_CDNS_PHY_DLY_SD_DEFAULT, },
	{ "cdns,phy-input-delay-sd-uhs-sdr12", SDHCI_CDNS_PHY_DLY_UHS_SDR12, },
	{ "cdns,phy-input-delay-sd-uhs-sdr25", SDHCI_CDNS_PHY_DLY_UHS_SDR25, },
	{ "cdns,phy-input-delay-sd-uhs-sdr50", SDHCI_CDNS_PHY_DLY_UHS_SDR50, },
	{ "cdns,phy-input-delay-sd-uhs-ddr50", SDHCI_CDNS_PHY_DLY_UHS_DDR50, },
	{ "cdns,phy-input-delay-mmc-highspeed", SDHCI_CDNS_PHY_DLY_EMMC_SDR, },
	{ "cdns,phy-input-delay-mmc-ddr", SDHCI_CDNS_PHY_DLY_EMMC_DDR, },
	{ "cdns,phy-dll-delay-sdclk", SDHCI_CDNS_PHY_DLY_SDCLK, },
	{ "cdns,phy-dll-delay-sdclk-hsmmc", SDHCI_CDNS_PHY_DLY_HSMMC, },
	{ "cdns,phy-dll-delay-strobe", SDHCI_CDNS_PHY_DLY_STROBE, },
};

static int sdhci_cdns_write_phy_reg(struct sdhci_cdns_plat *plat,
				    u8 addr, u8 data)
{
	void __iomem *reg = plat->hrs_addr + SDHCI_CDNS_HRS04;
	u32 tmp;
	int ret;

	if (plat->phy_enabled) {
		/* retrieve reg. addr */
		tmp = FIELD_PREP(SDHCI_CDNS_HRS04_ADDR, addr);

		ret = writel(tmp, reg);
		debug("%s: register = 0x%08x\n", __func__, readl(reg));

		/* read existing value, mask it */
		reg = plat->hrs_addr + SDHCI_CDNS_HRS05;
		tmp = readl(reg);
		debug("%s: register = 0x%08x\n", __func__, readl(reg));

		tmp &= ~data;
		tmp |= data;

		/* write operation */
		ret = writel(tmp, reg);
		debug("%s: register = 0x%08x\n", __func__, readl(reg));
	} else {
		tmp = FIELD_PREP(SDHCI_CDNS_HRS04_WDATA, data) |
			  FIELD_PREP(SDHCI_CDNS_HRS04_ADDR, addr);
		writel(tmp, reg);

		tmp |= SDHCI_CDNS_HRS04_WR;
		writel(tmp, reg);

		ret = readl_poll_timeout(reg, tmp, tmp & SDHCI_CDNS_HRS04_ACK, 10);
		if (ret)
			return ret;

		tmp &= ~SDHCI_CDNS_HRS04_WR;
		writel(tmp, reg);
	}

	return 0;
}

static int sdhci_cdns_phy_init(struct sdhci_cdns_plat *plat,
			       const void *fdt, int nodeoffset)
{
	const fdt32_t *prop;
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(sdhci_cdns_phy_cfgs); i++) {
		prop = fdt_getprop(fdt, nodeoffset,
				   sdhci_cdns_phy_cfgs[i].property, NULL);
		if (!prop)
			continue;

		ret = sdhci_cdns_write_phy_reg(plat,
					       sdhci_cdns_phy_cfgs[i].addr,
					       fdt32_to_cpu(*prop));

		if (ret)
			return ret;
	}

	return 0;
}

static void sdhci_cdns_set_control_reg(struct sdhci_host *host)
{
	struct mmc *mmc = host->mmc;
	struct sdhci_cdns_plat *plat = dev_get_plat(mmc->dev);
	unsigned int clock = mmc->clock;
	u32 mode, tmp;

	/*
	 * REVISIT:
	 * The mode should be decided by MMC_TIMING_* like Linux, but
	 * U-Boot does not support timing.  Use the clock frequency instead.
	 */
	if (clock <= 26000000) {
		mode = SDHCI_CDNS_HRS06_MODE_SD; /* use this for Legacy */
	} else if (clock <= 52000000) {
		if (mmc->ddr_mode)
			mode = SDHCI_CDNS_HRS06_MODE_MMC_DDR;
		else
			mode = SDHCI_CDNS_HRS06_MODE_MMC_SDR;
	} else {
		if (mmc->ddr_mode)
			mode = SDHCI_CDNS_HRS06_MODE_MMC_HS400;
		else
			mode = SDHCI_CDNS_HRS06_MODE_MMC_HS200;
	}

	tmp = readl(plat->hrs_addr + SDHCI_CDNS_HRS06);
	tmp &= ~SDHCI_CDNS_HRS06_MODE;
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS06_MODE, mode);

	writel(tmp, plat->hrs_addr + SDHCI_CDNS_HRS06);
	debug("%s: register = 0x%x\n", __func__,
	      readl(plat->hrs_addr + SDHCI_CDNS_HRS06));

	/* program phy based on generated settings, input through device tree */
	if (plat->phy_enabled)
		generic_phy_configure(&plat->phy_dev, NULL);
}

static const struct sdhci_ops sdhci_cdns_ops = {
	.set_control_reg = sdhci_cdns_set_control_reg,
};

static int sdhci_cdns_set_tune_val(struct sdhci_cdns_plat *plat,
				   unsigned int val)
{
	void __iomem *reg = plat->hrs_addr + SDHCI_CDNS_HRS06;
	u32 tmp;
	int i, ret;

	if (WARN_ON(!FIELD_FIT(SDHCI_CDNS_HRS06_TUNE, val)))
		return -EINVAL;

	tmp = readl(reg);
	tmp &= ~SDHCI_CDNS_HRS06_TUNE;
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS06_TUNE, val);

	/*
	 * Workaround for IP errata:
	 * The IP6116 SD/eMMC PHY design has a timing issue on receive data
	 * path. Send tune request twice.
	 */
	for (i = 0; i < 2; i++) {
		tmp |= SDHCI_CDNS_HRS06_TUNE_UP;
		writel(tmp, reg);

		debug("%s: register = 0x%08x\n", __func__, readl(reg));

		ret = readl_poll_timeout(reg, tmp,
					 !(tmp & SDHCI_CDNS_HRS06_TUNE_UP), 1);
		if (ret)
			return ret;
	}

	return 0;
}

static int __maybe_unused sdhci_cdns_execute_tuning(struct udevice *dev,
						    unsigned int opcode)
{
	struct sdhci_cdns_plat *plat = dev_get_plat(dev);
	struct mmc *mmc = &plat->mmc;
	int cur_streak = 0;
	int max_streak = 0;
	int end_of_streak = 0;
	int i;

	/*
	 * This handler only implements the eMMC tuning that is specific to
	 * this controller.  The tuning for SD timing should be handled by the
	 * SDHCI core.
	 */
	if (!IS_MMC(mmc))
		return -ENOTSUPP;

	if (WARN_ON(opcode != MMC_CMD_SEND_TUNING_BLOCK_HS200))
		return -EINVAL;

	for (i = 0; i < SDHCI_CDNS_MAX_TUNING_LOOP; i++) {
		if (sdhci_cdns_set_tune_val(plat, i) ||
		    mmc_send_tuning(mmc, opcode, NULL)) { /* bad */
			cur_streak = 0;
		} else { /* good */
			cur_streak++;
			if (cur_streak > max_streak) {
				max_streak = cur_streak;
				end_of_streak = i;
			}
		}
	}

	if (!max_streak) {
		dev_err(dev, "no tuning point found\n");
		return -EIO;
	}

	return sdhci_cdns_set_tune_val(plat, end_of_streak - max_streak / 2);
}

static struct dm_mmc_ops sdhci_cdns_mmc_ops;

static int sdhci_cdns_bind(struct udevice *dev)
{
	struct sdhci_cdns_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static int socfpga_sdhci_get_clk_rate(struct udevice *dev)
{
	struct sdhci_socfpga_priv_data *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;

#if (IS_ENABLED(CONFIG_CLK))
	struct clk clk;
	int ret;

	ret = clk_get_by_index(dev, 1, &clk);
	if (ret)
		return ret;

	host->max_clk = clk_get_rate(&clk);

	clk_free(&clk);
#else
	/* Fixed clock divide by 4 which due to the SDMMC wrapper */
	host->max_clk = cm_get_mmc_controller_clk_hz();
#endif

	if (!host->max_clk) {
		debug("SDHCI: MMC clock is zero!");
		return -EINVAL;
	}
	debug("max_clk: %d\n", host->max_clk);

	return 0;
}

static int sdhci_cdns_probe(struct udevice *dev)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sdhci_cdns_plat *plat = dev_get_plat(dev);
	struct sdhci_socfpga_priv_data *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;
	const char *phy_name = dev_read_string(dev, "phy-names");
	fdt_addr_t base;
	int ret;

	plat->phy_enabled = false;

	base = dev_read_addr(dev);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->hrs_addr = devm_ioremap(dev, base, SZ_1K);
	if (!plat->hrs_addr)
		return -ENOMEM;

	if (!phy_name)
		return -EINVAL;

	/* get SDMMC softreset */
	ret = reset_get_by_name(dev, "reset", &plat->softreset_ctl);
	if (ret) {
		pr_err("can't get soft reset for %s (%d)", dev->name, ret);
		return ret;
	}

	/* assert & deassert softreset */
	ret = reset_assert(&plat->softreset_ctl);
	if (ret < 0) {
		pr_err("SDMMC soft reset deassert failed: %d", ret);
		return ret;
	}

	ret = reset_deassert(&plat->softreset_ctl);
	if (ret < 0) {
		pr_err("SDMMC soft reset deassert failed: %d", ret);
		return ret;
	}

	/* probe ComboPHY */
	ret = generic_phy_get_by_name(dev, "combo-phy", &plat->phy_dev);
	if (ret) {
		printf("ComboPHY probe failed: %d\n", ret);
		return ret;
	}
	debug("ComboPHY probe success\n");

	ret = generic_phy_init(&plat->phy_dev);
	if (ret) {
		printf("ComboPHY init failed: %d\n", ret);
		return ret;
	}
	debug("ComboPHY init success\n");

	plat->phy_enabled = true;
	host->name = dev->name;
	host->ioaddr = plat->hrs_addr + SDHCI_CDNS_SRS_BASE;
	host->ops = &sdhci_cdns_ops;
	host->quirks |= SDHCI_QUIRK_WAIT_SEND_CMD;
	sdhci_cdns_mmc_ops = sdhci_ops;
#ifdef MMC_SUPPORTS_TUNING
	sdhci_cdns_mmc_ops.execute_tuning = sdhci_cdns_execute_tuning;
#endif

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		return ret;

	/* get max clk */
	ret = socfpga_sdhci_get_clk_rate(dev);
	if (ret)
		return ret;

	ret = sdhci_cdns_phy_init(plat, gd->fdt_blob, dev_of_offset(dev));
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	upriv->mmc = &plat->mmc;
	host->mmc->priv = host;
	host->mmc->dev = dev;

#if (IS_ENABLED(CONFIG_BLK))
	ret = sdhci_setup_cfg(&plat->cfg, host, host->max_clk, SD_MIN_CLK);
	if (ret)
		return ret;
#else
	ret = add_sdhci(host, host->max_clk, SD_MIN_CLK);
	if (ret)
		return ret;
#endif

	return sdhci_probe(dev);
}

static const struct udevice_id sdhci_cdns_match[] = {
	{ .compatible = "socionext,uniphier-sd4hc" },
	{ .compatible = "cdns,sd4hc" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(sdhci_cdns) = {
	.name = "sdhci-cdns",
	.id = UCLASS_MMC,
	.of_match = sdhci_cdns_match,
	.bind = sdhci_cdns_bind,
	.probe = sdhci_cdns_probe,
	.priv_auto	= sizeof(struct sdhci_socfpga_priv_data),
	.plat_auto	= sizeof(struct sdhci_cdns_plat),
	.ops = &sdhci_cdns_mmc_ops,
};
