/*
 * drivers/power/axp/axp803/axp803-gpio.h
 * (C) Copyright 2010-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Pannan <pannan@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef AXP803_GPIO_H
#define AXP803_GPIO_H
#include "axp803.h"

#define AXP_GPIO_IRQ_EDGE_RISING   (0x1<<7)
#define AXP_GPIO_IRQ_EDGE_FALLING  (0x1<<6)
#define AXP_GPIO_INPUT_TRIG_MASK   (0x7<<0)
#define AXP_GPIO_EDGE_TRIG_MASK    (AXP_GPIO_IRQ_EDGE_RISING | \
				AXP_GPIO_IRQ_EDGE_FALLING)

#define AXP_GPIO0_CFG              (AXP803_GPIO0_CTL)     /* 0x90 */
#define AXP_GPIO1_CFG              (AXP803_GPIO1_CTL)     /* 0x92 */
#define AXP_GPIO01_STATE           (AXP803_GPIO01_SIGNAL) /* 0x94 */
#define AXP_GPIO01_INTEN           (AXP803_INTEN5)        /* 0x44 */
#define AXP_GPIO01_INTSTA          (AXP803_INTSTS5)       /* 0x4C */

#endif /* AXP803_GPIO_H */
