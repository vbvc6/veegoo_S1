/*
 * Copyright (C) 2012 Invensense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>

#include "inv_mpu_iio.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20608D"},
	{128, "ICM20690"},
	{128, "ICM20602"},
};

static const int ois_gyro_fs[] = {250, 500, 1000, 2000, -1, 31, 62, 125};
static const int ois_accel_fs[] = {2, 4, 8, 1};

static int debug_mem_read_addr = 0x900;
static char debug_reg_addr = 0x6;

const char sensor_l_info[][30] = {
	"SENSOR_L_ACCEL",
	"SENSOR_L_GYRO",
	"SENSOR_L_MAG",
	"SENSOR_L_ALS",
	"SENSOR_L_SIXQ",
	"SENSOR_L_NINEQ",
	"SENSOR_L_PEDQ",
	"SENSOR_L_GEOMAG",
	"SENSOR_L_PRESSURE",
	"SENSOR_L_GYRO_CAL",
	"SENSOR_L_MAG_CAL",
	"SENSOR_L_EIS_GYRO",
	"SENSOR_L_ACCEL_WAKE",
	"SENSOR_L_GYRO_WAKE",
	"SENSOR_L_MAG_WAKE",
	"SENSOR_L_ALS_WAKE",
	"SENSOR_L_SIXQ_WAKE",
	"SENSOR_L_NINEQ_WAKE",
	"SENSOR_L_PEDQ_WAKE",
	"SENSOR_L_GEOMAG_WAKE",
	"SENSOR_L_PRESSURE_WAKE",
	"SENSOR_L_GYRO_CAL_WAKE",
	"SENSOR_L_MAG_CAL_WAKE",
	"SENSOR_L_NUM_MAX",
};

static int inv_set_accel_bias_reg(struct inv_mpu_state *st,
				  int accel_bias, int axis)
{
	int accel_reg_bias;
	u8 addr;
	u8 d[2];
	int result = 0;

	switch (axis) {
	case 0:
		/* X */
		addr = REG_XA_OFFS_H;
		break;
	case 1:
		/* Y */
		addr = REG_YA_OFFS_H;
		break;
	case 2:
		/* Z* */
		addr = REG_ZA_OFFS_H;
		break;
	default:
		result = -EINVAL;
		goto accel_bias_set_err;
	}

	result = inv_plat_read(st, addr, 2, d);
	if (result)
		goto accel_bias_set_err;
	accel_reg_bias = ((int)d[0] << 8) | d[1];

	/* accel_bias is 2g scaled by 1<<16.
	 * Convert to 16g, and mask bit0 */
	accel_reg_bias -= ((accel_bias / 8 / 65536) & ~1);

	d[0] = (accel_reg_bias >> 8) & 0xff;
	d[1] = (accel_reg_bias) & 0xff;
	result = inv_plat_single_write(st, addr, d[0]);
	if (result)
		goto accel_bias_set_err;
	result = inv_plat_single_write(st, addr + 1, d[1]);
	if (result)
		goto accel_bias_set_err;

accel_bias_set_err:
	return result;
}

static int inv_set_gyro_bias_reg(struct inv_mpu_state *st,
				 const int gyro_bias, int axis)
{
	int gyro_reg_bias;
	u8 addr;
	u8 d[2];
	int result = 0;

	switch (axis) {
	case 0:
		/* X */
		addr = REG_XG_OFFS_USR_H;
		break;
	case 1:
		/* Y */
		addr = REG_YG_OFFS_USR_H;
		break;
	case 2:
		/* Z */
		addr = REG_ZG_OFFS_USR_H;
		break;
	default:
		result = -EINVAL;
		goto gyro_bias_set_err;
	}

	/* gyro_bias is 2000dps scaled by 1<<16.
	 * Convert to 1000dps */
	gyro_reg_bias = (-gyro_bias * 2 / 65536);

	d[0] = (gyro_reg_bias >> 8) & 0xff;
	d[1] = (gyro_reg_bias) & 0xff;
	result = inv_plat_single_write(st, addr, d[0]);
	if (result)
		goto gyro_bias_set_err;
	result = inv_plat_single_write(st, addr + 1, d[1]);
	if (result)
		goto gyro_bias_set_err;

gyro_bias_set_err:
	return result;
}

