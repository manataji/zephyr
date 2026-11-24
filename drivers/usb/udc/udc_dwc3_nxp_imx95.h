/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * NXP i.MX95 quirk module for the Synopsys DWC3 UDC driver (udc_dwc3.c).
 *
 * Register values below come from the i.MX 95 Reference Manual (Rev. 7):
 *   - USB3 controller        : 0x4C100000 (HSIO USB_3_01 USB3)
 *   - USB3 GLUE registers    : 0x4C1F0000 (USB3_CTRL0/1, PHY_CTRL0..6, ...)
 *   - USB3 PHY TCA           : 0x4C1FC000
 *   - HSIOMIX BLK_CTRL       : 0x4C010000 (GPR_REG0: USB_PHY_REF_CLK_SEL)
 *
 * NOTE: UNTESTED on hardware. The PHY power-up sequence below is derived from
 * the RM field descriptions; verify against the Linux/U-Boot i.MX95 USB PHY
 * code (do not copy GPL code verbatim) and adjust delays if needed.
 */

#ifndef ZEPHYR_DRIVERS_USB_UDC_DWC3_NXP_IMX95_H
#define ZEPHYR_DRIVERS_USB_UDC_DWC3_NXP_IMX95_H

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/device_mmio.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define DWC3_CFG(dev)    ((const struct udc_dwc3_config *)(dev)->config)
#define DEV_CFG(dev)     ((const struct udc_dwc3_imx95_config *)(DWC3_CFG(dev)->quirk_config))
#define DEV_DATA(dev)    ((struct udc_dwc3_imx95_data *)(DWC3_CFG(dev)->quirk_data))
#define DEV_BLKCTL(dev)  ((mm_reg_t)DEVICE_MMIO_NAMED_GET(dev, blkctl))
#define DEV_GLUE(dev)    ((mm_reg_t)DEVICE_MMIO_NAMED_GET(dev, glue))
#define DEV_TCA(dev)     ((mm_reg_t)DEVICE_MMIO_NAMED_GET(dev, tca))

/* HSIOMIX BLK_CTRL (0x4C010000) */
#define IMX95_BLKCTL_GPR_REG0                   0x00
#define IMX95_BLKCTL_GPR_REG0_USB_PHY_REF_SEL   BIT(6)  /* 0: 24 MHz XTAL, 1: 100 MHz HSIO PLL */

/* USB3 GLUE (0x4C1F0000) */
#define IMX95_GLUE_PHY_CTRL0                    0x40
#define IMX95_PHY_CTRL0_REF_USE_PAD             BIT(0)
#define IMX95_PHY_CTRL0_REF_CLKDIV2             BIT(1)
#define IMX95_PHY_CTRL0_REF_SSP_EN              BIT(2)
#define IMX95_PHY_CTRL0_REFCLKSEL_MASK          GENMASK(4, 3)   /* reserved: set to 0b10 */
#define IMX95_PHY_CTRL0_FSEL_MASK               GENMASK(10, 5)
#define IMX95_PHY_CTRL0_FSEL_100MHZ             0x27            /* 10_0111b */
#define IMX95_PHY_CTRL0_FSEL_24MHZ              0x2A            /* 10_1010b */
#define IMX95_PHY_CTRL0_SSC_EN                  BIT(11)

#define IMX95_GLUE_PHY_CTRL1                    0x44
#define IMX95_PHY_CTRL1_PHY_RESET               BIT(0)
#define IMX95_PHY_CTRL1_VDATSRCENB0             BIT(19) /* battery charging VDAT_SRC (reset: 1) */
#define IMX95_PHY_CTRL1_VDATDETENB0             BIT(20) /* battery charging CHG_DET  (reset: 1) */
#define IMX95_PHY_CTRL1_SYS_VBUSVALID_SW        BIT(28)
#define IMX95_PHY_CTRL1_USB_DEV_POR             BIT(29)

#define IMX95_GLUE_PHY_CTRL2                    0x48
#define IMX95_PHY_CTRL2_SYS_VBUSVALID_SEL_MASK  GENMASK(22, 21)
#define IMX95_PHY_CTRL2_SYS_VBUSVALID_SEL_SW    3   /* use PHY_CTRL1[28] */

