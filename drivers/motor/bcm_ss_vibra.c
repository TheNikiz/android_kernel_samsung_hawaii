/*****************************************************************************
* Copyright 2012 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif /*CONFIG_HAS_WAKELOCK*/

#include "../staging/android/timed_output.h"
#include "../staging/android/timed_gpio.h"

#define VIB_ON 1
#define VIB_OFF 0
#define MIN_TIME_MS 50

#if defined(CONFIG_HAS_WAKELOCK)
static struct wake_lock vib_wl;
#endif /*CONFIG_HAS_WAKELOCK*/

static struct workqueue_struct *vib_workqueue;
static struct delayed_work	vibrator_off_work;
static int extend_time = 0;
static struct regulator* vib_regulator = NULL;
static unsigned int vib_voltage;

static void vibrator_ctrl_regulator(int on_off)
{
	int ret=0;
	printk(KERN_NOTICE "Vibrator: %s\n",(on_off?"ON":"OFF"));

	if(on_off)
	{
		if(!regulator_is_enabled(vib_regulator))
		{
			regulator_set_voltage(vib_regulator, vib_voltage, vib_voltage);
			ret = regulator_enable(vib_regulator);
			printk(KERN_NOTICE "Vibrator: enable %d\n", ret);
		}
	}
	else
	{
		if(regulator_is_enabled(vib_regulator))
		{
			ret = regulator_disable(vib_regulator);
			printk(KERN_NOTICE "Vibrator: disable %d\n", ret);
		}
	}
}

static void vibrator_off_worker(struct work_struct *work)
{
	printk(KERN_NOTICE "Vibrator: %s %d\n", __func__, extend_time);

	extend_time = 0;
	vibrator_ctrl_regulator(VIB_OFF);
#if defined(CONFIG_HAS_WAKELOCK)
	wake_unlock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
	int timeout)
{
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock(&vib_wl);
#endif /*CONFIG_HAS_WAKELOCK*/

	if(cancel_delayed_work_sync(&vibrator_off_work))
		printk(KERN_NOTICE "Vibrator: Cancel off work to re-start\n");

	if( timeout > 0 )
	{
		vibrator_ctrl_regulator(VIB_ON);
		if(timeout < MIN_TIME_MS)
			extend_time = MIN_TIME_MS - timeout;
		else
			extend_time = 0;
	}
	printk(KERN_NOTICE "Vibrator: Set duration: %dms + %dms\n", timeout, extend_time);
	queue_delayed_work(vib_workqueue, &vibrator_off_work, msecs_to_jiffies(timeout + extend_time));
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	int retTime = jiffies_to_msecs(jiffies - vibrator_off_work.timer.expires);
	printk(KERN_NOTICE "Vibrator: Current duration: %dms\n", retTime);
	return retTime;
}

static struct timed_output_dev vibrator_timed_dev = {
	.name = "vibrator",
	.get_time = vibrator_get_remaining_time,
	.enable = vibrator_enable_set_timeout,
};

static int vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;

	vib_regulator = regulator_get(NULL, (const char *)(pdev->dev.platform_data));	
#if defined(CONFIG_MACH_CAPRI_SS_CRATER) || defined(CONFIG_MACH_HAWAII_SS_KYLEPRO) || defined(CONFIG_MACH_HAWAII_SS_GOYA3G) || defined(CONFIG_MACH_HAWAII_SS_VIVALTODS5M) || defined(CONFIG_MACH_HAWAII_SS_VIVALTONFC3G)
		vib_voltage = 3300000;
#else
		vib_voltage = 3000000;
#endif

#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_init(&vib_wl, WAKE_LOCK_SUSPEND, __stringify(vib_wl));
#endif

	ret = timed_output_dev_register(&vibrator_timed_dev);
	if (ret < 0) {
		printk(KERN_ERR "Vibrator: timed_output dev registration failure\n");
		goto error;
	}

	vib_workqueue = create_workqueue("vib_wq");
	INIT_DELAYED_WORK(&vibrator_off_work, vibrator_off_worker);

	return 0;

error:
#if defined(CONFIG_HAS_WAKELOCK)
	wake_lock_destroy(&vib_wl);
#endif
	regulator_put(vib_regulator);
	vib_regulator = NULL;
	return ret;
}

static int vibrator_remove(struct platform_device *pdev)
{
	timed_output_dev_unregister(&vibrator_timed_dev);
	if (vib_regulator) 
	{
		regulator_put(vib_regulator);
		vib_regulator = NULL;
	}

	return 0;
}

static int vibrator_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vibrator_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe		= vibrator_probe,
	.remove		= vibrator_remove,
	.suspend		= vibrator_suspend,
	.resume		=  vibrator_resume,
	.driver		= {
		.name	= "vibrator",
		.owner	= THIS_MODULE,
	},
};

static int __init vibrator_init(void)
{
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	platform_driver_unregister(&vibrator_driver);
}

module_init(vibrator_init);
module_exit(vibrator_exit);

MODULE_DESCRIPTION("Android Vibrator driver");
MODULE_LICENSE("GPL");
