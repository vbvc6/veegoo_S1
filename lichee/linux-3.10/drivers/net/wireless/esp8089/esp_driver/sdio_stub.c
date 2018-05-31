/* Copyright (c) 2008 -2014 Espressif System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *  sdio stub code for allwinner
 */
extern int wifi_pm_gpio_ctrl(char* name, int level);
extern void sunxi_wlan_set_power(bool on_off);
extern void sunxi_mmc_rescan_card(unsigned id);
extern int sunxi_wlan_get_bus_index(void);


void sif_platform_rescan_card(unsigned insert)
{
    unsigned wlan_bus_index = sunxi_wlan_get_bus_index(); 
	printk("%s id %d %u\n", __func__, wlan_bus_index, insert);
    sunxi_mmc_rescan_card(wlan_bus_index);
}
void sif_platform_reset_target(void)
{
#if 0
        wifi_pm_gpio_ctrl("esp_wl_chip_en", 0);
        mdelay(100);
        wifi_pm_gpio_ctrl("esp_wl_chip_en", 1);
	
	mdelay(100);
#endif
	sunxi_wlan_set_power(0);
	mdelay(100);
	sunxi_wlan_set_power(1);
	mdelay(100);
}

void sif_platform_target_poweroff(void)
{
#if 0
        wifi_pm_gpio_ctrl("esp_wl_rst", 0);
	mdelay(100);
        wifi_pm_gpio_ctrl("esp_wl_chip_en", 0);
	mdelay(100);
	
	//wifi_pm_gpio_ctrl("esp_wl_pw",0);
	//mdelay(10);
#endif
	sunxi_wlan_set_power(0);
	mdelay(100);
}

void sif_platform_target_poweron(void)
{
#if 0
//	int num = 3;
//	while(num--)
//	{
//		if(!wifi_pm_gpio_ctrl("esp_wl_pw",1))
//			break;
//		mdelay(10);
//	}
//	mdelay(120);
	wifi_pm_gpio_ctrl("esp_wl_chip_en",1);
	mdelay(100);

        wifi_pm_gpio_ctrl("esp_wl_rst",1);
	mdelay(100);
#endif
	sunxi_wlan_set_power(0);
	mdelay(100);
	sunxi_wlan_set_power(1);
	mdelay(100);
        }

void sif_platform_target_speed(int high_speed)
{
#if 0

        wifi_pm_gpio_ctrl("esp_wl_rst", high_speed);
#endif
}

#ifdef MMC_NO_CHANGE
//extern int sw_mci_check_r1_ready(struct mmc_host* mmc, unsigned ms);//jacky remove
extern int sunxi_mmc_check_r1_ready(struct mmc_host* mmc, unsigned ms);
void sif_platform_check_r1_ready(struct esp_pub *epub)
{
        struct esp_sdio_ctrl *sctrl = NULL;
        struct sdio_func *func = NULL;
	int err;

	if (epub == NULL) {
        	ESSERT(epub != NULL);
		return;
	}
        sctrl = (struct esp_sdio_ctrl *)epub->sif;
        func = sctrl->func;
	if (func == NULL) {
        	ESSERT(func != NULL);
		return;
	}
	err = sunxi_mmc_check_r1_ready(func->card->host, 1000);
        if (err != 0)
          printk("%s data timeout.\n", __func__);
}
#else
void sif_platform_check_r1_ready(struct esp_pub *epub)
{
}
#endif

#ifdef ESP_ACK_INTERRUPT
extern void sdmmc_ack_interrupt(struct mmc_host *mmc);
void sif_platform_ack_interrupt(struct esp_pub *epub)
{
        struct esp_sdio_ctrl *sctrl = NULL;
        struct sdio_func *func = NULL;
	
	if (epub == NULL) {
        	ESSERT(epub != NULL);
		return;
	}
        sctrl = (struct esp_sdio_ctrl *)epub->sif;
        func = sctrl->func;
	if (func == NULL) {
        	ESSERT(func != NULL);
		return;
	}

        sdmmc_ack_interrupt(func->card->host);
}
#endif //ESP_ACK_INTERRUPT

