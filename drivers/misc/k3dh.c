/*
 *  STMicroelectronics k3dh acceleration sensor driver
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
//#include <mach/common.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#if defined (CONFIG_SENSORS_ACC_16BIT)
#include <linux/k2hh.h>
#else
#include <linux/k3dh.h>
#endif

#include <linux/k3dh_dev.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/sensors_core.h>

#define K3DH_DEBUG 0

#if K3DH_DEBUG
#define ACCDBG(fmt, args...) printk(KERN_INFO fmt, ## args)
#else
#define ACCDBG(fmt, args...)
#endif

#define ACC_DEV_MAJOR 241
#define K3DH_RETRY_COUNT	3

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"
#define CALIBRATION_DATA_AMOUNT	20

#define ACC_ENABLED 1

#define G_MAX			7995148 /* (SENSITIVITY_8G*(2^15-1)) */
#define G_MIN			- 7995392 /* (-SENSITIVITY_8G*(2^15)   */
#define FUZZ			0
#define FLAT			0

/* For movement recognition*/
#if defined (CONFIG_SENSORS_ACC_16BIT)
#define DEFAULT_INTERRUPT_SETTING       0x2A /* INT1 XH,YH,ZH : enable */
#define DEFAULT_CTRL3_SETTING           0x08 /* INT enable */
#else
#define DEFAULT_INTERRUPT_SETTING       0x0A /* INT1 XH,YH : enable */
#define DEFAULT_CTRL3_SETTING           0x60 /* INT enable */
#endif

#define DEFAULT_INTERRUPT2_SETTING      0x20 /* INT2 ZH enable */
#define DEFAULT_THRESHOLD               0x7F /* 2032mg (16*0x7F) */
#define DYNAMIC_THRESHOLD               300     /* mg */
#define DYNAMIC_THRESHOLD2              700     /* mg */
#define MOVEMENT_DURATION               0x00 /*INT1 (DURATION/odr)ms*/
enum {  
        OFF = 0,
        ON = 1
};
#define ABS(a)          (((a) < 0) ? -(a) : (a))
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))

/* K3DH acceleration data */
struct k3dh_acc {
	s16 x;
	s16 y;
	s16 z;
};

struct k3dh_data {
	struct i2c_client *k3dh_client;
	struct input_dev *input;
    	struct mutex value_mutex;
	struct mutex enable_mutex;
    	struct k3dh_acc value;
	struct k3dh_acc cal_data;
	atomic_t enable;
	atomic_t delay;
    	struct delayed_work work;    
        s8 orientation[9];
	int movement_recog_flag;
	unsigned char interrupt_state;
	struct wake_lock reactive_wake_lock;

	struct device *k3dh_device;
	struct k3dh_platform_data *pdata;
	int irq;
	unsigned int irq_gpio;   
};

static int k3dh_acc_i2c_read(struct i2c_client *client, char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < K3DH_RETRY_COUNT; i++) {
		if (i2c_transfer(client->adapter, msgs, 2) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= K3DH_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, K3DH_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int k3dh_acc_i2c_write(struct i2c_client *client, char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < K3DH_RETRY_COUNT; i++) {
		if (i2c_transfer(client->adapter, msg, 1) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= K3DH_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, K3DH_RETRY_COUNT);
		return -EIO;
	}
	return 0;
}


/* Read X,Y and Z-axis acceleration raw data */
static int k3dh_read_accel_raw_xyz(struct i2c_client *client, struct k3dh_acc *acc)
{
    	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	int err=0;
	unsigned char acc_data[6] = {0};
	s16 temp;

	acc_data[0] = OUT_X_L | AC; /* read from OUT_X_L to OUT_Z_H by auto-inc */
	err = k3dh_acc_i2c_read(k3dh->k3dh_client, acc_data, 6);
	if (err < 0){
		pr_err("k3dh_read_accel_raw_xyz() failed\n");
		return err;
	}

	acc->x = (acc_data[1] << 8) | acc_data[0];
	acc->y = (acc_data[3] << 8) | acc_data[2];
	acc->z = (acc_data[5] << 8) | acc_data[4];

#if !defined(CONFIG_SENSORS_ACC_16BIT)
	acc->x = acc->x >> 4;
	acc->y = acc->y >> 4;
	acc->z = acc->z >> 4;
#endif

	if (k3dh->orientation[0]) {
		acc->x *= k3dh->orientation[0];
		acc->y *= k3dh->orientation[4];
	} else {
		temp = acc->x*k3dh->orientation[1];
		acc->x = acc->y*k3dh->orientation[3];
		acc->y = temp;
	}
	acc->z *= k3dh->orientation[8];

	return 0;
}

static int k3dh_read_accel_xyz(struct i2c_client *client, struct k3dh_acc *acc)
{
    	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	int err = 0;

	err = k3dh_read_accel_raw_xyz(k3dh->k3dh_client, acc);
	if (err < 0) {
		pr_err("k3dh_read_accel_xyz() failed\n");
		return err;
	}
	acc->x -= k3dh->cal_data.x;
	acc->y -= k3dh->cal_data.y;
	acc->z -= k3dh->cal_data.z;

	mutex_lock(&k3dh->value_mutex);
	k3dh->value.x = acc->x;
	k3dh->value.y = acc->y;
	k3dh->value.z = acc->z;
	mutex_unlock(&k3dh->value_mutex);

	return err;
}

static void k3dh_set_mode(struct i2c_client *client, unsigned char Mode)
{
    struct k3dh_data *k3dh = i2c_get_clientdata(client);
    unsigned char acc_data[2] = {0};

    printk(KERN_INFO "[K3DH] k3dh_set_mode : mode=%d\n", Mode);
    
    if(Mode)
    {
#if defined(CONFIG_SENSORS_ACC_16BIT)
        acc_data[0] = CTRL_REG1;   /*0x20*/
        acc_data[1] = K2HH_DEFAULT_POWER_ON_SETTING | CTRL_REG1_HR;
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0) {
            printk(KERN_ERR "[K3DH][%s] Change to Normal Mode(CTRL_REG1) is failed\n",__FUNCTION__);
        }
        acc_data[0] = CTRL_REG4;   /*0x23*/
        acc_data[1] = K2HH_DEFAULT_BW_FS;   //BW [2:1]=>11:50Hz, FS[1:0]=>00:+-2g, ODR=>1:according to BW, IF_ADD_INC is default 1
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0) {
             printk(KERN_ERR "[K3DH][%s] Changing BW (CTRL_REG4) is failed\n",__FUNCTION__);
        }         
#else 	     
        acc_data[0] = CTRL_REG1;    /*0x20*/
        acc_data[1] = DEFAULT_POWER_ON_SETTING;
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0) {
            printk(KERN_ERR "[K3DH][%s] Change to Normal Mode(CTRL_REG1) is failed\n",__FUNCTION__);
        }
        acc_data[0] = CTRL_REG4;    /*0x23*/
        acc_data[1] = CTRL_REG4_HR;
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0) {
            printk(KERN_ERR "[K3DH][%s] Change to Normal Mode(CTRL_REG4) is failed\n",__FUNCTION__);
        }
