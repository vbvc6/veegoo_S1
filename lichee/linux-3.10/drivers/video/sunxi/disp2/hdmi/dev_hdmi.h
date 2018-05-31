/*
 *
 * Copyright (c) 2016 Allwinnertech Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef __DEV_DISPLAY_H__
#define __DEV_DISPLAY_H__

#include "drv_hdmi_i.h"

int hdmi_open(struct inode *inode, struct file *file);
int hdmi_release(struct inode *inode, struct file *file);
ssize_t hdmi_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
ssize_t hdmi_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
int hdmi_mmap(struct file *file, struct vm_area_struct * vma);
long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

extern s32 hdmi_init(struct platform_device *pdev);
extern s32 hdmi_exit(void);
extern s32 Fb_Init(u32 from);
s32 hdmi_hpd_state(u32 state);
s32 hdmi_hpd_event(void);

typedef struct
{
	struct device           *dev;
	bool                    bopen;
	enum disp_tv_mode            mode;//vic
	u32                   base_hdmi;
	struct work_struct      hpd_work;
}hdmi_info_t;

extern hdmi_info_t ghdmi;
extern struct disp_video_timings video_timing[];

#endif
