/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <version.h>
#include <image.h>
#include <asm/arch/reset_manager.h>
#include <spl.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/freeze_controller.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_RAM;
}

/*
 * Board initialization after bss clearance
 */
void spl_board_init(void)
{
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
	cm_config_t cm_default_cfg = {
		/* main group */
		MAIN_VCO_BASE,
		CLKMGR_MAINPLLGRP_MPUCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MPUCLK_CNT),
		CLKMGR_MAINPLLGRP_MAINCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MAINCLK_CNT),
		CLKMGR_MAINPLLGRP_DBGATCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_DBGATCLK_CNT),
		CLKMGR_MAINPLLGRP_MAINQSPICLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MAINQSPICLK_CNT),
		CLKMGR_PERPLLGRP_PERNANDSDMMCCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MAINNANDSDMMCCLK_CNT),
		CLKMGR_MAINPLLGRP_CFGS2FUSER0CLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_CFGS2FUSER0CLK_CNT),
		CLKMGR_MAINPLLGRP_MAINDIV_L3MPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L3MPCLK) |
		CLKMGR_MAINPLLGRP_MAINDIV_L3SPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L3SPCLK) |
		CLKMGR_MAINPLLGRP_MAINDIV_L4MPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L4MPCLK) |
		CLKMGR_MAINPLLGRP_MAINDIV_L4SPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L4SPCLK),
		CLKMGR_MAINPLLGRP_DBGDIV_DBGATCLK_SET(
			CONFIG_HPS_MAINPLLGRP_DBGDIV_DBGATCLK) |
		CLKMGR_MAINPLLGRP_DBGDIV_DBGCLK_SET(
			CONFIG_HPS_MAINPLLGRP_DBGDIV_DBGCLK),
		CLKMGR_MAINPLLGRP_TRACEDIV_TRACECLK_SET(
			CONFIG_HPS_MAINPLLGRP_TRACEDIV_TRACECLK),
		CLKMGR_MAINPLLGRP_L4SRC_L4MP_SET(
			CONFIG_HPS_MAINPLLGRP_L4SRC_L4MP) |
		CLKMGR_MAINPLLGRP_L4SRC_L4SP_SET(
			CONFIG_HPS_MAINPLLGRP_L4SRC_L4SP),

		/* peripheral group */
		PERI_VCO_BASE,
		CLKMGR_PERPLLGRP_EMAC0CLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_EMAC0CLK_CNT),
		CLKMGR_PERPLLGRP_EMAC1CLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_EMAC1CLK_CNT),
		CLKMGR_PERPLLGRP_PERQSPICLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_PERQSPICLK_CNT),
		CLKMGR_PERPLLGRP_PERNANDSDMMCCLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_PERNANDSDMMCCLK_CNT),
		CLKMGR_PERPLLGRP_PERBASECLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_PERBASECLK_CNT),
		CLKMGR_PERPLLGRP_S2FUSER1CLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_S2FUSER1CLK_CNT),
		CLKMGR_PERPLLGRP_DIV_USBCLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_USBCLK) |
		CLKMGR_PERPLLGRP_DIV_SPIMCLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_SPIMCLK) |
		CLKMGR_PERPLLGRP_DIV_CAN0CLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_CAN0CLK) |
		CLKMGR_PERPLLGRP_DIV_CAN1CLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_CAN1CLK),
		CLKMGR_PERPLLGRP_GPIODIV_GPIODBCLK_SET(
			CONFIG_HPS_PERPLLGRP_GPIODIV_GPIODBCLK),
		CLKMGR_PERPLLGRP_SRC_QSPI_SET(
			CONFIG_HPS_PERPLLGRP_SRC_QSPI) |
		CLKMGR_PERPLLGRP_SRC_NAND_SET(
			CONFIG_HPS_PERPLLGRP_SRC_NAND) |
		CLKMGR_PERPLLGRP_SRC_SDMMC_SET(
			CONFIG_HPS_PERPLLGRP_SRC_SDMMC),

		/* sdram pll group */
		SDR_VCO_BASE,
		CLKMGR_SDRPLLGRP_DDRDQSCLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQSCLK_PHASE) |
		CLKMGR_SDRPLLGRP_DDRDQSCLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQSCLK_CNT),
		CLKMGR_SDRPLLGRP_DDR2XDQSCLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_DDR2XDQSCLK_PHASE) |
		CLKMGR_SDRPLLGRP_DDR2XDQSCLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_DDR2XDQSCLK_CNT),
		CLKMGR_SDRPLLGRP_DDRDQCLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQCLK_PHASE) |
		CLKMGR_SDRPLLGRP_DDRDQCLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQCLK_CNT),
		CLKMGR_SDRPLLGRP_S2FUSER2CLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_S2FUSER2CLK_PHASE) |
		CLKMGR_SDRPLLGRP_S2FUSER2CLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_S2FUSER2CLK_CNT),
	};

	debug("Freezing all I/O banks\n");
	/* freeze all IO banks */
	sys_mgr_frzctrl_freeze_req();

	debug("Reconfigure Clock Manager\n");
	/* reconfigure the PLLs */
	cm_basic_init(&cm_default_cfg);

	/* configure the IOCSR / IO buffer settings */
	if (scan_mgr_configure_iocsr())
		hang();

	/* configure the pin muxing through system manager */
	sysmgr_pinmux_init();
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */

	/* de-assert reset for peripherals and bridges based on handoff */
	reset_deassert_peripherals_handoff();

	debug("Unfreezing/Thaw all I/O banks\n");
	/* unfreeze / thaw all IO banks */
	sys_mgr_frzctrl_thaw_req();

	/* enable console uart printing */
	preloader_console_init();
}
