/* sec_thermistor.c
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/sec_thermistor.h>
#include <linux/power_supply.h>
#include <linux/mfd/bcmpmu59xxx.h>
#include <linux/delay.h>


#define SEC_THERMISTOR_ADC_READ_TRIES 10
#define SEC_THERMISTOR_ADC_RETRY_DELAY 50
extern struct bcmpmu59xxx *bcmpmu_info;

struct sec_therm_info {
	struct device *dev;
	struct sec_therm_platform_data *pdata;
	struct delayed_work polling_work;

	int curr_temperature;
	int curr_temp_adc;
};

#if defined (SSRM_TEST)
static int tempTest;
#endif

static void  sec_thermistor_bcmpmu_adc_read(struct bcmpmu59xxx *bcmpmu,
				enum bcmpmu_adc_channel channel,
				enum bcmpmu_adc_req req,
				struct bcmpmu_adc_result *result)
{
	int ret = 0;
	int retries = SEC_THERMISTOR_ADC_READ_TRIES;
	static int t_result_conv;

	while (retries--) {
		ret = bcmpmu_adc_read(bcmpmu, channel, req, result);
		if (!ret)
			break;
		msleep(SEC_THERMISTOR_ADC_RETRY_DELAY);
	}
	BUG_ON(retries <= 0);
	if(retries <= 0)
		result->conv= t_result_conv;
	else
		t_result_conv = result->conv;
}


static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	struct bcmpmu_adc_result adc_result;
	int temper;

	sec_thermistor_bcmpmu_adc_read(bcmpmu_info, info->pdata->adc_channel,
				PMU_ADC_REQ_RTM_MODE, &adc_result);

	temper = adc_result.conv;

#if defined (SSRM_TEST)
    temper = tempTest;
#endif

	return sprintf(buf, "%d\n", temper);
}

#if defined (SSRM_TEST)
static ssize_t sec_therm_store_temperature(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &tempTest);
	return count;
}
#endif

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	struct bcmpmu_adc_result adc_result;
	int adc;

	sec_thermistor_bcmpmu_adc_read(bcmpmu_info, info->pdata->adc_channel,
				PMU_ADC_REQ_RTM_MODE, &adc_result);

	adc = adc_result.raw;

	return sprintf(buf, "%d\n", adc);
}


#if defined (SSRM_TEST)
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR, sec_therm_show_temperature, sec_therm_store_temperature);
#else
static DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature, NULL);
#endif
static DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL);

static struct attribute *sec_therm_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temp_adc.attr,
	NULL
};

static const struct attribute_group sec_therm_group = {
	.attrs = sec_therm_attributes,
};

static void notify_change_of_temperature(struct sec_therm_info *info)
{
	char temp_buf[20];
	char *envp[2];
	int env_offset = 0;

	snprintf(temp_buf, sizeof(temp_buf), "TEMPERATURE=%d",
		 info->curr_temperature);
	envp[env_offset++] = temp_buf;
	envp[env_offset] = NULL;

	dev_info(info->dev, "%s: uevent: %s\n", __func__, temp_buf);
	kobject_uevent_env(&info->dev->kobj, KOBJ_CHANGE, envp);
}

static void sec_therm_polling_work(struct work_struct *work)
{
	struct sec_therm_info *info =
		container_of(work, struct sec_therm_info, polling_work.work);
	int adc;
	int temper;
	struct bcmpmu_adc_result adc_result;

	sec_thermistor_bcmpmu_adc_read(bcmpmu_info, info->pdata->adc_channel,
				PMU_ADC_REQ_SAR_MODE, &adc_result);
	adc = adc_result.raw;
	if (adc < 0)
		goto out;
	temper = adc_result.conv;
		
	dev_info(info->dev, "%s: PA_adc=%d\n", __func__, adc);
	dev_info(info->dev, "%s: PA_temper=%d\n", __func__, temper);

	/* if temperature was changed, notify to framework */
	if (info->curr_temperature != temper) {
		info->curr_temp_adc = adc;
		info->curr_temperature = temper;
		notify_change_of_temperature(info);
	}

out:
	schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->pdata->polling_interval));
}

static int sec_therm_probe(struct platform_device *pdev)
{
	struct sec_therm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct sec_therm_info *info;
	int ret = 0;

	dev_info(&pdev->dev, "%s: SEC Thermistor Driver Loading\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	info->pdata = pdata;
	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_group);

	if (ret) {
		dev_err(info->dev,
			"failed to create sysfs attribute group\n");

		kfree(info);
	}

	if (!(pdata->no_polling)) {

		INIT_DELAYED_WORK(&info->polling_work,
			sec_therm_polling_work);
		schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->pdata->polling_interval));

	}
	return ret;
}

static int sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);

	if (!info)
		return 0;

	sysfs_remove_group(&info->dev->kobj, &sec_therm_group);

	if (!(info->pdata->no_polling))
		cancel_delayed_work(&info->polling_work);
	kfree(info);

	return 0;
}

#ifdef CONFIG_PM
static int sec_therm_suspend(struct device *dev)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	if (!(info->pdata->no_polling))
		cancel_delayed_work_sync(&info->polling_work);

	return 0;
}

static int sec_therm_resume(struct device *dev)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	if (!(info->pdata->no_polling))
		schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->pdata->polling_interval));
	return 0;
}
#else
#define sec_therm_suspend	NULL
#define sec_therm_resume	NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops sec_thermistor_pm_ops = {
	.suspend = sec_therm_suspend,
	.resume = sec_therm_resume,
};

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		   .name = "sec-thermistor",
		   .owner = THIS_MODULE,
		   .pm = &sec_thermistor_pm_ops,
	},
	.probe = sec_therm_probe,
	.remove = sec_therm_remove,
};

static int __init sec_therm_init(void)
{
	pr_info("func:%s\n", __func__);
	#if defined (SSRM_TEST)
	tempTest=333;
	#endif
	return platform_driver_register(&sec_thermistor_driver);
}
module_init(sec_therm_init);

static void __exit sec_therm_exit(void)
{
	platform_driver_unregister(&sec_thermistor_driver);
}
module_exit(sec_therm_exit);

MODULE_AUTHOR("ms925.kim@samsung.com");
MODULE_DESCRIPTION("sec thermistor driver");
MODULE_LICENSE("GPL");