static int _bias_store(struct device *dev,
		       struct device_attribute *attr, const char *buf,
		       size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto bias_store_fail;
	switch (this_attr->address) {
	case ATTR_ACCEL_X_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[0] = data;
		break;
	case ATTR_ACCEL_Y_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[1] = data;
		break;
	case ATTR_ACCEL_Z_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[2] = data;
		break;
	case ATTR_GYRO_X_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[0] = data;
		break;
	case ATTR_GYRO_Y_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[1] = data;
		break;
	case ATTR_GYRO_Z_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[2] = data;
		break;
	default:
		break;
	}

bias_store_fail:
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

static ssize_t inv_bias_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_debug_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	return count;
	switch (this_attr->address) {
	case ATTR_DMP_LP_EN_OFF:
		st->chip_config.lp_en_mode_off = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DMP_CLK_SEL:
		st->chip_config.clk_sel = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DEBUG_REG_ADDR:
		debug_reg_addr = data;
		break;
	case ATTR_DEBUG_REG_WRITE:
		inv_plat_single_write(st, debug_reg_addr, data);
		break;
	case ATTR_DEBUG_WRITE_CFG:
		break;
	}
	return count;
}

static int _misc_attr_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_LOW_POWER_GYRO_ON:
		st->chip_config.low_power_gyro_on = !!data;
		break;
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		st->debug_determine_engine_on = !!data;
		break;
	case ATTR_GYRO_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.fsr = data;
		result = inv_set_gyro_sf(st);
		return result;
	case ATTR_ACCEL_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.accel_fs = data;
		result = inv_set_accel_sf(st);
		return result;
	case ATTR_DMP_PED_STEP_THRESH:
		st->ped.step_thresh = data;
		return 0;
	case ATTR_DMP_PED_INT_THRESH:
		st->ped.int_thresh = data;
		return 0;
	default:
		return -EINVAL;
	}
	st->trigger_state = MISC_TRIGGER;
	result = set_inv_enable(indio_dev);

	return result;
}

