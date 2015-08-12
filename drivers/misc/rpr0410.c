/*******************************************************************************
* Copyright 2013 Broadcom Corporation.  All rights reserved.
*
*       @file   drivers/misc/rpr0410.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/sensors_core.h>
#include <linux/hrtimer.h>
#include <linux/rpr0410.h>

#define RPR0410_SYSTEM_CONTROL	0x40
#define RPR0410_MODE_CONTROL	0x41
#define RPR0410_ALS_PS_CONTROL	0x42
#define RPR0410_PERSIST		0x43
#define RPR0410_PS_DATA_LSB	0x44
#define RPR0410_PS_DATA_MSB	0x45
#define RPR0410_ALS_DATA0_LSB	0x46
#define RPR0410_ALS_DATA0_MSB	0x47
#define RPR0410_ALS_DATA1_LSB	0x48
#define RPR0410_ALS_DATA1_MSB	0x49
#define RPR0410_INTERRUPT	0x4a
#define RPR0410_PS_TH_LSB	0x4b
#define RPR0410_PS_TH_MSB	0x4c
#define RPR0410_PS_TL_LSB	0x4d
#define RPR0410_PS_TL_MSB	0x4e
#define RPR0410_ALS_TH_LSB	0x4f
#define RPR0410_ALS_TH_MSB	0x50
#define RPR0410_ALS_TL_LSB	0x51
#define RPR0410_ALS_TL_MSB	0x52

#define RPR0410_INT_RESET_CMD	0x80
#define RPR0410_SW_RESET_CMD	0x40

#define RPR0410_MEASURE_TIME_MASK	0x0f
#define RPR0410_ALS_GAIN_MASK		0x3c	/*[0x42/5:2] */
#define RPR0410_LED_DRIVE_MASK		0x03	/*[0x42/1:0] */

#define RPR0410_PERSIST_MASK		0x0f	/*[0x43/3:0] */

#define RPR0410_INT_TRIG_MASK		0x03	/*[0x4a/1:0] */
#define RPR0410_INT_LATCH_MASK		0x04
#define RPR0410_INT_ASSERT_MASK		0x08
#define RPR0410_INT_MODE_MASK		0x30
#define RPR0410_ALS_INT_STATUS		0x40
#define RPR0410_PS_INT_STATUS		0x80

#define I2C_RETRY_DELAY 5
#define I2C_RETRIES 5
#define LIGHT_SENSOR_START_TIME_DELAY 50000000

struct rpr0410_reg {
	const char *name;
	u8 reg;
} rpr0410_regs[] = {
	{
	"SYSTEM_CONTROL", RPR0410_SYSTEM_CONTROL}, {
	"MODE_CONTROL", RPR0410_MODE_CONTROL}, {
	"ALS_PS_CONTROL", RPR0410_ALS_PS_CONTROL}, {
	"PERSIST", RPR0410_PERSIST}, {
	"PS_DATA_LSB", RPR0410_PS_DATA_LSB}, {
	"PS_DATA_MSB", RPR0410_PS_DATA_MSB}, {
	"ALS_DATA0_LSB", RPR0410_ALS_DATA0_LSB}, {
	"ALS_DATA0_MSB", RPR0410_ALS_DATA0_MSB}, {
	"ALS_DATA1_LSB", RPR0410_ALS_DATA1_LSB}, {
	"ALS_DATA1_MSB", RPR0410_ALS_DATA1_MSB}, {
	"INTERRUPT", RPR0410_INTERRUPT}, {
	"PS_TH_LSB", RPR0410_PS_TH_LSB}, {
	"PS_TH_MSB", RPR0410_PS_TH_MSB}, {
	"PS_TL_LSB", RPR0410_PS_TL_LSB}, {
	"PS_TL_MSB", RPR0410_PS_TL_MSB}, {
	"ALS_TH_LSB", RPR0410_ALS_TH_LSB}, {
	"ALS_TH_MSB", RPR0410_ALS_TH_MSB}, {
	"ALS_TL_LSB", RPR0410_ALS_TL_LSB}, {
"ALS_TL_MSB", RPR0410_ALS_TL_MSB},};

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

#define MAX_MODE_CTRL_ITEM	(16)
#define RPR0410_ALS_LEVEL_NUM	(15)

static const struct mode_ctrl_table {
	short als;
	short ps;
} g_mtable[MAX_MODE_CTRL_ITEM] = {
	{
	0, 0},			/*  0 */
	{
	0, 10},			/*  1 */
	{
	0, 40},			/*  2 */
	{
	0, 100},		/*  3 */
	{
	0, 400},		/*  4 */
	{
	100, 0},		/*  5 */
	{
	100, 100},		/*  6 */
	{
	100, 400},		/*  7 */
	{
	400, 0},		/*  8 */
	{
	400, 100},		/*  9 */
	{
	400, 0},		/* 10 */
	{
	400, 400},		/* 11 */
	{
	0, 0},			/* 12 */
	{
	0, 0},			/* 13 */
	{
	0, 0},			/* 14 */
	{
	0, 0}			/* 15 */
};

/* gain table */
#define MAX_ALS_GAIN_ITEM (16)
static const struct als_gain_table_t {
	char data0;
	char data1;
} g_agtable[MAX_ALS_GAIN_ITEM] = {
	{
	1, 1},			/*  0 */
	{
	0, 0},			/*  1 */
	{
	0, 0},			/*  2 */
	{
	0, 0},			/*  3 */
	{
	2, 1},			/*  4 */
	{
	2, 2},			/*  5 */
	{
	0, 0},			/*  6 */
	{
	0, 0},			/*  7 */
	{
	0, 0},			/*  8 */
	{
	0, 0},			/*  9 */
	{
	0, 0},			/* 10 */
	{
	0, 0},			/* 11 */
	{
	64, 64},		/* 12 */
	{
	0, 0},			/* 13 */
	{
	128, 64},		/* 14 */
	{
	128, 128}		/* 15 */
};