#endif
    }
    else
    {
	if (k3dh->movement_recog_flag == ON) {
            acc_data[0] = CTRL_REG1;
            acc_data[1] = LOW_PWR_MODE;
            if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0)
                printk(KERN_ERR "[K3DH][%s] Change to Low Power Mode is failed\n",__FUNCTION__);
	} 
        else {
            acc_data[0] = CTRL_REG1;
            acc_data[1] = PM_OFF;
            if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0)
                printk(KERN_ERR "[K3DH][%s] Change to Suspend Mode is failed\n",__FUNCTION__);  
	}            
    }

}

#if defined(CONFIG_SENSORS_HSCDTD006A) || defined(CONFIG_SENSORS_HSCDTD008A)
static struct k3dh_data * g_k3dh=NULL;
static atomic_t flgEna;
static atomic_t delay;
int accsns_get_acceleration_data(int *xyz)
{
	struct k3dh_acc acc;

	if(g_k3dh == NULL) return 1;

        k3dh_read_accel_xyz(g_k3dh->k3dh_client, &acc);

        xyz[0] = (int)(acc.x);
        xyz[1] = (int)(acc.y);
        xyz[2] = (int)(acc.z);

	ACCDBG("[K3DH] Acc_I2C, x:%d, y:%d, z:%d\n", xyz[0], xyz[1], xyz[2]);

	return 0;
}

void accsns_activate(int flgatm, int flg, int dtime)
{
    int pre_enable;

    if(g_k3dh == NULL) return;
    
    pre_enable = atomic_read(&g_k3dh->enable);

    printk(KERN_INFO "[K3DH] accsns_activate : flgatm=%d, flg=%d, dtime=%d\n", flgatm, flg, dtime);

    if (flg != 0) flg = 1;

    //Power modes
    if (flg == 0) //sleep
    {
	if (pre_enable == 1) {
            k3dh_set_mode(g_k3dh->k3dh_client, 0);
            atomic_set(&g_k3dh->enable, 0);
	}
    }
    else
    {
        if (pre_enable == 0) {
            k3dh_set_mode(g_k3dh->k3dh_client, 1);
            printk(KERN_INFO "[K3DH] accsns_activate : (%d,%d,%d)\n",
            g_k3dh->cal_data.x, g_k3dh->cal_data.y, g_k3dh->cal_data.z);
            atomic_set(&g_k3dh->enable, 1);
        }
    }
    mdelay(2);

    if (flgatm) {
        atomic_set(&flgEna, flg);
        atomic_set(&delay, dtime);
    }
}
EXPORT_SYMBOL(accsns_get_acceleration_data);
EXPORT_SYMBOL(accsns_activate);
#endif

static ssize_t k3dh_raw_data_show(struct device *dev, 
        struct device_attribute *attr, char *buf)
{
       	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	struct k3dh_acc acc;

	if (!atomic_read(&k3dh->enable))
	{
	    k3dh_read_accel_xyz(k3dh->k3dh_client, &acc);        
	}
        else
        {
            mutex_lock(&k3dh->value_mutex);
            acc.x = k3dh->value.x;
            acc.y = k3dh->value.y;
            acc.z = k3dh->value.z;
            mutex_unlock(&k3dh->value_mutex);
        }