/*
 * inv_misc_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_misc_attr_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _misc_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static int _debug_attr_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	if (!st->debug_determine_engine_on)
		return -EINVAL;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE:
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = !!data;
		break;
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		return -ENOSYS;
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		return -ENOSYS;
	case ATTR_DMP_GYRO_CAL_ENABLE:
		st->gyro_cal_enable = !!data;
		break;
	case ATTR_DMP_EVENT_INT_ON:
		st->chip_config.dmp_event_int_on = !!data;
		break;
	case ATTR_DMP_ON:
		st->chip_config.dmp_on = !!data;
		break;
	case ATTR_GYRO_ENABLE:
		st->chip_config.gyro_enable = !!data;
		break;
	case ATTR_ACCEL_ENABLE:
		st->chip_config.accel_enable = !!data;
		break;
	case ATTR_COMPASS_ENABLE:
		return -ENODEV;
	default:
		return -EINVAL;
	}
	st->trigger_state = DEBUG_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_debug_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_debug_attr_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _debug_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static int inv_rate_convert(struct inv_mpu_state *st, int ind, int data)
{
	int t, out, out1, out2;

	out = MPU_INIT_SENSOR_RATE;

	if ((SENSOR_L_MAG == ind) || (SENSOR_L_MAG_WAKE == ind)
	    || (SENSOR_L_MAG_CAL == ind) || (SENSOR_L_MAG_CAL_WAKE == ind)) {
		if ((MSEC_PER_SEC / st->slave_compass->rate_scale) < data)
			data = MSEC_PER_SEC / st->slave_compass->rate_scale;
	}
	t = MPU_DEFAULT_DMP_FREQ / data;
	if (!t)
		t = 1;
	out1 = MPU_DEFAULT_DMP_FREQ / (t + 1);
	out2 = MPU_DEFAULT_DMP_FREQ / t;
	if ((data - out1) * INV_ODR_BUFFER_MULTI < data)
		out = out1;
	else
		out = out2;

	return out;
}

static ssize_t inv_sensor_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return snprintf(buf, MAX_WR_SZ, "%d\n",
					st->sensor_l[this_attr->address].rate);
}

static ssize_t inv_sensor_rate_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, rate, ind;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data <= 0) {
		pr_err("sensor_rate_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	rate = inv_rate_convert(st, ind, data);

	pr_debug("sensor [%s] requested  rate %d input [%d]\n",
						sensor_l_info[ind], rate, data);

	if (rate == st->sensor_l[ind].rate)
		return count;
	mutex_lock(&indio_dev->mlock);
	st->sensor_l[ind].rate = rate;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_rate(st);
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	pr_debug("%s rate %d div %d\n", sensor_l_info[ind],
				st->sensor_l[ind].rate, st->sensor_l[ind].div);
	mutex_unlock(&indio_dev->mlock);

	return count;
}

static ssize_t inv_sensor_on_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return snprintf(buf, MAX_WR_SZ, "%d\n", st->sensor_l[this_attr->address].on);
}

static ssize_t inv_sensor_on_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, on, ind;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data < 0) {
		pr_err("sensor_on_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	on = !!data;

	pr_debug("sensor [%s] requested  %s, input [%d]\n",
			sensor_l_info[ind], (on == 1) ? "On" : "Off", data);

	if (on == st->sensor_l[ind].on) {
		pr_debug("sensor [%s] is already %s, input [%d]\n",
			sensor_l_info[ind], (on == 1) ? "On" : "Off", data);
		return count;
	}

	mutex_lock(&indio_dev->mlock);
	st->sensor_l[ind].on = on;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_rate(st);
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	pr_debug("Sensor [%s] is %s by sysfs\n",
				sensor_l_info[ind], (on == 1) ? "On" : "Off");
	return count;
}

static int inv_check_l_step(struct inv_mpu_state *st)
{
	if (st->step_counter_l_on || st->step_counter_wake_l_on)
		st->ped.on = true;
	else
		st->ped.on = false;

	return 0;
}

static int _basic_attr_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result || (data < 0))
		return -EINVAL;

	switch (this_attr->address) {
	case ATTR_DMP_PED_ON:
		if ((!!data) == st->ped.on)
			return count;
		st->ped.on = !!data;
		break;
	case ATTR_DMP_SMD_ENABLE:
		return -ENODEV;
	case ATTR_DMP_TILT_ENABLE:
		if ((!!data) == st->chip_config.tilt_enable)
			return count;
		st->chip_config.tilt_enable = !!data;
		pr_info("Tile %s\n",
			st->chip_config.tilt_enable ==
			1 ? "Enabled" : "Disabled");
		break;
	case ATTR_DMP_PICK_UP_ENABLE:
		if ((!!data) == st->chip_config.pick_up_enable) {
			pr_info("Pick_up enable already %s\n",
				st->chip_config.pick_up_enable ==
				1 ? "Enabled" : "Disabled");
			return count;
		}
		st->chip_config.pick_up_enable = !!data;
		pr_info("Pick up %s\n",
			st->chip_config.pick_up_enable ==
			1 ? "Enable" : "Disable");
		break;
	case ATTR_DMP_EIS_ENABLE:
		if ((!!data) == st->chip_config.eis_enable)
			return count;
		st->chip_config.eis_enable = !!data;
		pr_info("Eis %s\n",
			st->chip_config.eis_enable == 1 ? "Enable" : "Disable");
		break;
	case ATTR_DMP_STEP_DETECTOR_ON:
		st->step_detector_l_on = !!data;
		break;
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		st->step_detector_wake_l_on = !!data;
		break;
	case ATTR_DMP_ACTIVITY_ON:
		return -ENODEV;
	case ATTR_DMP_STEP_COUNTER_ON:
		st->step_counter_l_on = !!data;
		break;
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		st->step_counter_wake_l_on = !!data;
		break;
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		if (data == st->batch.timeout)
			return count;
		st->batch.timeout = data;
		break;
	default:
		return -EINVAL;
	};
	inv_check_l_step(st);
	inv_check_sensor_on(st);

	st->trigger_state = EVENT_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_basic_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_basic_attr_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _basic_attr_store(dev, attr, buf, count);

	mutex_unlock(&indio_dev->mlock);

	return result;
}

static int _ois_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	switch (this_attr->address) {
	case IN_OIS_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ois.en);
	case IN_OIS_ACCEL_FS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						ois_accel_fs[st->ois.accel_fs]);
	case IN_OIS_GYRO_FS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						ois_gyro_fs[st->ois.gyro_fs]);
	default:
		return -EINVAL;
	}
}
static ssize_t inv_ois_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _ois_show(dev, attr, buf);

	mutex_unlock(&indio_dev->mlock);

	return result;
}
static ssize_t _ois_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	int result;
	u8 v;

	result = kstrtoint(buf, 10, &data);
	if (result || (data < 0))
		return -EINVAL;

	switch (this_attr->address) {
	case IN_OIS_ENABLE:
		st->ois.en = !!data;
		if (st->ois.en)
			v = BIT_OIS_ENABLE;
		else
			v = 0;
		result = inv_plat_single_write(st, REG_USER_CTRL_NEW, v);
		if (result)
			return result;
		v = (st->ois.gyro_fs << OIS_GYRO_FS_SHIFT);
		if (st->ois.en)
			v |= OIS_FCHOICE_10;
		result = inv_plat_single_write(st, REG_SIGNAL_PATH_RESET, v);
		if (result)
			return result;
		set_inv_enable(indio_dev);
		break;
	case IN_OIS_ACCEL_FS:
		if ((data < ARRAY_SIZE(ois_accel_fs))) {
			result = inv_plat_read(st, REG_ACCEL_CONFIG, 1, &v);
			if (result)
				return result;
			v |= data;
			result = inv_plat_single_write(st, REG_ACCEL_CONFIG, v);
			if (result)
				return result;
			st->ois.accel_fs = data;
		} else {
			return -EINVAL;
		}

		break;
	case IN_OIS_GYRO_FS:
		if ((data < ARRAY_SIZE(ois_gyro_fs))
						&& (ois_gyro_fs[data] != -1)) {
			result = inv_plat_single_write(st,
				REG_SIGNAL_PATH_RESET,
				OIS_FCHOICE_10 | (data << OIS_GYRO_FS_SHIFT));
			if (result)
				return result;
			st->ois.gyro_fs = data;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return count;
}

/*
 * inv_basic_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_ois_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	inv_switch_power_in_lp(st, true);
	result = _ois_store(dev, attr, buf, count);
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

/*
 * inv_attr_show() -  calling this function will show current
 *                        dmp parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	s8 *m;

	switch (this_attr->address) {
	case ATTR_GYRO_SCALE:
		{
			const s16 gyro_scale[] = { 250, 500, 1000, 2000 };

			return snprintf(buf, MAX_WR_SZ, "%d\n",
				       gyro_scale[st->chip_config.fsr]);
		}
	case ATTR_ACCEL_SCALE:
		{
			const s16 accel_scale[] = { 2, 4, 8, 16 };
			return snprintf(buf, MAX_WR_SZ, "%d\n",
				       accel_scale[st->chip_config.accel_fs]);
		}
	case ATTR_COMPASS_SCALE:
		st->slave_compass->get_scale(st, &result);
		return snprintf(buf, MAX_WR_SZ, "%d\n", result);
	case ATTR_COMPASS_SENSITIVITY_X:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_info.compass_sens[0]);
	case ATTR_COMPASS_SENSITIVITY_Y:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_info.compass_sens[1]);
	case ATTR_COMPASS_SENSITIVITY_Z:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_info.compass_sens[2]);
	case ATTR_GYRO_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.gyro_enable);
	case ATTR_ACCEL_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.accel_enable);
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		return -ENOSYS;
	case ATTR_DMP_GYRO_CAL_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_cal_enable);
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->debug_determine_engine_on);
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		return -ENOSYS;
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		return -ENOSYS;
	case ATTR_FIRMWARE_LOADED:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
					st->chip_config.firmware_loaded);
	case ATTR_DMP_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.dmp_on);
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->batch.timeout);
	case ATTR_DMP_EVENT_INT_ON:
		return snprintf(buf, MAX_WR_SZ,
				"%d\n", st->chip_config.dmp_event_int_on);
	case ATTR_DMP_PED_INT_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.int_on);
	case ATTR_DMP_PED_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.on);
	case ATTR_DMP_PED_STEP_THRESH:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.step_thresh);
	case ATTR_DMP_PED_INT_THRESH:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.int_thresh);
	case ATTR_DMP_SMD_ENABLE:
		return -ENODEV;
	case ATTR_DMP_TILT_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.tilt_enable);
	case ATTR_DMP_PICK_UP_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.pick_up_enable);
	case ATTR_DMP_EIS_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.eis_enable);
	case ATTR_DMP_LOW_POWER_GYRO_ON:
		return -ENOSYS;
	case ATTR_DMP_LP_EN_OFF:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.lp_en_mode_off);
	case ATTR_COMPASS_ENABLE:
		return -ENODEV;
	case ATTR_DMP_STEP_COUNTER_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_counter_l_on);
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_counter_wake_l_on);
	case ATTR_DMP_STEP_DETECTOR_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_detector_l_on);
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_detector_wake_l_on);
	case ATTR_DMP_ACTIVITY_ON:
		return -ENODEV;
	case ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			       st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on);
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		return -ENOSYS;
	case ATTR_GYRO_MATRIX:
		m = st->plat_data.orientation;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			       m[8]);
	case ATTR_ACCEL_MATRIX:
		m = st->plat_data.orientation;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			       m[8]);
	case ATTR_COMPASS_MATRIX:
		if (st->plat_data.sec_slave_type ==
		    SECONDARY_SLAVE_TYPE_COMPASS)
			m = st->plat_data.secondary_orientation;
		else
			return -ENODEV;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			       m[8]);
	case ATTR_SECONDARY_NAME:
		{
			const char *n[] = { "NULL", "AK8975", "AK8972",
				"AK8963", "MLX90399", "AK09911", "AK09912", "AK09916"};

			switch (st->plat_data.sec_slave_id) {
			case COMPASS_ID_AK8975:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[1]);
			case COMPASS_ID_AK8972:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[2]);
			case COMPASS_ID_AK8963:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[3]);
			case COMPASS_ID_MLX90399:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[4]);
			case COMPASS_ID_AK09911:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[5]);
			case COMPASS_ID_AK09912:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[6]);
			case COMPASS_ID_AK09916:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[7]);
			default:
				return snprintf(buf, MAX_WR_SZ, "%s\n", n[0]);
			}
		}
	case ATTR_DMP_DEBUG_MEM_READ:
		{
			int out;

			inv_switch_power_in_lp(st, true);
			result =
			    read_be32_from_mem(st, &out, debug_mem_read_addr);
			if (result)
				return result;
			inv_switch_power_in_lp(st, false);
			return snprintf(buf, MAX_WR_SZ, "0x%x\n", out);
		}
	case ATTR_DMP_MAGN_ACCURACY:
		return -ENODEV;
	case ATTR_GYRO_SF:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_sf);
	case ATTR_ANGLVEL_X_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_st_bias[0]);
	case ATTR_ANGLVEL_Y_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_st_bias[1]);
	case ATTR_ANGLVEL_Z_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_st_bias[2]);
	case ATTR_ANGLVEL_X_OIS_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->gyro_ois_st_bias[0]);
	case ATTR_ANGLVEL_Y_OIS_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->gyro_ois_st_bias[1]);
	case ATTR_ANGLVEL_Z_OIS_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->gyro_ois_st_bias[2]);
	case ATTR_ACCEL_X_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_st_bias[0]);
	case ATTR_ACCEL_Y_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_st_bias[1]);
	case ATTR_ACCEL_Z_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_st_bias[2]);
	case ATTR_ACCEL_X_OIS_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_ois_st_bias[0]);
	case ATTR_ACCEL_Y_OIS_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_ois_st_bias[1]);
	case ATTR_ACCEL_Z_OIS_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_ois_st_bias[2]);
	case ATTR_GYRO_X_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_gyro_bias[0]);
	case ATTR_GYRO_Y_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_gyro_bias[1]);
	case ATTR_GYRO_Z_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_gyro_bias[2]);
	case ATTR_ACCEL_X_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_accel_bias[0]);
	case ATTR_ACCEL_Y_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_accel_bias[1]);
	case ATTR_ACCEL_Z_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_accel_bias[2]);
	default:
		return -EPERM;
	}
}

static ssize_t inv_self_test(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int res;

	mutex_lock(&indio_dev->mlock);
	res = inv_hw_self_test(st);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return snprintf(buf, MAX_WR_SZ, "%d\n", res);
}

static ssize_t inv_ois_self_test(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int res;

	mutex_lock(&indio_dev->mlock);
	res = inv_hw_self_test(st);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return snprintf(buf, MAX_WR_SZ, "%d\n", res);
}

/*
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

/*
 * inv_ped_show() -  calling this function showes pedometer interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_ped_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_WR_SZ, "1\n");
}

static ssize_t inv_tilt_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

static ssize_t inv_pick_up_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

/*
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	int bytes_printed = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	mutex_lock(&indio_dev->mlock);
	bytes_printed += snprintf(buf + bytes_printed, MAX_WR_SZ, "bank 0\n");

	for (ii = 0; ii < 0x7F; ii++) {
		/* don't read fifo r/w register */
		if ((ii == REG_MEM_R_W) || (ii == REG_FIFO_R_W))
			data = 0;
		else
			inv_plat_read(st, ii, 1, &data);
		bytes_printed += snprintf(buf + bytes_printed, MAX_WR_SZ,
				"%#2x: %#2x\n", ii, data);
	}
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return bytes_printed;
}