/*************** Global Data ******************/
/* parameter for als calculation */
#define COEFFICIENT               (4)
const unsigned long data0_coefficient[COEFFICIENT] = { 192, 141, 127, 117 };
const unsigned long data1_coefficient[COEFFICIENT] = { 316, 108, 86, 74 };
const unsigned long judge_coefficient[COEFFICIENT] = { 29, 65, 85, 158 };

/* driver data */
struct rpr0410_int_cfg_t {
	u8 int_mode;
	u8 int_assert;
	u8 int_latch;
	u8 int_trig;
};
struct rpr0410_data {
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct i2c_client *i2c_client;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct mutex power_lock;
	struct wake_lock prox_wake_lock;
	struct mutex update_lock;
	struct workqueue_struct *light_wq;
	struct workqueue_struct *prox_wq;
	struct hrtimer timer_light;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	u8 measure_time;
	u8 als_measure_time;
	u8 ps_measure_time;
	u8 als_gain;
	u8 als_ps_gain0;
	u8 als_ps_gain1;
	u8 als_ps_led;
	u8 persist;
	u16 prox_threshold_high;
	u16 prox_threshold_low;
	ktime_t als_delay;
	u8 power_state;
	u8 interrupt_mode;
	u8 interrupt_assert;
	u8 interrupt_latch;
	u8 interrupt_trig;
	u16 crosstalk;
	int irq;
};

static int _rpr0410_suspend(struct i2c_client *client, pm_message_t mesg);
static int _rpr0410_resume(struct i2c_client *client);

int rpr0410_i2c_read(struct rpr0410_data *rpr0410, u8 addr, u8 *val)
{
	int err;
	int tries;
	u8 buf[2];
	buf[0] = addr;
	struct i2c_msg msgs[] = {
		{
		 .addr = rpr0410->i2c_client->addr,
		 .flags = rpr0410->i2c_client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = rpr0410->i2c_client->addr,
		 .flags = (rpr0410->i2c_client->flags & I2C_M_TEN)
		 | I2C_M_RD,
		 .len = 1,
		 .buf = buf,
		 },
	};
	tries = 0;
	do {
		err = i2c_transfer(rpr0410->i2c_client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&rpr0410->i2c_client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
		*val = buf[0];
	}
	return err;
}

int rpr0410_i2c_write(struct rpr0410_data *rpr0410, u8 addr, u8 val)
{
	int err;
	int tries = 0;
	u8 buf[2] = { addr, val };
	struct i2c_msg msgs[] = {
		{
		 .addr = rpr0410->i2c_client->addr,
		 .flags = rpr0410->i2c_client->flags & I2C_M_TEN,
		 .len = 2,
		 .buf = buf,
		 },
	};
	do {
		err = i2c_transfer(rpr0410->i2c_client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));
	if (err != 1) {
		dev_err(&rpr0410->i2c_client->dev, "write transfer error\n");
		err = -EIO;
	} else
		err = 0;
	return err;
}

static int rpr0410_is_mtable_valid(struct mode_ctrl_table mt)
{
	int i;
	int ret;
	ret = 0;
	for (i = 0; i < MAX_MODE_CTRL_ITEM; i++) {
		if (mt.als == g_mtable[i].als && mt.ps == g_mtable[i].ps) {
			ret = 1;
			break;
		}
	}
	return ret;
}

static void rpr0410_alp_enable(struct rpr0410_data *rpr0410, bool enable_als,
			       bool enable_ps)
{
	int ret;
	struct mode_ctrl_table mt;
	ret = 0;
	/*mode control */
	mt.als = g_mtable[rpr0410->measure_time].als;
	mt.ps = g_mtable[rpr0410->measure_time].ps;
	ret = rpr0410_is_mtable_valid(mt);
	pr_info("rpr0410_alp_enable: als=%d, ps=%d\n", enable_als, enable_ps);
	if (!ret) {
		pr_info("rpr0410: mode control parameter is invalid\n");
		return;
	}
	mutex_lock(&rpr0410->update_lock);
	if (enable_als == 1 && enable_ps == 1) {
		rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL,
				  rpr0410->measure_time);
	} else if (enable_als == 1 && enable_ps == 0) {
		if (g_mtable[rpr0410->measure_time].als == 400)
			rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x08);
		else
			rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x05);

	} else if (enable_als == 0 && enable_ps == 1) {
		if (g_mtable[rpr0410->measure_time].ps == 10)
			rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x01);
		else if (g_mtable[rpr0410->measure_time].ps == 40)
			rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x02);
		else if (g_mtable[rpr0410->measure_time].ps == 100)
			rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x03);
		else if (g_mtable[rpr0410->measure_time].ps == 400)
			rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x04);
	} else if (enable_als == 0 && enable_ps == 0)
		rpr0410_i2c_write(rpr0410, RPR0410_MODE_CONTROL, 0x00);
	mutex_unlock(&rpr0410->update_lock);
}

static int rpr0410_als_ps_control_set(struct rpr0410_data *rpr0410, u8 als_gain,
				      u8 led_drive)
{
	int ret;
	u8 reg;
	ret = 0;
	reg = (led_drive & RPR0410_LED_DRIVE_MASK) | ((als_gain << 2) &
						      RPR0410_ALS_GAIN_MASK);
	ret = rpr0410_i2c_write(rpr0410, RPR0410_ALS_PS_CONTROL, reg);
	if (ret < 0)
		pr_err("[rpr0410] cannot write als gain and led drive\n");
	rpr0410->als_ps_gain0 = g_agtable[als_gain].data0;
	rpr0410->als_ps_gain1 = g_agtable[als_gain].data1;
	return ret;
}

static int rpr0410_persist_set(struct rpr0410_data *rpr0410, u8 persist)
{
	int ret;
	u8 reg;
	ret = 0;
	reg = persist & RPR0410_PERSIST_MASK;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_PERSIST, reg);
	if (ret < 0)
		pr_err("[rpr0410] cannot write persist\n");
	return ret;
}