#define IMX95_GLUE_PHY_CTRL6                    0x58
#define IMX95_PHY_CTRL6_ALT_CLK_SEL             BIT(0)   /* reset 0 (HW may leave 1) */
#define IMX95_PHY_CTRL6_ALT_CLK_EN              BIT(1)   /* reset 1 */
#define IMX95_PHY_CTRL6_RXTERM_OVERRIDE_SEL     BIT(29)  /* 1: pipe3 RxTermination from register */
#define IMX95_PHY_CTRL6_RXTERM_OVERRIDE         BIT(28)  /* 0: inactive */

/* USB3 PHY TCA (0x4C1FC000) */
#define IMX95_TCA_GCFG                          0x10
#define IMX95_TCA_GCFG_ROLE_HSTDEV              BIT(4)  /* 1: device mode */
#define IMX95_TCA_GCFG_OP_MODE_MASK             GENMASK(1, 0)   /* x0: system config, x1: ctrl synced */
#define IMX95_TCA_TCPC                          0x14
#define IMX95_TCA_SYSMODE_CFG                   0x18
#define IMX95_TCA_SYSMODE_TYPEC_FLIP            BIT(2)
#define IMX95_TCA_SYSMODE_TYPEC_DISABLE         BIT(3)
#define IMX95_TCA_TCPC_VALID                    BIT(4)  /* request is valid (auto-cleared) */
#define IMX95_TCA_TCPC_LOW_POWER_EN             BIT(3)  /* reset value 1: PHY lanes disabled */
#define IMX95_TCA_TCPC_ORIENT_FLIP              BIT(2)
#define IMX95_TCA_TCPC_MUX_MASK                 GENMASK(1, 0)
#define IMX95_TCA_TCPC_MUX_USB3                 1       /* 00: no connection (reset), 01: USB3 */
#define IMX95_TCA_PSTATE                        0x30
#define IMX95_TCA_GEN_STATUS                    0x34
#define IMX95_GLUE_PHY_STATUS0                  0x80

struct udc_dwc3_imx95_config {
	DEVICE_MMIO_NAMED_ROM(blkctl);
	DEVICE_MMIO_NAMED_ROM(glue);
	DEVICE_MMIO_NAMED_ROM(tca);

	const struct device *clk_hsio_dev;
	clock_control_subsys_t clk_hsio_id;
	const struct device *clk_ref_dev;
	clock_control_subsys_t clk_ref_id;
	const struct device *clk_suspend_dev;
	clock_control_subsys_t clk_suspend_id;
	bool force_vbus_valid;
	bool typec_flipped;
};

struct udc_dwc3_imx95_data {
	DEVICE_MMIO_NAMED_RAM(blkctl);
	DEVICE_MMIO_NAMED_RAM(glue);
	DEVICE_MMIO_NAMED_RAM(tca);
	int clk_rv[3]; /* hsio, ref, suspend: result of clock_control_on() */
};

static inline void imx95_rmw(mm_reg_t addr, uint32_t clr, uint32_t set)
{
	uint32_t v = sys_read32(addr);

	v &= ~clr;
	v |= set;
	sys_write32(v, addr);
}

static int udc_dwc3_imx95_clk_on(const struct device *ctlr, clock_control_subsys_t id)
{
	int rv;

	/* "clocks" is optional in devicetree */
	if (ctlr == NULL) {
		return 0;
	}

	if (!device_is_ready(ctlr)) {
		LOG_ERR("clock controller not ready");
		return -ENODEV;
	}

	rv = clock_control_on(ctlr, id);
	if (rv == -EALREADY) {
		return 0;
	}
	if (rv != 0) {
		LOG_ERR("clock_control_on failed: %d", rv);
	}

	return rv;
}