	return sprintf(buf,"%d,%d,%d\n", acc.x, acc.y, acc.z );

}
static DEVICE_ATTR(raw_data, S_IRUGO, k3dh_raw_data_show, NULL);

static int k3dh_open_calibration(struct i2c_client *client)
{
    	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
            printk(KERN_INFO "[K3DH] %s: no calibration file\n", __func__);
            err = PTR_ERR(cal_filp);
            if (err != -ENOENT)
                pr_err("%s: Can't open calibration file= %d\n", __func__, err);
            set_fs(old_fs);
            return err;
	}

        if (cal_filp && cal_filp->f_op && cal_filp->f_op->read){
            int ret=0;
            ret = cal_filp->f_op->read(cal_filp, (char *)(&k3dh->cal_data), 3 * sizeof(s16), &cal_filp->f_pos);
            if (ret != 3 * sizeof(s16)) {
                pr_err("%s: Can't read the cal data from file= %d\n", __func__, ret);
                err = -EIO;
            }
            printk(KERN_INFO "%s: (%d,%d,%d)\n", __func__, k3dh->cal_data.x,
	        			 k3dh->cal_data.y, k3dh->cal_data.z);
        }

        if(cal_filp)
            filp_close(cal_filp, NULL);
	set_fs(old_fs);

	return err;
}

static int k3dh_do_calibration(struct i2c_client *client, int enable)
{
    	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	struct k3dh_acc data = { 0, };
	struct file *cal_filp = NULL;
	int sum[3] = { 0, };
	int err = 0;
	int i;
	mm_segment_t old_fs;

	for (i = 0; i < CALIBRATION_DATA_AMOUNT; i++) {

            err = k3dh_read_accel_raw_xyz(k3dh->k3dh_client, &data);
            if (err < 0) {
                pr_err("%s: k3dh_read_accel_raw_xyz() failed in the %dth loop\n", __func__, i);
                return err;
            }

            sum[0] += data.x;
            sum[1] += data.y;
            sum[2] += data.z;

            ACCDBG("[K3DH] calibration sum data (%d,%d,%d)\n", sum[0], sum[1], sum[2]);
	}

	if (enable) {
        	k3dh->cal_data.x = sum[0] / CALIBRATION_DATA_AMOUNT; 
        	k3dh->cal_data.y = sum[1] / CALIBRATION_DATA_AMOUNT; 

                if(sum[2] >= 0) {
#if defined(CONFIG_SENSORS_ACC_16BIT)
                    k3dh->cal_data.z = (sum[2] / CALIBRATION_DATA_AMOUNT) - 16384;     //K2HH(16bit) 16384 +- 3082
#else 
                    k3dh->cal_data.z = (sum[2] / CALIBRATION_DATA_AMOUNT) - 1024;       //K3DH(12bit) 1024 +-226
#endif
                } else {
#if defined(CONFIG_SENSORS_ACC_16BIT)
		    k3dh->cal_data.z = (sum[2] / CALIBRATION_DATA_AMOUNT) + 16384;      //K2HH(16bit) 16384 +- 3082
#else 
                    k3dh->cal_data.z = (sum[2] / CALIBRATION_DATA_AMOUNT) + 1024;       //K3DH(12bit) 1024 +-226,
#endif
                }

	} else {
		k3dh->cal_data.x = 0;
		k3dh->cal_data.y = 0;
		k3dh->cal_data.z = 0;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_CREAT |O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cal_filp)) {
            err = PTR_ERR(cal_filp);
            pr_err("%s: Can't open calibration file= %d\n", __func__, err);
            set_fs(old_fs);
            return err;
	}

        if (cal_filp && cal_filp->f_op && cal_filp->f_op->write){
            int ret=0;
            ret = cal_filp->f_op->write(cal_filp,(char *)(&k3dh->cal_data), 3 * sizeof(s16), &cal_filp->f_pos);
            if (ret != 3 * sizeof(s16)) {
                pr_err("%s: Can't write the cal data to file = %d\n", __func__, ret);
                err = -EIO;
            }
        }
        else{
                pr_err("%s: (cal_filp && cal_filp->f_op && cal_filp->f_op->write)= 0\n", __func__);
                err = -EIO;
        }

        if(cal_filp)
            filp_close(cal_filp, NULL);
	set_fs(old_fs);

	return err;
}

static ssize_t accel_calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
       	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	int count = 0;
        int isCalibration = 0;

	printk(KERN_INFO "[K3DH] cal_data : %d %d %d\n", k3dh->cal_data.x, k3dh->cal_data.y, k3dh->cal_data.z);

        if(k3dh->cal_data.x == 0 && k3dh->cal_data.y == 0 && k3dh->cal_data.z == 0)
            isCalibration = 0;
        else
            isCalibration = 1;

	count= sprintf(buf, "%d\n", isCalibration);
	return count;
}

static ssize_t accel_calibration_store(struct device *dev,  
        struct device_attribute *attr,  const char *buf, size_t size)
{
    	struct i2c_client *client = to_i2c_client(dev);
	int err;
	int enable = 0;