static int rpr0410_interrupt_set(struct rpr0410_data *rpr0410,
				 struct rpr0410_int_cfg_t cfg)
{
	int ret;
	u8 reg;
	ret = 0;
	reg = (cfg.int_trig & RPR0410_INT_TRIG_MASK)
	    | ((cfg.int_latch << 2) & RPR0410_INT_LATCH_MASK)
	    | ((cfg.int_assert << 3) & RPR0410_INT_ASSERT_MASK)
	    | ((cfg.int_mode << 4) & RPR0410_INT_MODE_MASK);
	ret = rpr0410_i2c_write(rpr0410, RPR0410_INTERRUPT, reg);
	if (ret < 0)
		pr_err("[rpr0410] cannot write persist\n");
	return ret;
}

static int rpr0410_als_threshold_set(struct rpr0410_data *rpr0410,
				     u16 tl, u16 th)
{
	int ret;
	u8 reg;
	ret = 0;
	reg = th & 0x00ff;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_ALS_TH_LSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_ALS_TH_LSB err=%d\n", ret);
	reg = (th & 0xff00) >> 8;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_ALS_TH_MSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_ALS_TH_MSB err=%d\n", ret);
	reg = tl & 0x00ff;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_ALS_TL_LSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_ALS_TL_LSB err=%d\n", ret);
	reg = (tl & 0xff00) >> 8;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_ALS_TL_MSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_ALS_TH_MSB err=%d\n", ret);
	return ret;

}

static int rpr0410_ps_threshold_set(struct rpr0410_data *rpr0410,
				    u16 tl, u16 th)
{
	int ret;
	u8 reg;
	ret = 0;
	reg = th & 0x00ff;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TH_LSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_PS_TH_LSB err=%d\n", ret);
	reg = (th & 0x0f00) >> 8;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TH_MSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_PS_TH_MSB err=%d\n", ret);
	reg = tl & 0x00ff;

	ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_LSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_PS_TL_LSB err=%d\n", ret);
	reg = (tl & 0x0f00) >> 8;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_MSB, reg);
	if (ret < 0)
		pr_err("[rpr0410] write RPR0410_PS_TL_MSB err=%d\n", ret);
	return ret;
}

static int rpr0410_proximity_calibration(struct rpr0410_data *rpr0410,
					 u8 step, u8 max_steps)
{
	int ret = 0;
	u8 prox_state = 0;
	u16 pdata;
	u32 avg;
	u32 sum;
	u8 reg;
	int i;
	reg = 0;
	sum = 0;
	if (!(rpr0410->power_state & PROXIMITY_ENABLED)) {
		rpr0410->power_state |= PROXIMITY_ENABLED;
		prox_state = 1;
		if (rpr0410->power_state & LIGHT_ENABLED)
			rpr0410_alp_enable(rpr0410, true, true);
		else
			rpr0410_alp_enable(rpr0410, false, true);
	}
	for (i = 0; i < 32; i++) {
		ret = rpr0410_i2c_read(rpr0410, RPR0410_PS_DATA_LSB, &reg);
		pdata = reg;
		ret = rpr0410_i2c_read(rpr0410, RPR0410_PS_DATA_MSB, &reg);
		pdata |= reg << 8;
		pdata &= 0xfff;
		sum += pdata;
	}
	avg = sum >> 5;
	rpr0410->crosstalk = avg;
	rpr0410->prox_threshold_low = rpr0410->crosstalk + 10;
	rpr0410->prox_threshold_high = rpr0410->prox_threshold_low + 10;
	rpr0410_als_threshold_set(rpr0410, rpr0410->prox_threshold_low,
				  rpr0410->prox_threshold_high);
	if (prox_state) {
		if (rpr0410->power_state & LIGHT_ENABLED)
			rpr0410_alp_enable(rpr0410, true, false);
		else
			rpr0410_alp_enable(rpr0410, false, false);
		rpr0410->power_state &= ~PROXIMITY_ENABLED;
		prox_state = 0;
	}
	return ret;
}

/*sysfs interface support*/
static ssize_t rpr0410_proximity_enable_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	return sprintf(buf, "%d\n", rpr0410->power_state & PROXIMITY_ENABLED);
}

static ssize_t rpr0410_proximity_enable_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	struct rpr0410_data *rpr0410 = dev_get_drvdata(dev);
	bool new_value;
	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&rpr0410->power_lock);
	if (new_value && !(rpr0410->power_state & PROXIMITY_ENABLED)) {
		rpr0410->power_state |= PROXIMITY_ENABLED;
		if (rpr0410->power_state & LIGHT_ENABLED)
			rpr0410_alp_enable(rpr0410, true, true);
		else
			rpr0410_alp_enable(rpr0410, false, true);
		enable_irq(rpr0410->irq);
		mutex_unlock(&rpr0410->power_lock);
	} else if (!new_value && (rpr0410->power_state & PROXIMITY_ENABLED)) {
		if (rpr0410->power_state & LIGHT_ENABLED)
			rpr0410_alp_enable(rpr0410, true, false);
		else
			rpr0410_alp_enable(rpr0410, false, false);
		rpr0410->power_state &= ~PROXIMITY_ENABLED;
		disable_irq_nosync(rpr0410->irq);
		mutex_unlock(&rpr0410->power_lock);
		cancel_work_sync(&rpr0410->work_prox);
	} else
		mutex_unlock(&rpr0410->power_lock);
	return size;
}

static ssize_t rpr0410_enable_als_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	return sprintf(buf, "%d\n", rpr0410->power_state & LIGHT_ENABLED);
}