static ssize_t inv_flush_batch_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;

	mutex_lock(&indio_dev->mlock);
	result = inv_flush_batch_data(indio_dev, data);
	mutex_unlock(&indio_dev->mlock);

	return count;
}

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU_SCAN_TIMESTAMP),
};

static DEVICE_ATTR(poll_pedometer, S_IRUGO, inv_ped_show, NULL);
static DEVICE_ATTR(poll_tilt, S_IRUGO, inv_tilt_show, NULL);
static DEVICE_ATTR(poll_pick_up, S_IRUGO, inv_pick_up_show, NULL);

/* special run time sysfs entry, read only */
static DEVICE_ATTR(debug_reg_dump, S_IRUGO | S_IWUGO, inv_reg_dump_show, NULL);
static DEVICE_ATTR(out_temperature, S_IRUGO | S_IWUGO,
		   inv_temperature_show, NULL);
static DEVICE_ATTR(misc_self_test, S_IRUGO | S_IWUGO, inv_self_test, NULL);
static DEVICE_ATTR(misc_ois_self_test, S_IRUGO | S_IWUGO,
						inv_ois_self_test, NULL);

static IIO_DEVICE_ATTR(info_anglvel_matrix, S_IRUGO, inv_attr_show, NULL,
		       ATTR_GYRO_MATRIX);