	err = kstrtoint(buf, 10, &enable);

	if (err) {
		pr_err("ERROR: %s got bad char\n", __func__);
		return -EINVAL;
	}

	err = k3dh_do_calibration(client, enable);
	if (err < 0) {
		pr_err("%s: failed\n", __func__);
		return err;
	}

	return size;
}
static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP, 
    accel_calibration_show, 
    accel_calibration_store);

static ssize_t accel_calibration_open(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    	struct i2c_client *client = to_i2c_client(dev);
	k3dh_open_calibration(client);
        return 1;
}
static DEVICE_ATTR(accel_cal_open, S_IRUGO,  accel_calibration_open,NULL);

static ssize_t k3dh_accel_reactive_alert_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	int onoff = OFF, err = 0, ctrl_reg = 0;
	bool factory_test = false;
	struct k3dh_acc raw_data;
	u8 thresh1 = 0, thresh2 = 0;
	unsigned char acc_data[2] = {0};

	if (sysfs_streq(buf, "1"))
		onoff = ON;
	else if (sysfs_streq(buf, "0"))
		onoff = OFF;
	else if (sysfs_streq(buf, "2")) {
		onoff = ON;
                k3dh->movement_recog_flag = OFF;        
		factory_test = true;
		printk(KERN_INFO "[K3DH] [%s] factory_test = %d\n", __FUNCTION__, factory_test);
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (onoff == ON && k3dh->movement_recog_flag == OFF) {
        
		printk(KERN_INFO "[K3DH] [%s] reactive alert is on.\n", __FUNCTION__);
        
		k3dh->interrupt_state = 0; /* Init interrupt state.*/

		if (!atomic_read(&k3dh->enable)) {
			acc_data[0] = CTRL_REG1;
			acc_data[1] = FASTEST_MODE;
			if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
				printk(KERN_ERR "[%s] Change to Fastest Mode is failed\n",__FUNCTION__);
				ctrl_reg = CTRL_REG1;
				goto err_i2c_write;
			}

			/* trun on time, T = 7/odr ms */
			usleep_range(10000, 10000);
		}        

		/* Get x, y, z data to set threshold1, threshold2. */
		err = k3dh_read_accel_xyz(k3dh->k3dh_client, &raw_data);
		printk(KERN_INFO "[K3DH] [%s] raw x=%d, y=%d, z=%d\n", __FUNCTION__, raw_data.x, raw_data.y, raw_data.z);
        
		if (err < 0) {
			pr_err("%s: k3dh_accel_read_xyz failed\n",	__func__);
			goto exit;
		}
		if (!atomic_read(&k3dh->enable)) {
			acc_data[0] = CTRL_REG1;
			acc_data[1] = LOW_PWR_MODE; /* Change to 50Hz*/
			if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
				printk(KERN_ERR "[%s] Change to Low Power Mode is failed\n",__FUNCTION__);
				ctrl_reg = CTRL_REG1;
				goto err_i2c_write;
			}
		}
        
		/* Change raw data to threshold value & settng threshold */      
#if defined(CONFIG_SENSORS_ACC_16BIT) 
		if (factory_test == true) {
			thresh1 = 0;
			thresh2 = 0;            
		}
                else {
                        /* 2G : (2000/256 = 7.8125mg) */
        		thresh1 = (MAX(ABS(raw_data.x), ABS(raw_data.y))*1000/16384+DYNAMIC_THRESHOLD)*256/2000;
			thresh2 = (ABS(raw_data.z)*1000/16384 + DYNAMIC_THRESHOLD2) * 256/2000;
		}

		acc_data[0] = IG_THS_X1;
		acc_data[1] = thresh1;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
			ctrl_reg = IG_THS_X1;
			goto err_i2c_write;
		}
		acc_data[0] = IG_THS_Y1;
		acc_data[1] = thresh1;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		        ctrl_reg = IG_THS_Y1;
		        goto err_i2c_write;
		}
		acc_data[0] = IG_THS_Z1;
		acc_data[1] = thresh2;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		        ctrl_reg = IG_THS_Z1;
		        goto err_i2c_write;
		}
#else
		if (factory_test == true) {
			thresh1 = 0;
			thresh2 = 0;            
		}
                else {
        		thresh1 = (MAX(ABS(raw_data.x), ABS(raw_data.y)) + DYNAMIC_THRESHOLD) / 16;
			thresh2 = (ABS(raw_data.z) + DYNAMIC_THRESHOLD2) /16;                
		}

		acc_data[0] = INT1_THS;
		acc_data[1] = thresh1;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
			ctrl_reg = INT1_THS;
			goto err_i2c_write;
		}
		acc_data[0] = INT2_THS;
		acc_data[1] = thresh2;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
			ctrl_reg = INT2_THS;	
			goto err_i2c_write;
		}        