static ssize_t rpr0410_enable_als_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct rpr0410_data *rpr0410 = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&rpr0410->power_lock);
	if (new_value && !(rpr0410->power_state & LIGHT_ENABLED)) {
		rpr0410->power_state |= LIGHT_ENABLED;
		if (rpr0410->power_state & PROXIMITY_ENABLED)
			rpr0410_alp_enable(rpr0410, true, true);
		else
			rpr0410_alp_enable(rpr0410, true, false);
		mutex_unlock(&rpr0410->power_lock);
		hrtimer_start(&rpr0410->timer_light,
			      ktime_set(0, LIGHT_SENSOR_START_TIME_DELAY),
			      HRTIMER_MODE_REL);

	} else if (!new_value && (rpr0410->power_state & LIGHT_ENABLED)) {
		if (rpr0410->power_state & PROXIMITY_ENABLED)
			rpr0410_alp_enable(rpr0410, false, true);
		else
			rpr0410_alp_enable(rpr0410, false, false);
		rpr0410->power_state &= ~LIGHT_ENABLED;
		mutex_unlock(&rpr0410->power_lock);
		hrtimer_cancel(&rpr0410->timer_light);
		cancel_work_sync(&rpr0410->work_light);
	} else
		mutex_unlock(&rpr0410->power_lock);
	return size;
}

static ssize_t rpr0410_proximity_threshold_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	return sprintf(buf, "%d,%d\n", rpr0410->prox_threshold_high,
		       rpr0410->prox_threshold_low);
}

static ssize_t rpr0410_proximity_crosstalk(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	return sprintf(buf, "%d", rpr0410->crosstalk);
}

static ssize_t rpr0410_prox_cali_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	ret = rpr0410_proximity_calibration(rpr0410, 1, 100);
	if (ret < 0)
		return ret;
	return count;

}

static ssize_t rpr0410_registers_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	unsigned i, n, reg_count;
	u8 value;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	ret = 0;
	reg_count = sizeof(rpr0410_regs) / sizeof(rpr0410_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		ret = rpr0410_i2c_read(rpr0410, rpr0410_regs[i].reg, &value);
		if (ret < 0)
			return ret;
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n", rpr0410_regs[i].name, value);
	}
	return n;
}

static ssize_t rpr0410_registers_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned i, reg_count, value;
	int ret;
	char name[30];
	struct i2c_client *client = to_i2c_client(dev);
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	if (count >= 30)
		return -EFAULT;
	if (sscanf(buf, "%30s %x", name, &value) != 2) {
		pr_err("input invalid\n");
		return -EFAULT;
	}
	reg_count = sizeof(rpr0410_regs) / sizeof(rpr0410_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, rpr0410_regs[i].name)) {
			ret = rpr0410_i2c_write(rpr0410, rpr0410_regs[i].reg,
						value);
			if (ret) {
				pr_err("Failed to write register %s\n", name);
				return -EFAULT;
			}
			return count;
		}
	}
	pr_err("no such register %s\n", name);
	return -EFAULT;
}

static DEVICE_ATTR(enable_als, 0644,
		   rpr0410_enable_als_show, rpr0410_enable_als_store);
static DEVICE_ATTR(enable_ps, 0644,
		   rpr0410_proximity_enable_show,
		   rpr0410_proximity_enable_store);
static DEVICE_ATTR(crosstalk, 0444, rpr0410_proximity_crosstalk,
		   NULL);
static DEVICE_ATTR(prox_cali, 0644,
		   rpr0410_proximity_threshold_show, rpr0410_prox_cali_store);
static DEVICE_ATTR(registers, 0644, rpr0410_registers_show,
		   rpr0410_registers_store);

static struct attribute *rpr0410_attributes[] = {
	&dev_attr_enable_als.attr,
	&dev_attr_enable_ps.attr,
	&dev_attr_crosstalk.attr,
	&dev_attr_prox_cali.attr,
	&dev_attr_registers.attr,
	NULL
};

static const struct attribute_group rpr0410_attr_group = {
	.attrs = rpr0410_attributes,
};

static void long_long_divider(unsigned long long data,
			      unsigned long base_divier, unsigned long *answer,
			      unsigned long long *overplus)
{
	unsigned long long divier;
	unsigned long unit_sft;

	if ((long long)data < 0) {
		data /= 2;
		base_divier /= 2;
	}
	divier = base_divier;
	if (data > MASK_LONG) {
		unit_sft = 0;
		while (data > divier) {
			unit_sft++;
			divier = divier << 1;
		}
		while (data > base_divier) {
			if (data > divier) {
				*answer += 1 << unit_sft;
				data -= divier;
			}
			unit_sft--;
			divier = divier >> 1;
		}
		*overplus = data;
	} else {
		*answer = (unsigned long)(data & MASK_LONG) / base_divier;
		/* calculate over plus and shift 16bit */
		*overplus =
		    (unsigned long long)(data - (*answer * base_divier));
	}
}