static IIO_DEVICE_ATTR(info_accel_matrix, S_IRUGO, inv_attr_show, NULL,
		       ATTR_ACCEL_MATRIX);
static IIO_DEVICE_ATTR(info_magn_matrix, S_IRUGO, inv_attr_show, NULL,
		       ATTR_COMPASS_MATRIX);

static IIO_DEVICE_ATTR(info_secondary_name, S_IRUGO, inv_attr_show, NULL,
		       ATTR_SECONDARY_NAME);
static IIO_DEVICE_ATTR(info_gyro_sf, S_IRUGO, inv_attr_show, NULL,
		       ATTR_GYRO_SF);
/* write only sysfs */
static DEVICE_ATTR(misc_flush_batch, S_IWUGO, NULL, inv_flush_batch_store);

/* sensor on/off sysfs control */
static IIO_DEVICE_ATTR(in_accel_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store, SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store, SENSOR_L_GYRO);
static IIO_DEVICE_ATTR(in_magn_enable, S_IRUGO | S_IWUGO, inv_sensor_on_show,
		       inv_sensor_on_store, SENSOR_L_MAG);
static IIO_DEVICE_ATTR(in_eis_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_EIS_GYRO);
static IIO_DEVICE_ATTR(in_accel_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_ACCEL_WAKE);
static IIO_DEVICE_ATTR(in_anglvel_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_GYRO_WAKE);
static IIO_DEVICE_ATTR(in_magn_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_MAG_WAKE);