#endif
		printk(KERN_INFO "[K3DH] [%s] threshold1=0x%x, threshold2=0x%x\n",  __FUNCTION__, thresh1, thresh2);

		/* INT enable */
		acc_data[0] = CTRL_REG3; /*0x22*/
		acc_data[1] = DEFAULT_CTRL3_SETTING;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
			ctrl_reg = CTRL_REG3;
			goto err_i2c_write;
		}

		k3dh->movement_recog_flag = ON;
        	
		enable_irq(k3dh->irq);
        	printk(KERN_INFO "[K3DH] enable_irq IRQ_NO:%d\n",k3dh->irq);
                enable_irq_wake(k3dh->irq);
        
	} else if (onoff == OFF && k3dh->movement_recog_flag == ON) {
	
		printk(KERN_INFO "[K3DH] [%s] reactive alert is off.\n", __FUNCTION__);

		disable_irq_nosync(k3dh->irq);
        	printk(KERN_INFO "[K3DH] disable_irq IRQ_NO:%d\n",k3dh->irq);
                disable_irq_wake(k3dh->irq);
            
		/* INT disable */
		acc_data[0] = CTRL_REG3;
		acc_data[1] = PM_OFF;
		if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
			ctrl_reg = CTRL_REG3;
			goto err_i2c_write;
		}

		k3dh->movement_recog_flag = OFF;
        	k3dh->interrupt_state = 0; /* Init interrupt state.*/
            
	}
	return count;
err_i2c_write:
	pr_err("%s: i2c write ctrl_reg = 0x%d failed(err=%d)\n", __func__, ctrl_reg, err);
exit:
	return ((err < 0) ? err : -err);
}

static ssize_t k3dh_accel_reactive_alert_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
    
	return sprintf(buf, "%d\n", k3dh->interrupt_state);
}

static irqreturn_t k3dh_accel_interrupt_thread(int irq, void *dev)
{
    	struct k3dh_data *k3dh = dev;
        unsigned char acc_data[2] = {0};

	/* INT disable */
	acc_data[0] = CTRL_REG3;
	acc_data[1] = PM_OFF;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write ctrl_reg3 failed\n",__FUNCTION__);
	}

	wake_lock_timeout(&k3dh->reactive_wake_lock, msecs_to_jiffies(3000));
	printk(KERN_INFO "[K3DH] [%s] called\n", __FUNCTION__);
    	k3dh->interrupt_state = 1;
	return IRQ_HANDLED;
}
static ssize_t k3dh_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	struct k3dh_acc acc;
	unsigned char acc_data[2] = {0};
	unsigned char temp[8] = {0};
        int i;
	int result = 1;
        int ret = 0;
        s32 NO_ST[3] = {0, 0, 0};
        s32 ST[3] = {0, 0, 0};

	acc_data[0] = CTRL_REG1;   /*0x20*/
        acc_data[1] = 0x3f;
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] Change to Normal Mode is failed\n",__FUNCTION__);
        }
	acc_data[0] = CTRL_REG4;   /*0x23*/
        acc_data[1] = 0x04;  
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] CTRL_REG4 set is failed\n",__FUNCTION__);
	}
	acc_data[0] = CTRL_REG5;
        acc_data[1] = 0x00; 
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] Selftest set is failed\n",__FUNCTION__);
	}
	acc_data[0] = CTRL_REG6;
        acc_data[1] = 0x00; 
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] CTRL_REG6 set is failed\n",__FUNCTION__);
	}
        mdelay(80);
	
	k3dh_read_accel_xyz(k3dh->k3dh_client, &acc);	

	for (i = 0; i < 5; i++) {
		while(1) {
			temp[0] = STATUS; 
			if (k3dh_acc_i2c_read(k3dh->k3dh_client, temp, 1) < 0) {
				 pr_err("[%s] i2c read error\n",__FUNCTION__);
				 goto exit_status_err;
		
			}
			if (temp[0] & 0x08)
				break;
		}
		k3dh_read_accel_xyz(k3dh->k3dh_client, &acc);
		NO_ST[0] += acc.x;
		NO_ST[1] += acc.y;
		NO_ST[2] += acc.z;
	}
	NO_ST[0] /= 5;
	NO_ST[1] /= 5;
	NO_ST[2] /= 5;

	acc_data[0] = CTRL_REG5;
	acc_data[1] = CTRL_REG5_ST1;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] Selftest ST1 set is failed\n",__FUNCTION__);
	}
	mdelay(80);

	k3dh_read_accel_xyz(k3dh->k3dh_client, &acc);
	
	for (i = 0; i < 5; i++) {
                while(1) {
		       temp[0] = STATUS;  
	               if (k3dh_acc_i2c_read(k3dh->k3dh_client, temp, 1) < 0) {
	                       pr_err("[%s] i2c read error",__FUNCTION__);
	                       goto exit_status_err;
	               }
	               if (temp[0] & 0x08)
	                        break;
	        }
		k3dh_read_accel_xyz(k3dh->k3dh_client, &acc);
                ST[0] += acc.x;
                ST[1] += acc.y;
                ST[2] += acc.z;
        }
        ST[0] /= 5;
        ST[1] /= 5;
        ST[2] /= 5;
	
	for (i = 0; i < 3; i++) {
		ST[i] -= NO_ST[i];
		ST[i] = abs(ST[i]);
		if ((SELF_TEST_2G_MIN_LSB > ST[i]) \
		        || (ST[i] > SELF_TEST_2G_MAX_LSB)) {
		        pr_info("[%s] : %d Out of range!! (%d)\n",
		                __FUNCTION__, i, ST[i]);
		        result = 0;
		}
	}

	printk(KERN_INFO "[K3DH][%s] self_test = %d,%d,%d\n", __FUNCTION__, ST[0], ST[1], ST[2]);

	if (result) {
	        ret = sprintf(buf, "1,%d,%d,%d\n", ST[0], ST[1], ST[2]);
	} else {
	        ret = sprintf(buf, "0,%d,%d,%d\n", ST[0], ST[1], ST[2]);
	}

	goto exit;