static int calc_rohm_als_data(unsigned short data0, unsigned short data1,
			      unsigned char gain0, unsigned char gain1,
			      unsigned char time)
{
#define DECIMAL_BIT      (15)
#define JUDGE_FIXED_COEF (100)
#define MAX_OUTRANGE     (11357)
#define MAXRANGE_NMODE   (0xFFFF)
#define MAXSET_CASE      (4)

	int final_data;
	struct CALC_DATA calc_data;
	struct CALC_ANS calc_ans;
	unsigned long calc_judge;
	unsigned char set_case;
	unsigned long div_answer;
	unsigned long long div_overplus;
	unsigned long long overplus;
	unsigned long max_range;

	/* set the value of measured als data */
	calc_data.als_data0 = data0;
	calc_data.als_data1 = data1;
	calc_data.gain_data0 = gain0;

	/* set max range */
	if (calc_data.gain_data0 == 0)
		return -1;
	else
		max_range = MAX_OUTRANGE / calc_data.gain_data0;

	/* calculate data */
	if (calc_data.als_data0 == MAXRANGE_NMODE) {
		calc_ans.positive = max_range;
		calc_ans.decimal = 0;
	} else {
		/* get the value which is measured from power table */
		calc_data.als_time = time;
		if (calc_data.als_time == 0)
			/* issue error value when time is 0 */
			return -1;

		calc_judge = calc_data.als_data1 * JUDGE_FIXED_COEF;
		if (calc_judge < (calc_data.als_data0 * judge_coefficient[0]))
			set_case = 0;
		else if (calc_judge <
			 (calc_data.als_data0 * judge_coefficient[1]))
			set_case = 1;
		else if (calc_judge
			 < (calc_data.als_data0 * judge_coefficient[2]))
			set_case = 2;
		else if (calc_judge
			 < (calc_data.als_data0 * judge_coefficient[3]))
			set_case = 3;
		else
			set_case = MAXSET_CASE;

		calc_ans.positive = 0;
		if (set_case >= MAXSET_CASE)
			calc_ans.decimal = 0;
		else {
			calc_data.gain_data1 = gain1;
			if (calc_data.gain_data1 == 0)
				/* issue error value when gain is 0 */
				return -1;
			calc_data.data0 =
			    (unsigned long long)(data0_coefficient[set_case]
						 * calc_data.als_data0) *
			    calc_data.gain_data1;
			calc_data.data1 =
			    (unsigned long long)(data1_coefficient[set_case]
						 * calc_data.als_data1) *
			    calc_data.gain_data0;
			if (calc_data.data0 < calc_data.data1)
				return -1;
			else
				calc_data.data = (calc_data.data0
						  - calc_data.data1);
			calc_data.dev_unit = calc_data.gain_data0
			    * calc_data.gain_data1 * calc_data.als_time * 10;
			if (calc_data.dev_unit == 0)
				/* issue error value when dev_unit is 0 */
				return -1;

			/* calculate a positive number */
			div_answer = 0;
			div_overplus = 0;
			long_long_divider(calc_data.data, calc_data.dev_unit,
					  &div_answer, &div_overplus);
			calc_ans.positive = div_answer;
			/* calculate a decimal number */
			calc_ans.decimal = 0;
			overplus = div_overplus;
			if (calc_ans.positive < max_range) {
				if (overplus != 0) {
					overplus = overplus << DECIMAL_BIT;
					div_answer = 0;
					div_overplus = 0;
					long_long_divider(overplus,
							  calc_data.dev_unit,
							  &div_answer,
							  &div_overplus);
					calc_ans.decimal = div_answer;
				}
			}

			else
				calc_ans.positive = max_range;
		}
	}

	final_data =
	    calc_ans.positive * CUT_UNIT + ((calc_ans.decimal * CUT_UNIT)
					    >> DECIMAL_BIT);

	return final_data;

#undef DECIMAL_BIT
#undef JUDGE_FIXED_COEF
#undef MAX_OUTRANGE
#undef MAXRANGE_NMODE
#undef MAXSET_CASE
}

static unsigned int rpr0410_als_data_filter(unsigned int als_data)
{
	u16 als_level[RPR0410_ALS_LEVEL_NUM] = { 0, 50, 100, 150, 200,
		250, 300, 350, 400, 450,
		550, 650, 750, 900, 1100
	};
	u16 als_value[RPR0410_ALS_LEVEL_NUM] = { 0, 50, 100, 150, 200,
		250, 300, 350, 1000, 1200,
		1900, 2500, 7500, 15000, 25000
	};
	u8 idx;
	for (idx = 0; idx < RPR0410_ALS_LEVEL_NUM; idx++) {
		if (als_data < als_value[idx])
			break;
	}
	if (idx >= RPR0410_ALS_LEVEL_NUM) {
		pr_err("rpr0410 als data to level: exceed range.\n");
		idx = RPR0410_ALS_LEVEL_NUM - 1;
	}

	return als_level[idx];
}

static u16 rpr0410_get_als(struct rpr0410_data *rpr0410)
{
	u16 als;
	u16 data0;
	u16 data1;
	u8 reg;
	int ret;
	ret = 0;

	rpr0410_i2c_read(rpr0410, RPR0410_ALS_DATA0_LSB, &reg);
	data0 = reg;
	rpr0410_i2c_read(rpr0410, RPR0410_ALS_DATA0_MSB, &reg);
	data0 |= reg << 8;
	rpr0410_i2c_read(rpr0410, RPR0410_ALS_DATA1_LSB, &reg);
	data1 = reg;
	rpr0410_i2c_read(rpr0410, RPR0410_ALS_DATA1_MSB, &reg);
	data1 |= reg << 8;
	/* get gain */
	ret = rpr0410_i2c_read(rpr0410, RPR0410_ALS_PS_CONTROL, &reg);
	if (ret < 0)
		pr_err("[rpr0410]: read als ps control reg fail %d\n",
		       __LINE__);

	reg = (reg & RPR0410_ALS_GAIN_MASK) >> 2;
	rpr0410->als_ps_gain0 = g_agtable[reg].data0;
	rpr0410->als_ps_gain1 = g_agtable[reg].data1;
	pr_info("rpr0410: reg=%d, als_ps_gain0=%d, als_ps_gain1=%d\n", reg,
		rpr0410->als_ps_gain0, rpr0410->als_ps_gain1);
	/* get measure time */
	ret = rpr0410_i2c_read(rpr0410, RPR0410_MODE_CONTROL, &reg);
	if (ret < 0)
		pr_err("[rpr0410]: read mode control reg fail %d\n", __LINE__);
	reg = reg & RPR0410_MEASURE_TIME_MASK;
	rpr0410->als_measure_time = g_mtable[reg].als;
	als = calc_rohm_als_data(data0, data1, rpr0410->als_ps_gain0,
				 rpr0410->als_ps_gain1,
				 rpr0410->als_measure_time);
	if (als == 0)
		als++;
	/* make the caculated data to follow framework requirement */
	als = rpr0410_als_data_filter(als);
	return als;
}

static void rpr0410_work_func_light(struct work_struct *work)
{
	u16 als;
	struct rpr0410_data *rpr0410 = container_of(work, struct rpr0410_data,
						    work_light);
	als = rpr0410_get_als(rpr0410);
	pr_info("rpr0410_work_func_light als =%d\n", als);
	input_report_abs(rpr0410->light_input_dev, ABS_MISC, als);
	input_sync(rpr0410->light_input_dev);
}

/* light timer function */
static enum hrtimer_restart rpr0410_light_timer_func(struct hrtimer *timer)
{
	struct rpr0410_data *rpr0410 =
	    container_of(timer, struct rpr0410_data, timer_light);
	queue_work(rpr0410->light_wq, &rpr0410->work_light);
	hrtimer_forward_now(&rpr0410->timer_light, rpr0410->als_delay);
	return HRTIMER_RESTART;
}

