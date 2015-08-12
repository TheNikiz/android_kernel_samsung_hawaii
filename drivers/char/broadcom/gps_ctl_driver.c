/****************************************************************************
Copyright 2010 Broadcom Corporation.  All rights reserved.

Unless you and Broadcom execute a separate written software license agreement
governing use of this software, this software is licensed to you under the
terms of the GNU General Public License version 2, available at
http://www.gnu.org/copyleft/gpl.html (the "GPL").

Notwithstanding the above, under no circumstances may you combine this software
in any way with any other Broadcom software provided under a license other than
the GPL, without Broadcom's express prior written consent.
******************************************************************************/

/***************************************************************************
**
*
*   @file   gps_ctl_driver.c
*
*   @brief  This driver is used for turn on/off GPS regulator3.3V
*
*
****************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/regulator/consumer.h>
#include <linux/broadcom/bcm_major.h>

#define GPS_KERNEL_MODULE_NAME  "bcm_gps_ctl"

#define GPS_LOG_LEVEL	KERN_ERR

#define GPS_KERNEL_TRACE_ON
#ifdef GPS_KERNEL_TRACE_ON
#define GPS_TRACE(str) printk str
#else
#define GPS_TRACE(str) {}
#endif

#define GPS_CMD_REGUL_CTL_ON	0
#define GPS_CMD_REGUL_CTL_OFF	1
#define GPS_CMD_REGUL_STATUS	2

/**
 *  file ops
 */
static int gps_ctl_kernel_open(struct inode *inode, struct file *filp);
static int gps_ctl_kernel_read(struct file *filep, char __user *buf,
			   size_t size, loff_t *off);
static long gps_ctl_kernel_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg);
static int gps_ctl_kernel_release(struct inode *inode, struct file *filp);

static struct class *gps_ctl_class;
static struct regulator* gps_regulator = NULL;
static int gps_voltage = 3300000;

/*Platform device data*/
/*
typedef struct _PlatformDevData_t {
	int init;
} PlatformDevData_t;
*/
static const struct file_operations sFileOperations = {
	.owner = THIS_MODULE,
	.open = gps_ctl_kernel_open,
	.read = gps_ctl_kernel_read,
	.write = NULL,
	.unlocked_ioctl = gps_ctl_kernel_ioctl,
	.poll = NULL,
	.mmap = NULL,
	.release = gps_ctl_kernel_release
};

/*************************************************************
**
 *  Called by Linux I/O system to handle open() call.
 *  @param  (in)    not used
 *  @param  (io)    file pointer
 *  @return int     0 if success, -1 if error
 *  @note
 *      API is defined by struct file_operations 'open' member.
 */

static int gps_ctl_kernel_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
//	if (gps_regulator == NULL) {
//		gps_regulator = regulator_get(NULL, "mmc2_vcc");
//		ret = 1;
//	}

	GPS_TRACE((GPS_LOG_LEVEL "gps_ctl_kernel_open "));
	return ret;
}

static int gps_ctl_kernel_read(struct file *filep, char __user *buf,
			   size_t size, loff_t *off)
{
	bool ret = false;
	if(gps_regulator != NULL) 
	{
		ret = regulator_is_enabled(gps_regulator);
	}
	GPS_TRACE((GPS_LOG_LEVEL "gps_ctl_kernel_read\n"));
	
	if(ret == false)return 0;
	else return 1;
}

static long gps_ctl_kernel_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	bool retval = false;
	int errorcode = 0;
	GPS_TRACE((GPS_LOG_LEVEL "gps_ctl_kernel_ioctl cmd=%d gps_regulator=0x%x\n",
		   cmd, gps_regulator));
	
	if(gps_regulator == NULL )
	{
		GPS_TRACE((GPS_LOG_LEVEL  "gps: gps_regulator is NULL\n"));
		return -1;
	}

	switch (cmd) {
	case GPS_CMD_REGUL_CTL_ON:
		if (!regulator_is_enabled(gps_regulator)) {
			regulator_set_voltage(gps_regulator,gps_voltage,gps_voltage);
			regulator_enable(gps_regulator);
			GPS_TRACE((GPS_LOG_LEVEL "gps_ctl_kernel_ioctl regulator_enable()\n"));
		}
		break;
	case GPS_CMD_REGUL_CTL_OFF:
		if (regulator_is_enabled(gps_regulator)) {
			errorcode = regulator_disable(gps_regulator);
			if(!errorcode)
				GPS_TRACE((GPS_LOG_LEVEL  "gps: gps_regulator disabled\n"));
			else
				GPS_TRACE((GPS_LOG_LEVEL  "gps: regulator disable failed, check why gps_regulator isn't disabled: Error Code%d\n", errorcode));
		}
		break;
	case GPS_CMD_REGUL_STATUS:
		if(gps_regulator != NULL) 
		{
			retval = regulator_is_enabled(gps_regulator);
		}
		GPS_TRACE((GPS_LOG_LEVEL "gps_ctl_kernel_read \n"));
		break;
	default:
		break;
	}
	
	if(retval == false)return 0;
	else return 1;
}