exit_status_err:
	ret = sprintf(buf, "0,0,0,0\n");
exit:	
        acc_data[0] = CTRL_REG1;
	acc_data[1] = 0x00;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] Change to Normal Mode is failed\n",__FUNCTION__);
	}
        acc_data[0] = CTRL_REG5;
	acc_data[1] = 0x00;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] Selftest set is failed\n",__FUNCTION__);
	}

        if (atomic_read(&k3dh->enable) == OFF) {
                k3dh_set_mode(k3dh->k3dh_client, 0);
        } else {
                k3dh_set_mode(k3dh->k3dh_client, 1);
        }

	return ret;
}

static DEVICE_ATTR(selftest, S_IRUGO, k3dh_selftest_show, NULL);
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
        k3dh_accel_reactive_alert_show,
        k3dh_accel_reactive_alert_store);

static struct device_attribute *accelerometer_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
	&dev_attr_accel_cal_open,	
	&dev_attr_reactive_alert,
	&dev_attr_selftest,
	NULL,
};

/////////////////////////////////////////////////////////////////////////////////////

static ssize_t poll_delay_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&k3dh->delay));
}


static ssize_t poll_delay_store(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);

	printk(KERN_INFO "[K3DH] %s : delay = %ld\n", __func__, data);

	if (error)
		return error;
    
	atomic_set(&k3dh->delay, (unsigned int) data);
	return size;
}
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP, 
    poll_delay_show, poll_delay_store);

static void k3dh_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&k3dh->enable);

	mutex_lock(&k3dh->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			k3dh_set_mode(k3dh->k3dh_client, 1);
			schedule_delayed_work(&k3dh->work,
				msecs_to_jiffies(atomic_read(&k3dh->delay)));
			atomic_set(&k3dh->enable, 1);
		}

	} else {
		if (pre_enable == 1) {
			k3dh_set_mode(k3dh->k3dh_client, 0);
			cancel_delayed_work_sync(&k3dh->work);
			atomic_set(&k3dh->enable, 0);
		}
	}
	mutex_unlock(&k3dh->enable_mutex);
}

static ssize_t acc_enable_show(struct device *dev, 
        struct device_attribute *attr, char *buf)
{
    	struct i2c_client *client = to_i2c_client(dev);
	struct k3dh_data *k3dh = i2c_get_clientdata(client);    

    return sprintf(buf, "%d\n", atomic_read(&k3dh->enable));
}

static ssize_t acc_enable_store(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long data;
	int error;

	error = strict_strtoul(buf, 10, &data);

	printk(KERN_INFO "[K3DH] %s : enable = %ld\n", __func__, data);

	if (error)
		return error;
	if ((data == 0) || (data == 1))
		k3dh_set_enable(dev, data);

	return size;
}

static struct device_attribute dev_attr_acc_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       acc_enable_show, acc_enable_store);

static struct attribute *acc_sysfs_attrs[] = {
	&dev_attr_acc_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group acc_attribute_group = {
	.attrs = acc_sysfs_attrs,
};

///////////////////////////////////////////////////////////////////////////////////

static void k3dh_work_func(struct work_struct *work)
{
    	struct k3dh_data *k3dh = container_of((struct delayed_work *)work,
			struct k3dh_data, work);
    	struct k3dh_acc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&k3dh->delay));

        k3dh_read_accel_xyz(k3dh->k3dh_client, &acc);

	input_report_abs(k3dh->input, ABS_X, acc.x);
	input_report_abs(k3dh->input, ABS_Y, acc.y);
	input_report_abs(k3dh->input, ABS_Z, acc.z);
	input_sync(k3dh->input);

	schedule_delayed_work(&k3dh->work, delay);
}