/* sensor rate sysfs control */
static IIO_DEVICE_ATTR(in_accel_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_rate, S_IRUGO | S_IWUGO, inv_sensor_rate_show,
		       inv_sensor_rate_store, SENSOR_L_GYRO);
static IIO_DEVICE_ATTR(in_magn_rate, S_IRUGO | S_IWUGO, inv_sensor_rate_show,
		       inv_sensor_rate_store, SENSOR_L_MAG);
static IIO_DEVICE_ATTR(in_eis_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_EIS_GYRO);
static IIO_DEVICE_ATTR(in_accel_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_ACCEL_WAKE);
static IIO_DEVICE_ATTR(in_anglvel_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_GYRO_WAKE);
static IIO_DEVICE_ATTR(in_magn_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_MAG_WAKE);

/* OIS related sysfs */
static IIO_DEVICE_ATTR(in_ois_accel_fs, S_IRUGO | S_IWUGO, inv_ois_show,
		       inv_ois_store, IN_OIS_ACCEL_FS);
static IIO_DEVICE_ATTR(in_ois_gyro_fs, S_IRUGO | S_IWUGO, inv_ois_show,
		       inv_ois_store, IN_OIS_GYRO_FS);
static IIO_DEVICE_ATTR(in_ois_enable, S_IRUGO | S_IWUGO, inv_ois_show,
		       inv_ois_store, IN_OIS_ENABLE);

/* debug determine engine related sysfs */
static IIO_DEVICE_ATTR(debug_anglvel_accuracy_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store,
		       ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_accuracy_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store,
		       ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_gyro_cal_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_GYRO_CAL_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_cal_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_ACCEL_CAL_ENABLE);

static IIO_DEVICE_ATTR(debug_gyro_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store, ATTR_GYRO_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store, ATTR_ACCEL_ENABLE);
static IIO_DEVICE_ATTR(debug_compass_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store,
		       ATTR_COMPASS_ENABLE);
static IIO_DEVICE_ATTR(debug_dmp_on, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_ON);
static IIO_DEVICE_ATTR(debug_dmp_event_int_on, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_EVENT_INT_ON);
static IIO_DEVICE_ATTR(debug_magn_accuracy, S_IRUGO | S_IWUGO, inv_attr_show,
		       NULL, ATTR_DMP_MAGN_ACCURACY);

static IIO_DEVICE_ATTR(misc_batchmode_timeout, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_BATCHMODE_TIMEOUT);

/* engine scale */
static IIO_DEVICE_ATTR(in_accel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_misc_attr_store, ATTR_ACCEL_SCALE);
static IIO_DEVICE_ATTR(in_anglvel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_misc_attr_store, ATTR_GYRO_SCALE);
static IIO_DEVICE_ATTR(in_magn_scale, S_IRUGO | S_IWUGO, inv_attr_show,
		       NULL, ATTR_COMPASS_SCALE);

static IIO_DEVICE_ATTR(in_magn_sensitivity_x, S_IRUGO | S_IWUGO, inv_attr_show,
		       NULL, ATTR_COMPASS_SENSITIVITY_X);
static IIO_DEVICE_ATTR(in_magn_sensitivity_y, S_IRUGO | S_IWUGO, inv_attr_show,
		       NULL, ATTR_COMPASS_SENSITIVITY_Y);
static IIO_DEVICE_ATTR(in_magn_sensitivity_z, S_IRUGO | S_IWUGO, inv_attr_show,
		       NULL, ATTR_COMPASS_SENSITIVITY_Z);