static int udc_dwc3_imx95_preinit(const struct device *dev)
{
	const struct udc_dwc3_imx95_config *cfg = DEV_CFG(dev);
	mm_reg_t blkctl, glue, tca;
	uint32_t fsel;

	printk("imx95-usb: preinit start\n");

	/*
	 * 1. Clocks via SCMI.
	 * Bring-up: a failing clock enable is reported but not fatal, so that
	 * we can see which one fails (the SM may refuse to gate some clocks).
	 */
	/*
	 * Linux imx95.dtsi gives the DWC3 core three clocks: "bus_early" (HSIO),
	 * "ref" (24 MHz) and "suspend" (32 kHz). Enable all three and remember
	 * the results (logged from the init hook, where logging is available).
	 */
	DEV_DATA(dev)->clk_rv[0] = udc_dwc3_imx95_clk_on(cfg->clk_hsio_dev, cfg->clk_hsio_id);
	DEV_DATA(dev)->clk_rv[1] = udc_dwc3_imx95_clk_on(cfg->clk_ref_dev, cfg->clk_ref_id);
	DEV_DATA(dev)->clk_rv[2] = udc_dwc3_imx95_clk_on(cfg->clk_suspend_dev, cfg->clk_suspend_id);
	printk("imx95-usb: clock on rv hsio=%d ref=%d suspend=%d\n", DEV_DATA(dev)->clk_rv[0],
	       DEV_DATA(dev)->clk_rv[1], DEV_DATA(dev)->clk_rv[2]);

	/* 2. Map register blocks */
	DEVICE_MMIO_NAMED_MAP(dev, blkctl, K_MEM_CACHE_NONE);
	DEVICE_MMIO_NAMED_MAP(dev, glue, K_MEM_CACHE_NONE);
	DEVICE_MMIO_NAMED_MAP(dev, tca, K_MEM_CACHE_NONE);

	blkctl = DEV_BLKCTL(dev);
	glue = DEV_GLUE(dev);
	tca = DEV_TCA(dev);
	printk("imx95-usb: mapped blkctl=0x%lx glue=0x%lx tca=0x%lx\n", (unsigned long)blkctl,
	       (unsigned long)glue, (unsigned long)tca);

	printk("imx95-usb: before GPR_REG0=0x%08x PHY_CTRL0=0x%08x CTRL1=0x%08x CTRL2=0x%08x\n",
	       sys_read32(blkctl + IMX95_BLKCTL_GPR_REG0), sys_read32(glue + IMX95_GLUE_PHY_CTRL0),
	       sys_read32(glue + IMX95_GLUE_PHY_CTRL1), sys_read32(glue + IMX95_GLUE_PHY_CTRL2));

	/*
	 * 3. PHY power-up.
	 * The ref clock selected in GPR_REG0 must match PHY_CTRL0.FSEL.
	 * (RM reset values: GPR_REG0.USB_PHY_REF_CLK_SEL = 0 -> 24 MHz, but
	 *  PHY_CTRL0.FSEL = 100 MHz, so they must be aligned explicitly.)
	 * 100 MHz via HSIO PLL is the RM-recommended option, but then the HSIO PLL
	 * must be running with SSC disabled; here we simply follow whatever
	 * GPR_REG0 currently selects.
	 */
	fsel = (sys_read32(blkctl + IMX95_BLKCTL_GPR_REG0) &
		IMX95_BLKCTL_GPR_REG0_USB_PHY_REF_SEL)
		       ? IMX95_PHY_CTRL0_FSEL_100MHZ
		       : IMX95_PHY_CTRL0_FSEL_24MHZ;

	/* Hold the PHY in reset while configuring; REF_SSP_EN must stay low until clock is up */
	/*
	 * Also disable the battery-charging detect/source circuits (reset value 1):
	 * U-Boot leaves PHY_CTRL1 = 0x00800006 (both cleared) in its working state.
	 */
	imx95_rmw(glue + IMX95_GLUE_PHY_CTRL1,
		  IMX95_PHY_CTRL1_VDATSRCENB0 | IMX95_PHY_CTRL1_VDATDETENB0,
		  IMX95_PHY_CTRL1_PHY_RESET);
	imx95_rmw(glue + IMX95_GLUE_PHY_CTRL0,
		  IMX95_PHY_CTRL0_FSEL_MASK | IMX95_PHY_CTRL0_REFCLKSEL_MASK |
			  IMX95_PHY_CTRL0_REF_USE_PAD | IMX95_PHY_CTRL0_REF_CLKDIV2 |
			  IMX95_PHY_CTRL0_REF_SSP_EN,
		  FIELD_PREP(IMX95_PHY_CTRL0_FSEL_MASK, fsel) |
			  FIELD_PREP(IMX95_PHY_CTRL0_REFCLKSEL_MASK, 0x2) |
			  IMX95_PHY_CTRL0_SSC_EN);

	/*
	 * PHY_CTRL6: U-Boot's working state is 0x20000000 (reset/boot value 0x3):
	 * ALT_CLK_SEL/ALT_CLK_EN cleared and the pipe3 RxTermination override
	 * selected (value 0 = inactive, i.e. SuperSpeed RX termination off).
	 */
	imx95_rmw(glue + IMX95_GLUE_PHY_CTRL6,
		  IMX95_PHY_CTRL6_ALT_CLK_SEL | IMX95_PHY_CTRL6_ALT_CLK_EN |
			  IMX95_PHY_CTRL6_RXTERM_OVERRIDE,
		  IMX95_PHY_CTRL6_RXTERM_OVERRIDE_SEL);

	/* Device mode: PoR as USB device */
	imx95_rmw(glue + IMX95_GLUE_PHY_CTRL1, 0, IMX95_PHY_CTRL1_USB_DEV_POR);

	/* Enable the reference clock to the SS prescaler, then release reset */
	imx95_rmw(glue + IMX95_GLUE_PHY_CTRL0, 0, IMX95_PHY_CTRL0_REF_SSP_EN);
	k_busy_wait(100); /* TODO: confirm required delay */
	imx95_rmw(glue + IMX95_GLUE_PHY_CTRL1, IMX95_PHY_CTRL1_PHY_RESET, 0);
	k_busy_wait(100); /* TODO: confirm required delay / PHY power-up status */

	/*
	 * 4. Type-C assist.
	 * U-Boot's working state has TCA_GCFG = 0 (OP_MODE = system configuration
	 * mode, TCA_SYSMODE_CFG used for direct TypeC_MUX control), while the reset
	 * value (1) is "controller synced" mode, where the XBar assist waits for
	 * handshakes from the controller. Use system configuration mode.
	 */
	sys_write32(0, tca + IMX95_TCA_GCFG);
	imx95_rmw(tca + IMX95_TCA_SYSMODE_CFG,
		  IMX95_TCA_SYSMODE_TYPEC_DISABLE | IMX95_TCA_SYSMODE_TYPEC_FLIP,
		  cfg->typec_flipped ? IMX95_TCA_SYSMODE_TYPEC_FLIP : 0);

	/*
	 * 5. Optionally force VBUS valid (after the Type-C mux has been configured,
	 * see the PHY_CTRL1[28] description in the RM).
	 */
	if (cfg->force_vbus_valid) {
		/* Bring-up aid: treat VBUS as always valid (enables D+ pull-up) */
		imx95_rmw(glue + IMX95_GLUE_PHY_CTRL2, IMX95_PHY_CTRL2_SYS_VBUSVALID_SEL_MASK,
			  FIELD_PREP(IMX95_PHY_CTRL2_SYS_VBUSVALID_SEL_MASK,
				     IMX95_PHY_CTRL2_SYS_VBUSVALID_SEL_SW));
		imx95_rmw(glue + IMX95_GLUE_PHY_CTRL1, 0, IMX95_PHY_CTRL1_SYS_VBUSVALID_SW);
	}


	printk("imx95-usb: after PHY_CTRL0=0x%08x CTRL1=0x%08x CTRL2=0x%08x TCA_GCFG=0x%08x\n",
	       sys_read32(glue + IMX95_GLUE_PHY_CTRL0), sys_read32(glue + IMX95_GLUE_PHY_CTRL1),
	       sys_read32(glue + IMX95_GLUE_PHY_CTRL2), sys_read32(tca + IMX95_TCA_GCFG));
	printk("imx95-usb: PHY_CTRL6=0x%08x\n", sys_read32(glue + IMX95_GLUE_PHY_CTRL6));
	printk("imx95-usb: PHY_STATUS0=0x%08x TCA_SYSMODE=0x%08x TCA_PSTATE=0x%08x TCA_GEN=0x%08x\n",
	       sys_read32(glue + IMX95_GLUE_PHY_STATUS0), sys_read32(tca + IMX95_TCA_SYSMODE_CFG),
	       sys_read32(tca + IMX95_TCA_PSTATE), sys_read32(tca + IMX95_TCA_GEN_STATUS));

	return 0;
}