static u16 proximity_get_value(struct rpr0410_data *rpr0410)
{
	int ret;
	u16 pdata;
	u8 reg;
	ret = rpr0410_i2c_read(rpr0410, RPR0410_INTERRUPT, &reg);
	if (ret < 0)
		pr_err("[rpr0410]: read interrupt err %d\n", __LINE__);
	if (reg & RPR0410_PS_INT_STATUS) {
		/*get led drive strength */
		ret = rpr0410_i2c_read(rpr0410, RPR0410_ALS_PS_CONTROL, &reg);
		if (ret < 0)
			pr_err("[rpr0410]: read als ps control reg fail %d\n",
			       __LINE__);
		rpr0410->als_ps_led = reg & RPR0410_LED_DRIVE_MASK;
		ret = rpr0410_i2c_read(rpr0410, RPR0410_PS_DATA_LSB, &reg);
		pdata = reg;
		ret = rpr0410_i2c_read(rpr0410, RPR0410_PS_DATA_MSB, &reg);
		pdata |= reg << 8;
		pdata &= 0xfff;

		return pdata;
	}
	return 0;
}

static void rpr0410_work_func_prox(struct work_struct *work)
{
	u16 pdata;
	u8 ps;
	int ret;
	struct rpr0410_data *rpr0410 = container_of(work, struct rpr0410_data,
						    work_prox);
	ret = 0;
	pdata = proximity_get_value(rpr0410);
	ps = 0;
	if (pdata > rpr0410->prox_threshold_high) {
		ps = 0;
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_LSB,
					rpr0410->prox_threshold_low & 0xff);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TL_LSB err %d\n",
			       __LINE__);
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_MSB,
					(rpr0410->
					 prox_threshold_low & 0xff00) >> 8);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TL_MSB err %d\n",
			       __LINE__);
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TH_LSB,
					rpr0410->prox_threshold_high & 0xff);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TH_LSB err %d\n",
			       __LINE__);
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_MSB,
					(rpr0410->
					 prox_threshold_high & 0xff00) >> 8);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TH_MSB err %d\n",
			       __LINE__);
	} else if (pdata < rpr0410->prox_threshold_low) {
		ps = 1;
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_LSB,
					rpr0410->prox_threshold_low & 0xff);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TL_LSB err %d\n",
			       __LINE__);
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_MSB,
					(rpr0410->
					 prox_threshold_low & 0xff00) >> 8);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TL_MSB err %d\n",
			       __LINE__);
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TH_LSB,
					rpr0410->prox_threshold_high & 0xff);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TH_LSB err %d\n",
			       __LINE__);
		ret = rpr0410_i2c_write(rpr0410, RPR0410_PS_TL_MSB,
					(rpr0410->
					 prox_threshold_high & 0xff00) >> 8);
		if (ret < 0)
			pr_err("[rpr0410]: write RPR0410_PS_TH_MSB err %d\n",
			       __LINE__);
	}

	input_report_abs(rpr0410->proximity_input_dev, ABS_DISTANCE, ps);
	input_sync(rpr0410->proximity_input_dev);
	enable_irq(rpr0410->irq);

}

static irqreturn_t rpr0410_irq_thread_fn(int irq, void *data)
{
	struct rpr0410_data *rpr0410 = data;
	disable_irq_nosync(rpr0410->irq);
	queue_work(rpr0410->prox_wq, &rpr0410->work_prox);
	return IRQ_HANDLED;
}

static int rpr0410_setup_irq(struct rpr0410_data *rpr0410)
{
	int ret = -EIO;
	int irq;
	pr_info("rpr0410_setup_irq gpio=%d\n", rpr0410->i2c_client->irq);
	ret = gpio_request(rpr0410->i2c_client->irq, "prox_int");
	gpio_direction_input(rpr0410->i2c_client->irq);
	irq = gpio_to_irq(rpr0410->i2c_client->irq);
	rpr0410->irq = irq;
	ret = request_threaded_irq(irq, NULL, &rpr0410_irq_thread_fn,
				   IRQF_TRIGGER_FALLING, "rpr0410", rpr0410);
	if (ret < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
		       __func__, irq, irq, ret);
		return ret;
	}

	/* start with interrupts disabled */
	disable_irq(rpr0410->irq);

	return ret;
}

static int rpr0410_sw_reset(struct rpr0410_data *rpr0410)
{
	int ret;
	ret = 0;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_SYSTEM_CONTROL,
				RPR0410_SW_RESET_CMD);
	if (ret < 0)
		pr_err("[rpr0410] sw reset fail\n");
	return ret;

}

static int rpr0410_int_reset(struct rpr0410_data *rpr0410)
{
	int ret;
	ret = 0;
	ret = rpr0410_i2c_write(rpr0410, RPR0410_SYSTEM_CONTROL,
				RPR0410_INT_RESET_CMD);
	if (ret < 0)
		pr_err("[rpr0410] int reset fail\n");
	return ret;
}

static int rpr0410_chip_init(struct rpr0410_data *rpr0410)
{
	int ret;
	struct rpr0410_int_cfg_t int_cfg;

	ret = 0;
	ret = rpr0410_als_ps_control_set(rpr0410, rpr0410->als_gain,
					 rpr0410->als_ps_led);
	if (ret < 0)
		pr_err("[rpr0410]fail to configure ALS_PS_CONTROL reg\n");
	ret = rpr0410_persist_set(rpr0410, rpr0410->persist);
	if (ret < 0)
		pr_err("[rpr0410]fail to configure PERSIST reg\n");
	ret = rpr0410_als_threshold_set(rpr0410, 0, 1);
	if (ret < 0)
		pr_err("[rpr0410]fail to configure als thresold\n");

	ret = rpr0410_ps_threshold_set(rpr0410, rpr0410->prox_threshold_low,
				       rpr0410->prox_threshold_high);
	if (ret < 0)
		pr_err("[rpr0410]fail to configure prox threshold\n");
	int_cfg.int_mode = rpr0410->interrupt_mode;
	int_cfg.int_assert = rpr0410->interrupt_assert;
	int_cfg.int_latch = rpr0410->interrupt_latch;
	int_cfg.int_trig = rpr0410->interrupt_trig;
	ret = rpr0410_interrupt_set(rpr0410, int_cfg);
	if (ret < 0)
		pr_err("[rpr0410]fail to set interrupt reg\n");
	return ret;

}