static IIO_DEVICE_ATTR(debug_low_power_gyro_on, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store,
		       ATTR_DMP_LOW_POWER_GYRO_ON);
static IIO_DEVICE_ATTR(debug_lp_en_off, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DMP_LP_EN_OFF);
static IIO_DEVICE_ATTR(debug_clock_sel, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DMP_CLK_SEL);
static IIO_DEVICE_ATTR(debug_reg_write, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DEBUG_REG_WRITE);
static IIO_DEVICE_ATTR(debug_cfg_write, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DEBUG_WRITE_CFG);
static IIO_DEVICE_ATTR(debug_reg_write_addr, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DEBUG_REG_ADDR);

static IIO_DEVICE_ATTR(in_accel_x_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_accel_x_ois_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_X_OIS_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_ois_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_Y_OIS_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_ois_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_Z_OIS_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_ois_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_X_OIS_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_ois_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_Y_OIS_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_ois_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_Z_OIS_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_accel_x_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_ACCEL_X_OFFSET);
static IIO_DEVICE_ATTR(in_accel_y_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_ACCEL_Y_OFFSET);
static IIO_DEVICE_ATTR(in_accel_z_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_ACCEL_Z_OFFSET);

static IIO_DEVICE_ATTR(in_anglvel_x_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_GYRO_X_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_y_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_GYRO_Y_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_z_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_GYRO_Z_OFFSET);

static IIO_DEVICE_ATTR(debug_determine_engine_on, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store,
		       ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON);

static IIO_DEVICE_ATTR(in_step_detector_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_STEP_DETECTOR_ON);
static IIO_DEVICE_ATTR(in_step_detector_wake_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_STEP_DETECTOR_WAKE_ON);
static IIO_DEVICE_ATTR(in_step_counter_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_basic_attr_store, ATTR_DMP_STEP_COUNTER_ON);
static IIO_DEVICE_ATTR(in_step_counter_wake_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_STEP_COUNTER_WAKE_ON);

static IIO_DEVICE_ATTR(event_tilt_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_TILT_ENABLE);

static IIO_DEVICE_ATTR(event_eis_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_EIS_ENABLE);

static IIO_DEVICE_ATTR(event_pick_up_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_PICK_UP_ENABLE);

static IIO_DEVICE_ATTR(event_pedometer_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store, ATTR_DMP_PED_ON);

static const struct attribute *inv_raw_attributes[] = {
	&dev_attr_debug_reg_dump.attr,
	&dev_attr_out_temperature.attr,
	&dev_attr_misc_flush_batch.attr,
	&dev_attr_misc_self_test.attr,
	&iio_dev_attr_in_accel_enable.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_enable.dev_attr.attr,
	&iio_dev_attr_info_accel_matrix.dev_attr.attr,
	&iio_dev_attr_in_accel_scale.dev_attr.attr,
	&iio_dev_attr_misc_batchmode_timeout.dev_attr.attr,
	&iio_dev_attr_in_accel_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_rate.dev_attr.attr,
	&iio_dev_attr_info_secondary_name.dev_attr.attr,
	&iio_dev_attr_debug_magn_accuracy.dev_attr.attr,
};

static const struct attribute *inv_debug_attributes[] = {
	&iio_dev_attr_debug_accel_enable.dev_attr.attr,
	&iio_dev_attr_debug_dmp_event_int_on.dev_attr.attr,
	&iio_dev_attr_debug_low_power_gyro_on.dev_attr.attr,
	&iio_dev_attr_debug_lp_en_off.dev_attr.attr,
	&iio_dev_attr_debug_clock_sel.dev_attr.attr,
	&iio_dev_attr_debug_reg_write.dev_attr.attr,
	&iio_dev_attr_debug_reg_write_addr.dev_attr.attr,
	&iio_dev_attr_debug_cfg_write.dev_attr.attr,
	&iio_dev_attr_debug_dmp_on.dev_attr.attr,
	&iio_dev_attr_debug_accel_cal_enable.dev_attr.attr,
	&iio_dev_attr_debug_accel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_debug_determine_engine_on.dev_attr.attr,
	&iio_dev_attr_debug_gyro_enable.dev_attr.attr,
	&iio_dev_attr_debug_gyro_cal_enable.dev_attr.attr,
	&iio_dev_attr_debug_anglvel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_debug_compass_enable.dev_attr.attr,
};