static int gps_ctl_kernel_release(struct inode *inode, struct file *filp)
{
	bool ret = false;
	GPS_TRACE((GPS_LOG_LEVEL,  "gps_ctl_kernel_release\n"));
//	if (gps_regulator != NULL) {
//		ret = regulator_disable(gps_regulator);
//		if(!ret)
//			GPS_TRACE((GPS_LOG_LEVEL  "gps: gps_regulator disabled\n"));
//		else
//			GPS_TRACE((GPS_LOG_LEVEL  "gps: regulator disable failed, check why gps_regulator isn't disabled: Error Code%d\n", ret));
//		
//	gps_regulator = NULL;
//	}

	return 0;
}

/***************************************************************************
**
 *  Called by Linux I/O system to initialize module.
 *  @return int     0 if success, -1 if error
 *  @note
 *      API is defined by module_init macro
 */
static int __init gps_ctl_kernel_module_init(void)
{
	int err = 1;
	struct device *myDevice;
	dev_t myDev;

	GPS_TRACE((GPS_LOG_LEVEL "enter gps_ctl_kernel_module_init()\n"));

	/*drive driver process: */
	if (register_chrdev(BCM_GPS_REGUL_CTL_MAJOR, GPS_KERNEL_MODULE_NAME,
			    &sFileOperations) < 0) {
		GPS_TRACE((GPS_LOG_LEVEL "register_chrdev failed\n"));
		return -1;
	}

	gps_ctl_class = class_create(THIS_MODULE, "bcm_gps_ctl");
	if (IS_ERR(gps_ctl_class))
		return PTR_ERR(gps_ctl_class);
	myDev = MKDEV(BCM_GPS_REGUL_CTL_MAJOR, 0);

	GPS_TRACE((GPS_LOG_LEVEL "mydev = %d\n", myDev));

	myDevice = device_create(gps_ctl_class, NULL, myDev, NULL, "bcm_gps_ctl");

	err = PTR_ERR(myDevice);
	if (IS_ERR(myDevice)) {
		GPS_TRACE((GPS_LOG_LEVEL "device create failed\n"));
		return -1;
	}


	GPS_TRACE((GPS_LOG_LEVEL "status gps_regulator: %d\n", gps_regulator));
	if (gps_regulator == NULL) {
		gps_regulator = regulator_get(NULL, "gpldo3_uc");
		GPS_TRACE((GPS_LOG_LEVEL "get gps_regulator:0x%x\n", gps_regulator));
	}

	GPS_TRACE((GPS_LOG_LEVEL "exit sucessfuly gps_ctl_kernel_module_init()\n"));
	return 0;
}

/***************************************************************************
**
 *  Called by Linux I/O system to exit module.
 *  @return int     0 if success, -1 if error
 *  @note
 *      API is defined by module_exit macro
 **/
static void __exit gps_ctl_kernel_module_exit(void)
{
	int ret =0;
	GPS_TRACE((GPS_LOG_LEVEL "gps_ctl_kernel_module_exit()\n"));
	
	if (gps_regulator != NULL) {
		ret = regulator_disable(gps_regulator);
		if(!ret)
			GPS_TRACE((GPS_LOG_LEVEL  "gps: gps_regulator disabled\n"));
		else
			GPS_TRACE((GPS_LOG_LEVEL  "gps: regulator disable failed, check why gps_regulator isn't disabled: Error Code%d\n", ret));
		
	gps_regulator = NULL;
	}
	unregister_chrdev(BCM_GPS_REGUL_CTL_MAJOR, GPS_KERNEL_MODULE_NAME);
}

/**
 *  export module init and export functions
 **/
module_init(gps_ctl_kernel_module_init);
module_exit(gps_ctl_kernel_module_exit);
MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPS Control Driver");