#ifdef CONFIG_HAS_EARLYSUSPEND
void rpr0410_early_suspend(struct early_suspend *h)
{
	struct rpr0410_data *rpr0410 = container_of(h,
						    struct rpr0410_data,
						    early_suspend);
	pm_message_t mesg = {
		.event = PM_EVENT_SUSPEND,
	};
	_rpr0410_suspend(rpr0410->i2c_client, mesg);
}

static void rpr0410_late_resume(struct early_suspend *desc)
{
	struct rpr0410_data *rpr0410 = container_of(desc,
						    struct rpr0410_data,
						    early_suspend);
	_rpr0410_resume(rpr0410->i2c_client);
}
#endif
static int rpr0410_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct rpr0410_data *rpr0410;
	struct device_node *np;
	u32 val;
	np = NULL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	rpr0410 = kzalloc(sizeof(struct rpr0410_data), GFP_KERNEL);
	if (!rpr0410) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}
	rpr0410->i2c_client = client;
	i2c_set_clientdata(client, rpr0410);

	if (!client->dev.platform_data) {
		np = rpr0410->i2c_client->dev.of_node;
		ret = of_property_read_u32(np, "gpio-irq-pin", &val);
		if (!ret)
			client->irq = val;
		else {
			pr_info("[rpr0410] gpio-irq-pin is not found in dts\n");
			client->irq = 89;
		}
		/* chip configuration */
		/*measure_time: 0x41:[3:0] */
		ret = of_property_read_u32(np, "measure_time", &val);
		if (!ret)
			rpr0410->measure_time = val;
		else {
			pr_info("[rpr0410] meature time is not found in dts\n");
			rpr0410->measure_time = 0x06;
		}
		/*als_ps_led: 0x42:[1:0] */
		ret = of_property_read_u32(np, "als_ps_led", &val);
		if (!ret)
			rpr0410->als_ps_led = val;
		else {
			pr_info("[rpr0410] prox drive is not found in dts\n");
			rpr0410->als_ps_led = 0x02;
		}
		/*als_gain: 0x42:[5:2] */
		ret = of_property_read_u32(np, "als_gain", &val);
		if (!ret)
			rpr0410->als_gain = val;
		else {
			pr_info("[rpr0410] als gain is not found in dts\n");
			rpr0410->als_gain = 0x04;
		}
		/*persist: 0x43 */
		ret = of_property_read_u32(np, "persist", &val);
		if (!ret)
			rpr0410->persist = val;
		else {
			pr_info("[rpr0410] persist is not found in dts\n");
			rpr0410->persist = 0x01;
		}
		/*interrupt mode: 0x4a:[5:4] */
		ret = of_property_read_u32(np, "interrupt_mode", &val);
		if (!ret)
			rpr0410->interrupt_mode = val;
		else {
			pr_info("[rpr0410] interrupt_mode is not in dts\n");
			rpr0410->interrupt_mode = 0x02;
		}
		/*interrupt_assert: 0x4a:[3] */
		ret = of_property_read_u32(np, "interrupt_assert", &val);
		if (!ret)
			rpr0410->interrupt_assert = val;
		else {
			pr_info("[rpr0410] interrupt_assert is not in dts\n");
			rpr0410->interrupt_assert = 0x0;
		}
		/*interrupt_latch: 0x4a:[1] */
		ret = of_property_read_u32(np, "interrupt_latch", &val);
		if (!ret)
			rpr0410->interrupt_latch = val;
		else {
			pr_info("[rpr0410] interrupt_latch is not in dts\n");
			rpr0410->interrupt_latch = 0x01;
		}
		/*interrupt_trig: 0x4a:[1:0] */
		ret = of_property_read_u32(np, "interrupt_trig", &val);
		if (!ret)
			rpr0410->interrupt_trig = val;
		else {
			pr_info("[rpr0410] interrupt_trig is not in dts\n");
			rpr0410->interrupt_trig = 0x01;
		}
		ret = of_property_read_u32(np, "prox_threshold_low", &val);
		if (!ret)
			rpr0410->prox_threshold_low = val;
		else {
			pr_info("[rpr0410] low prox threshold is not in dts\n");
			rpr0410->prox_threshold_low = 80;
		}
		ret = of_property_read_u32(np, "prox_threshold_high", &val);
		if (!ret)
			rpr0410->prox_threshold_high = val;
		else {
			pr_info("[rpr0410] high pthreshold is not in dts\n");
			rpr0410->prox_threshold_high = 90;
		}

	}

	/* wake lock init */
	wake_lock_init(&rpr0410->prox_wake_lock, WAKE_LOCK_SUSPEND,
		       "prox_wake_lock");
	mutex_init(&rpr0410->power_lock);
	mutex_init(&rpr0410->update_lock);
	ret = rpr0410_chip_init(rpr0410);

	ret = rpr0410_setup_irq(rpr0410);
	if (ret) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&rpr0410->timer_light, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rpr0410->als_delay = ns_to_ktime(500 * NSEC_PER_MSEC);
	rpr0410->timer_light.function = rpr0410_light_timer_func;

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		goto err_input_allocate_device_proximity;
	}
	rpr0410->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, rpr0410);
	input_dev->name = "proximity";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_proximity;
	}

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	rpr0410->light_wq = create_singlethread_workqueue("rpr0410_light_wq");
	if (!rpr0410->light_wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create light workqueue\n", __func__);
		goto err_create_light_workqueue;
	}
	rpr0410->prox_wq = create_singlethread_workqueue("rpr0410_prox_wq");
	if (!rpr0410->prox_wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create prox workqueue\n", __func__);
		goto err_create_prox_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&rpr0410->work_light, rpr0410_work_func_light);
	INIT_WORK(&rpr0410->work_prox, rpr0410_work_func_prox);

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(input_dev, rpr0410);
	input_dev->name = "lightsensor";
	input_set_capability(input_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(input_dev, ABS_MISC, 0, 25000, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_light;
	}
	rpr0410->light_input_dev = input_dev;
	ret = sysfs_create_group(&rpr0410->i2c_client->dev.kobj,
				 &rpr0410_attr_group);
#ifdef CONFIG_HAS_EARLYSUSPEND
	rpr0410->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	rpr0410->early_suspend.suspend = rpr0410_early_suspend;
	rpr0410->early_suspend.resume = rpr0410_late_resume;
	register_early_suspend(&rpr0410->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	goto done;

/* error, unwind it all */
err_input_register_device_light:
err_input_allocate_device_light:
	unregister_early_suspend(&rpr0410->early_suspend);
	input_unregister_device(rpr0410->light_input_dev);
	destroy_workqueue(rpr0410->prox_wq);
err_create_prox_workqueue:
	hrtimer_cancel(&rpr0410->timer_light);
	destroy_workqueue(rpr0410->light_wq);
err_create_light_workqueue:
	input_unregister_device(rpr0410->proximity_input_dev);
err_input_register_device_proximity:
err_input_allocate_device_proximity:
	free_irq(rpr0410->irq, 0);
err_setup_irq:
	mutex_destroy(&rpr0410->power_lock);
	wake_lock_destroy(&rpr0410->prox_wake_lock);
	kfree(rpr0410);
done:
	pr_info("rpr0410 probe success\n");
	return ret;
}

static int _rpr0410_suspend(struct i2c_client *client, pm_message_t mesg)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   rpr0410->power_state because we use that state in resume.
	 */
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	pr_info("_rpr0410_suspend: als/ps status = %d/%d\n",
		rpr0410->power_state & LIGHT_ENABLED,
		rpr0410->power_state & PROXIMITY_ENABLED);
	if ((rpr0410->power_state & LIGHT_ENABLED)
	    && (rpr0410->power_state & PROXIMITY_ENABLED)) {
		hrtimer_cancel(&rpr0410->timer_light);
		cancel_work_sync(&rpr0410->work_light);
		rpr0410_alp_enable(rpr0410, false, true);
	} else if ((rpr0410->power_state & LIGHT_ENABLED)
		   && !(rpr0410->power_state & PROXIMITY_ENABLED)) {
		hrtimer_cancel(&rpr0410->timer_light);
		cancel_work_sync(&rpr0410->work_light);
		cancel_work_sync(&rpr0410->work_prox);
		rpr0410_alp_enable(rpr0410, false, false);
	}
	return 0;
}