/* The init hook runs when logging is available: report what preinit did. */
static int udc_dwc3_imx95_init(const struct device *dev)
{
	mm_reg_t blkctl = DEV_BLKCTL(dev);
	mm_reg_t glue = DEV_GLUE(dev);
	mm_reg_t tca = DEV_TCA(dev);

	LOG_INF("imx95-usb: clock on rv hsio=%d ref=%d suspend=%d", DEV_DATA(dev)->clk_rv[0],
		DEV_DATA(dev)->clk_rv[1], DEV_DATA(dev)->clk_rv[2]);
	LOG_INF("imx95-usb: GPR_REG0=0x%08x PHY_CTRL0=0x%08x CTRL1=0x%08x CTRL2=0x%08x",
		sys_read32(blkctl + IMX95_BLKCTL_GPR_REG0), sys_read32(glue + IMX95_GLUE_PHY_CTRL0),
		sys_read32(glue + IMX95_GLUE_PHY_CTRL1), sys_read32(glue + IMX95_GLUE_PHY_CTRL2));
	LOG_INF("imx95-usb: PHY_CTRL6=0x%08x", sys_read32(glue + IMX95_GLUE_PHY_CTRL6));
	LOG_INF("imx95-usb: PHY_STATUS0=0x%08x TCA_GCFG=0x%08x SYSMODE=0x%08x PSTATE=0x%08x",
		sys_read32(glue + IMX95_GLUE_PHY_STATUS0), sys_read32(tca + IMX95_TCA_GCFG),
		sys_read32(tca + IMX95_TCA_SYSMODE_CFG), sys_read32(tca + IMX95_TCA_PSTATE));

	return 0;
}