static int k3dh_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct k3dh_data *k3dh;
	struct input_dev *input_dev;
    	struct k3dh_platform_data *platform_data;
	int err=0;
    	int ii = 0;
        int tempvalue;        
        unsigned char acc_data[2] = {0};

	printk(KERN_INFO "[K3DH] %s\n",__FUNCTION__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	k3dh = kzalloc(sizeof(struct k3dh_data), GFP_KERNEL);
	if (!k3dh) {
		dev_err(&client->dev, "failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit;
	}

	printk(KERN_INFO "[K3DH] [%s] slave addr = %x\n", __func__, client->addr);

        platform_data = client->dev.platform_data;
	/* read chip id */
	tempvalue = WHO_AM_I;
	err = i2c_master_send(client, (char*)&tempvalue, 1);
	if(err < 0) {
		printk(KERN_ERR "k3dh_probe : i2c_master_send [%d]\n", err);
	}

	err = i2c_master_recv(client, (char*)&tempvalue, 1);
	if(err < 0) {
		printk(KERN_ERR "k3dh_probe : i2c_master_recv [%d]\n", err);
	}

	if((tempvalue&0x00FF) == 0x0033) {  // changed for K3DM.
		printk(KERN_INFO "[K3DH] K3DH or K2DH detected 0x%x!\n", tempvalue);
	}
	else if((tempvalue&0x00FF) == 0x0041) { // changed for K3DM.
		 printk(KERN_INFO "[K3DH] K2HH detected 0x%x!\n", tempvalue);
	}
	else {
		printk(KERN_ERR "[K3DH] No device 0x%x!\n", tempvalue);
		goto kfree_exit;
        }
	i2c_set_clientdata(client, k3dh);
	k3dh->k3dh_client = client;

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		printk(KERN_ERR "%s: could not allocate input device\n", __func__);
		err = -ENOMEM;
		goto err_input_allocate_device;
	}
	input_dev->name = "k3dh";
	input_dev->id.bustype = BUS_I2C;    

	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, G_MIN, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Y, G_MIN, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Z, G_MIN, G_MAX, FUZZ, FLAT);

	input_set_drvdata(input_dev, k3dh);

	err = input_register_device(input_dev);
	if (err < 0) {
		printk(KERN_ERR "%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device;
	}
	k3dh->input = input_dev;

	printk(KERN_INFO "[K3DH] registering sensor-level input device\n");
    
	err = sysfs_create_group(&input_dev->dev.kobj,&acc_attribute_group);
	if (err) {
		printk(KERN_ERR "Creating k3dh attribute group failed");
		goto error_device;
	}

	/* initialized sensor orientation */
        for (ii = 0; ii < 9; ii++){
        	k3dh->orientation[ii] = platform_data->orientation[ii];
        }

	/* initialized sensor cal data */
        k3dh->cal_data.x=0;
        k3dh->cal_data.y=0;
        k3dh->cal_data.z=0;        
	k3dh->pdata = k3dh->k3dh_client->dev.platform_data;
	k3dh->movement_recog_flag = OFF;

	INIT_DELAYED_WORK(&k3dh->work, k3dh_work_func);
	atomic_set(&k3dh->delay, 200);
	atomic_set(&k3dh->enable, 0);
    	mutex_init(&k3dh->value_mutex);
    	mutex_init(&k3dh->enable_mutex);

	/* wake lock init for accelerometer sensor */
	wake_lock_init(&k3dh->reactive_wake_lock, WAKE_LOCK_SUSPEND, "reactive_wake_lock");

#if defined(CONFIG_SENSORS_ACC_16BIT)
	acc_data[0] = IG_THS_X1;
	acc_data[1] = DEFAULT_THRESHOLD;
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_ths failed\n",__FUNCTION__);
        }
	acc_data[0] = IG_THS_Y1;
        acc_data[1] = DEFAULT_THRESHOLD;
        if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
                printk(KERN_ERR "[%s] i2c write int1_ths failed\n",__FUNCTION__);
        }
	acc_data[0] = IG_THS_Z1;
	acc_data[1] = DEFAULT_THRESHOLD;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
	        printk(KERN_ERR "[%s] i2c write int1_ths failed\n",__FUNCTION__);
	}    
	acc_data[0] = IG_DUR1;
	acc_data[1] = MOVEMENT_DURATION;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_duration failed\n",__FUNCTION__);
	}    
	acc_data[0] = IG_CFG1; /*0x30*/
	acc_data[1] = DEFAULT_INTERRUPT_SETTING;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_cfg failed\n",__FUNCTION__);
	}  
#else    
	acc_data[0] = INT1_THS;
	acc_data[1] = DEFAULT_THRESHOLD;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_ths failed\n",__FUNCTION__);
	}
	acc_data[0] = INT1_DURATION;
	acc_data[1] = MOVEMENT_DURATION;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_duration failed\n",__FUNCTION__);
	}
	acc_data[0] = INT1_CFG; /*0x30*/
	acc_data[1] = DEFAULT_INTERRUPT_SETTING;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_cfg failed\n",__FUNCTION__);
	}
	acc_data[0] = INT2_THS;
	acc_data[1] = DEFAULT_THRESHOLD;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int2_ths failed\n",__FUNCTION__);
	}
	acc_data[0] = INT2_DURATION;
	acc_data[1] = MOVEMENT_DURATION;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write int1_duration failed\n",__FUNCTION__);
	}
	acc_data[0] = INT2_CFG;
	acc_data[1] = DEFAULT_INTERRUPT2_SETTING;
	if(k3dh_acc_i2c_write(k3dh->k3dh_client, acc_data, 2) !=0){
		printk(KERN_ERR "[%s] i2c write ctrl_reg3 failed\n",__FUNCTION__);
	}    