static const struct attribute *inv_gyro_attributes[] = {
	&iio_dev_attr_info_anglvel_matrix.dev_attr.attr,
	&iio_dev_attr_in_anglvel_enable.dev_attr.attr,
	&iio_dev_attr_in_eis_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale.dev_attr.attr,
	&iio_dev_attr_in_anglvel_rate.dev_attr.attr,
	&iio_dev_attr_in_eis_rate.dev_attr.attr,
	&iio_dev_attr_in_anglvel_wake_rate.dev_attr.attr,
	&iio_dev_attr_info_gyro_sf.dev_attr.attr,
};

static const struct attribute *inv_bias_attributes[] = {
	&iio_dev_attr_in_accel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_z_offset.dev_attr.attr,
};

static const struct attribute *inv_compass_attributes[] = {
	&iio_dev_attr_in_magn_sensitivity_x.dev_attr.attr,
	&iio_dev_attr_in_magn_sensitivity_y.dev_attr.attr,
	&iio_dev_attr_in_magn_sensitivity_z.dev_attr.attr,
	&iio_dev_attr_in_magn_scale.dev_attr.attr,
	&iio_dev_attr_info_magn_matrix.dev_attr.attr,
	&iio_dev_attr_in_magn_enable.dev_attr.attr,
	&iio_dev_attr_in_magn_rate.dev_attr.attr,
	&iio_dev_attr_in_magn_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_magn_wake_rate.dev_attr.attr,
};

static const struct attribute *inv_pedometer_attributes[] = {
	&dev_attr_poll_pedometer.attr,
	&dev_attr_poll_tilt.attr,
	&dev_attr_poll_pick_up.attr,
	&iio_dev_attr_event_pedometer_enable.dev_attr.attr,
	&iio_dev_attr_event_tilt_enable.dev_attr.attr,
	&iio_dev_attr_event_eis_enable.dev_attr.attr,
	&iio_dev_attr_event_pick_up_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_wake_enable.dev_attr.attr,
};

static const struct attribute *inv_ois_attributes[] = {
	&dev_attr_misc_ois_self_test.attr,
	&iio_dev_attr_in_ois_accel_fs.dev_attr.attr,
	&iio_dev_attr_in_ois_gyro_fs.dev_attr.attr,
	&iio_dev_attr_in_ois_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_ois_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_ois_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_ois_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_x_ois_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_ois_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_ois_st_calibbias.dev_attr.attr,
};

static struct attribute *inv_attributes[ARRAY_SIZE(inv_raw_attributes) +
					ARRAY_SIZE(inv_debug_attributes) +
					ARRAY_SIZE(inv_gyro_attributes) +
					ARRAY_SIZE(inv_bias_attributes) +
					ARRAY_SIZE(inv_compass_attributes) +
					ARRAY_SIZE(inv_pedometer_attributes) +
					ARRAY_SIZE(inv_ois_attributes) + 1];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.attrs = &inv_attribute_group,
};

/*
 *  inv_check_chip_type() - check and setup chip type.
 */
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name)
{
	int result;
	int t_ind;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	conf = &st->chip_config;
	plat = &st->plat_data;

	if (!strcmp(name, "icm20690"))
		st->chip_type = ICM20690;
	else if (!strcmp(name, "icm20602"))
		st->chip_type = ICM20602;
	else if (!strcmp(name, "icm20608d"))
		st->chip_type = ICM20608D;
	else
		return -EPERM;
	st->chip_config.has_gyro = 1;

	st->hw = &hw_info[st->chip_type];
	result = inv_mpu_initialize(st);
	if (result)
		return result;

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_raw_attributes,
					sizeof(inv_raw_attributes));
	t_ind += ARRAY_SIZE(inv_raw_attributes);

	memcpy(&inv_attributes[t_ind], inv_debug_attributes,
					sizeof(inv_debug_attributes));
	t_ind += ARRAY_SIZE(inv_debug_attributes);

	memcpy(&inv_attributes[t_ind], inv_pedometer_attributes,
					sizeof(inv_pedometer_attributes));
	t_ind += ARRAY_SIZE(inv_pedometer_attributes);

	memcpy(&inv_attributes[t_ind], inv_gyro_attributes,
					sizeof(inv_gyro_attributes));
	t_ind += ARRAY_SIZE(inv_gyro_attributes);

	memcpy(&inv_attributes[t_ind], inv_ois_attributes,
					sizeof(inv_ois_attributes));
	t_ind += ARRAY_SIZE(inv_ois_attributes);

	memcpy(&inv_attributes[t_ind], inv_bias_attributes,
					sizeof(inv_bias_attributes));
	t_ind += ARRAY_SIZE(inv_bias_attributes);

	if (st->chip_config.has_compass) {
		memcpy(&inv_attributes[t_ind],
					inv_compass_attributes,
					sizeof(inv_compass_attributes));
		t_ind += ARRAY_SIZE(inv_compass_attributes);
	}

	inv_attributes[t_ind] = NULL;

	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	return result;
}

int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	return 0;
}