const struct udc_dwc3_vendor_quirks udc_dwc3_vendor_quirks = {
	.preinit = udc_dwc3_imx95_preinit,
	.init = udc_dwc3_imx95_init,
};

/*
 * The "clocks" property is optional. When present, the clocks named "hsio",
 * "ref" and "suspend" are enabled via SCMI (the SM may refuse to gate them for
 * this agent; that is not fatal because they are already running).
 */
#define IMX95_CLK_DEV(n, cname)                                                                    \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, clocks),                                              \
		    (DEVICE_DT_GET(DT_INST_CLOCKS_CTLR_BY_NAME(n, cname))), (NULL))
#define IMX95_CLK_ID(n, cname)                                                                     \
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, clocks),                                              \
		    ((clock_control_subsys_t)DT_INST_PHA_BY_NAME(n, clocks, cname, name)), (NULL))

#define UDC_DWC3_QUIRK_DEFINE(n)                                                                   \
	static const struct udc_dwc3_imx95_config udc_dwc3_quirk_config_##n = {                    \
		DEVICE_MMIO_NAMED_ROM_INIT_BY_NAME(blkctl, DT_DRV_INST(n)),                        \
		DEVICE_MMIO_NAMED_ROM_INIT_BY_NAME(glue, DT_DRV_INST(n)),                          \
		DEVICE_MMIO_NAMED_ROM_INIT_BY_NAME(tca, DT_DRV_INST(n)),                           \
		.clk_hsio_dev = IMX95_CLK_DEV(n, hsio),                                            \
		.clk_hsio_id = IMX95_CLK_ID(n, hsio),                                              \
		.clk_ref_dev = IMX95_CLK_DEV(n, ref),                                              \
		.clk_ref_id = IMX95_CLK_ID(n, ref),                                                \
		.clk_suspend_dev = IMX95_CLK_DEV(n, suspend),                                      \
		.clk_suspend_id = IMX95_CLK_ID(n, suspend),                                        \
		.force_vbus_valid = DT_INST_PROP(n, nxp_force_vbus_valid),                         \
		.typec_flipped = DT_INST_PROP(n, nxp_typec_flipped),                               \
	};                                                                                         \
	static struct udc_dwc3_imx95_data udc_dwc3_quirk_data_##n;

#undef DEV_TCA
#undef DEV_GLUE
#undef DEV_BLKCTL
#undef DEV_DATA
#undef DEV_CFG
#undef DWC3_CFG

#endif /* ZEPHYR_DRIVERS_USB_UDC_DWC3_NXP_IMX95_H */