#endif

        k3dh->irq_gpio = platform_data->irq_gpio;
        /*Initialisation of GPIO_PS_OUT of proximity sensor*/
        if (gpio_request(k3dh->irq_gpio, "ACC INT")) {
                printk(KERN_ERR "[K3DH] ACC INT Request GPIO_%d failed!\n", k3dh->irq_gpio);
        }
        else {
                printk(KERN_ERR "[K3DH] ACC INT Request GPIO_%d Sucess!\n", k3dh->irq_gpio);
        }
 
        gpio_direction_input(k3dh->irq_gpio);
	k3dh->irq = gpio_to_irq(k3dh->irq_gpio);

	err = request_threaded_irq(k3dh->irq, NULL, k3dh_accel_interrupt_thread, 
		IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND, "ACC_INT", k3dh);
	if (err < 0) {
		pr_err("%s: can't allocate irq.\n", __func__);
		goto err_request_irq;
	} else {
            printk(KERN_INFO "[K3DH] request_irq success IRQ_NO:%d, GPIO:%d", k3dh->irq, k3dh->irq_gpio);
	} 
	disable_irq_nosync(k3dh->irq);
        printk(KERN_INFO "[K3DH] disable_irq IRQ_NO:%d\n",k3dh->irq);
	k3dh->interrupt_state = 0;

    	err = sensors_register(k3dh->k3dh_device,
			k3dh, accelerometer_attrs,"accelerometer_sensor");
	if (err < 0) {
        	pr_info("%s: could not sensors_register\n", __func__);
        	goto exit_k3dh_sensors_register;
	}

#if defined(CONFIG_SENSORS_HSCDTD006A) || defined(CONFIG_SENSORS_HSCDTD008A)
        g_k3dh = k3dh;
#endif
  
	return 0;

exit_k3dh_sensors_register:
err_request_irq:
    	mutex_destroy(&k3dh->value_mutex);
	mutex_destroy(&k3dh->enable_mutex);    
	wake_lock_destroy(&k3dh->reactive_wake_lock);
error_device:
	input_unregister_device(input_dev);
err_input_register_device:
err_input_allocate_device:
kfree_exit:
	kfree(k3dh);
#if defined(CONFIG_SENSORS_HSCDTD006A) || defined(CONFIG_SENSORS_HSCDTD008A)    
	g_k3dh = NULL;
#endif
exit:
	return err;
}

void k3dh_shutdown(struct i2c_client *client)
{
    	struct k3dh_data *k3dh = i2c_get_clientdata(client);
	k3dh_set_mode(client, 0);
        atomic_set(&k3dh->enable, 0);
}

static int k3dh_remove(struct i2c_client *client)
{
	struct k3dh_data *k3dh = i2c_get_clientdata(client);
    	mutex_destroy(&k3dh->value_mutex);
	mutex_destroy(&k3dh->enable_mutex);    
        wake_lock_destroy(&k3dh->reactive_wake_lock);
        sensors_unregister(k3dh->k3dh_device);        
	sysfs_remove_group(&k3dh->input->dev.kobj, &acc_attribute_group);
	input_unregister_device(k3dh->input);
        free_irq(k3dh->irq, k3dh);
	kfree(k3dh);
	return 0;
}

#ifdef CONFIG_PM
static int k3dh_suspend(struct device *dev)
{
	return 0;
}

static int k3dh_resume(struct device *dev)
{
	return 0;
}
#else
#define k3dh_suspend	NULL
#define k3dh_resume	NULL
#endif

static const struct i2c_device_id k3dh_id[] = {
	{ "k3dh", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, k3dh_id);

static const struct dev_pm_ops k3dh_pm_ops = {
	.suspend = k3dh_suspend,
	.resume = k3dh_resume,
};

static struct i2c_driver k3dh_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "k3dh",
#if !defined(CONFIG_SENSORS_HSCDTD006A)  && !defined(CONFIG_SENSORS_HSCDTD008A)
                .pm = &k3dh_pm_ops,
#endif
	},
	.id_table = k3dh_id,
	.probe = k3dh_probe,
	.shutdown = k3dh_shutdown,
	.remove = k3dh_remove,
};


static int __init k3dh_init(void)
{
        int ret=0;
	struct regulator *power_regulator = NULL;

    	printk(KERN_INFO "[K3DH] %s\n",__FUNCTION__);

	/* regulator init */
    	power_regulator = regulator_get(NULL, "tcxldo_uc");
    	if (IS_ERR(power_regulator)){
    	    printk(KERN_ERR "[K3DH] can not get prox_regulator (SENSOR_3.3V) \n");
    	} else {
            ret = regulator_set_voltage(power_regulator,3300000,3300000);
            printk(KERN_INFO "[K3DH] regulator_set_voltage : %d\n", ret);
            ret = regulator_enable(power_regulator);
            printk(KERN_INFO "[K3DH] regulator_enable : %d\n", ret);
            regulator_put(power_regulator);
    	}
        mdelay(10);
    	
	return i2c_add_driver(&k3dh_driver);
}

static void __exit k3dh_exit(void)
{
	i2c_del_driver(&k3dh_driver);
}

module_init(k3dh_init);
module_exit(k3dh_exit);

MODULE_DESCRIPTION("k3dh accelerometer driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");