static int _rpr0410_resume(struct i2c_client *client)
{
	/* Turn power back on if we were before suspend. */
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	pr_info("_rpr0410_resume: als/ps status = %d/%d\n",
		rpr0410->power_state & LIGHT_ENABLED,
		rpr0410->power_state & PROXIMITY_ENABLED);
	if ((rpr0410->power_state & LIGHT_ENABLED)
	    && (rpr0410->power_state & PROXIMITY_ENABLED)) {
		rpr0410_alp_enable(rpr0410, true, true);
		hrtimer_start(&rpr0410->timer_light,
			      ktime_set(0, LIGHT_SENSOR_START_TIME_DELAY),
			      HRTIMER_MODE_REL);
	} else if ((rpr0410->power_state & LIGHT_ENABLED)
		   && !(rpr0410->power_state & PROXIMITY_ENABLED)) {
		rpr0410_alp_enable(rpr0410, true, false);
		hrtimer_start(&rpr0410->timer_light,
			      ktime_set(0, LIGHT_SENSOR_START_TIME_DELAY),
			      HRTIMER_MODE_REL);
	}

	return 0;
}

static int rpr0410_remove(struct i2c_client *client)
{
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	unregister_early_suspend(&rpr0410->early_suspend);
	input_unregister_device(rpr0410->light_input_dev);
	input_unregister_device(rpr0410->proximity_input_dev);
	free_irq(rpr0410->irq, NULL);
	destroy_workqueue(rpr0410->light_wq);
	destroy_workqueue(rpr0410->prox_wq);
	mutex_destroy(&rpr0410->power_lock);
	wake_lock_destroy(&rpr0410->prox_wake_lock);
	kfree(rpr0410);
	return 0;
}

static void rpr0410_shutdown(struct i2c_client *client)
{
	int ret;
	struct rpr0410_data *rpr0410 = i2c_get_clientdata(client);
	ret = rpr0410_int_reset(rpr0410);
	if (ret < 0)
		pr_err("rpr0410: shutdown int reset fail\n");
	ret = rpr0410_sw_reset(rpr0410);
	if (ret < 0)
		pr_err("rpr0410: shutdown fail with %d\n", ret);
}

static const struct i2c_device_id rpr0410_device_id[] = {
	{"rpr0410", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, rpr0410_device_id);

static const struct of_device_id rpr0410_of_match[] = {
	{.compatible = "bcm,rpr0410",},
	{},
};

MODULE_DEVICE_TABLE(of, rpr0410_of_match);

static struct i2c_driver rpr0410_i2c_driver = {
	.driver = {
		   .name = "rpr0410",
		   .owner = THIS_MODULE,
		   .of_match_table = rpr0410_of_match,
		   },
	.probe = rpr0410_probe,
	.remove = rpr0410_remove,
	.id_table = rpr0410_device_id,
	.shutdown = rpr0410_shutdown,
};

static int __init rpr0410_init(void)
{
	return i2c_add_driver(&rpr0410_i2c_driver);
}

static void __exit rpr0410_exit(void)
{
	i2c_del_driver(&rpr0410_i2c_driver);
}

module_init(rpr0410_init);
module_exit(rpr0410_exit);

MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("Rohm Ambient Light and Proximity Sensor Driver");
MODULE_LICENSE("GPL v2");
