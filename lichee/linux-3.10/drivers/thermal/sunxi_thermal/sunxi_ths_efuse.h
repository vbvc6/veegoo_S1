/*
 * drivers/thermal/sunxi_thermal/sunxi_ths_efuse.h
 *
 * Copyright (C) 2013-2024 allwinner.
 *	JiaRui Xiao<xiaojiarui@allwinnertech.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __SUNXI_THS_EFUSE_H__
#define __SUNXI_THS_EFUSE_H__

/* GET TMEP RETURN VALUE */
#define WRONG_EFUSE_REG_DATA		(0xffff)
#define THS_EFUSE_DEFAULT_VALUE		(0x800)
#define THS_EFUSE_ENVIROMENT_MASK	(0x0fff)
#define THS_EFUSE_CP_FT_MASK		(0b0011000000000000)
#define THS_EFUSE_CP_FT_BIT		(12)/* depend on ths calibration doc */
#define THS_CALIBRATION_IN_FT		(1)



#if defined(CONFIG_ARCH_SUN50IW3)
#define SENSOR_CP_EUFSE_PER_REG_TO_TEMP (672)/* this value is 0.0672 */
#define CONST_MUL			(10)
#define CONST_DIV			(10000)/* protect one decimal so add one zero*/
#define FT_CALIBRATION_DEVIATION	(0)/* degrees celsius */
#elif defined(CONFIG_ARCH_SUN50IW6)
#define SENSOR_CP_EUFSE_PER_REG_TO_TEMP (672)/* this value is 0.0672 */
#define CONST_MUL			(10)
#define CONST_DIV			(10000)/* protect one decimal so add one zero*/
#define FT_CALIBRATION_DEVIATION	(7)/* degrees celsius */
#else
/* doesn't exist these Macro */
#define SENSOR_CP_EUFSE_PER_REG_TO_TEMP (-1UL)
#define CONST_MUL			(1)
#define CONST_DIV			(1*CONST_MUL)
#define FT_CALIBRATION_DEVIATION	(0)/* degrees celsius */
#endif

#endif/* __SUNXI_THS_EFUSE_H__ */

