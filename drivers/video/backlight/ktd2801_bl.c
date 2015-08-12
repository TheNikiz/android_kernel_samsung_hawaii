/*
 * linux/drivers/video/backlight/ktd2801_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Copyright 2013 Broadcom Corporation.  All rights reserved.
*
*	@file	drivers/video/backlight/ktd2801_bl.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ktd2801_bl.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/lcd.h>
#include <linux/of_gpio.h>

int current_intensity;
static int backlight_pin;

static DEFINE_SPINLOCK(bl_ctrl_lock);

static int backlight_mode = 1;

#define CABC_FEATURE_ON 0
#define LCD_REGISTER 0

#define MAX_BRIGHTNESS_IN_BLU	33

#define DIMMING_VALUE		8

#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	10

#define BACKLIGHT_SUSPEND 0
#define BACKLIGHT_RESUME 1
#define BACKLIGHT_DEBUG 1
#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

struct brt_value {
	int level;	/* Platform setting values */
	int tune_level;	/* Chip Setting values */
};

struct brt_value brt_table[] = {
   { MIN_BRIGHTNESS_VALUE,  4 }, 
   { 25,  8 },
   { 30,  12 },
   { 35,  16 },  
   { 40,  20 }, 
   { 45,  23 }, 
   { 50,  26 },
   { 55,  29 }, 
   { 60,  32 },
   { 65,  35 }, 
   { 70,  38 },
   { 75,  41 },
   { 80,  44 }, 
   { 85,  47 }, 
   { 90,  50 }, 
   { 95,  53 }, 
   { 100,  56 },   
   { 105,  59 },
   { 110,  62 }, 
   { 115,  65 }, 
   { 120,  68 },
   { 125,  71 },
   { 130,  74 },
   { 135,  77 },
   { 140,  80 },
   { 143,  82 },
   { 145,  85 },  
   { 150,  88 },
   { 155,  91 },
   { 160,  94 },
   { 162,  97 },  /* default 162 */
   { 165,  100 },
   { 170,  104 },
   { 175,  108 },
   { 180,  112 },
   { 185,  116 }, 
   { 190,  122 },
   { 195,  128 },
   { 200,  134 },
   { 205,  140 },
   { 210,  143 },
   { 215,  147 },
   { 220,  151 },
   { 225,  155 },
   { 230,  159 },
   { 235,  163 },
   { 240,  167 },
   { 245,  171 },
   { 250,  175 },
   { MAX_BRIGHTNESS_VALUE,  179}, 
};

#define MAX_BRT_STAGE (int)(sizeof(brt_table)/sizeof(struct brt_value))

struct ktd_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};

struct lcd_info {
	struct device			*dev;
	struct spi_device		*spi;
	unsigned int			power;
	unsigned int			gamma_mode;
	unsigned int			current_gamma_mode;
	unsigned int			current_bl;
	unsigned int			bl;
#if CABC_FEATURE_ON
	unsigned int			auto_brightness;
#endif
	unsigned int			ldi_enable;
	unsigned int			acl_enable;
	unsigned int			cur_acl;
	struct mutex			lock;
	struct mutex			bl_lock;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		early_suspend;
#endif
#ifdef SMART_DIMMING
	unsigned char			id[3];
	struct str_smart_dim		smart;
#endif
};
extern int lcd_exist;
void lcd_backlight_control(int level)
{
	int i, set_bit;
	unsigned long flags;

        if(0 == level ){
            BLDBG("[BACKLIGHT] backlight brightness level == 0\n");
            gpio_set_value(backlight_pin, 0);
            return; 
        }

	if(0 == lcd_exist ){
            BLDBG("[BACKLIGHT] LCD panel does not exist.\n");
            gpio_set_value(backlight_pin, 0);
            return; 
        }

	spin_lock_irqsave(&bl_ctrl_lock, flags);

        if(0 == gpio_get_value(backlight_pin)){
               gpio_set_value(backlight_pin, 1);
	       udelay(140);
	       gpio_set_value(backlight_pin, 0);
	       udelay(260);
	       gpio_set_value(backlight_pin, 1);
	       udelay(10);
        }else{
	       gpio_set_value(backlight_pin, 1);
	       //udelay(40);
         }

	BLDBG("Level %0x\n", level);
	for (i = 7; i >= 0; i--) {
		set_bit = ((level) & ((1<<i)))>>(i);
	if (set_bit) {
		gpio_set_value(backlight_pin, 0);
		udelay(10);
		gpio_set_value(backlight_pin, 1);
		udelay(20);
	} else {
		gpio_set_value(backlight_pin, 0);
		udelay(20);
		gpio_set_value(backlight_pin, 1);
		udelay(10);
		}
	}
	gpio_set_value(backlight_pin, 0);
	udelay(4);
	gpio_set_value(backlight_pin, 1);
	//udelay(400);
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
}
EXPORT_SYMBOL(lcd_backlight_control);

/* input: intensity in percentage 0% - 100% */
static int ktd_backlight_update_status(struct backlight_device *bd)
{
	int user_intensity = bd->props.brightness;
	int tune_level = 0;
	int i;

	BLDBG("[BACKLIGHT] update_status ==> user_intensity  : %d\n",
							user_intensity);

#if CABC_FEATURE_ON
	g_bl.props.brightness = user_intensity;
#endif
	current_intensity = user_intensity;

	if (bd->props.power != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.state & BL_CORE_SUSPENDED)
		printk("[BACKLIGHT] update_status ==> user_intensity  : %d\n",
			user_intensity);


	if (backlight_mode != BACKLIGHT_RESUME) {
		BLDBG("[BACKLIGHT] suspend mode %d\n", backlight_mode);
		return 0;
	}

	if(user_intensity > 0) {
		if(user_intensity < MIN_BRIGHTNESS_VALUE) {
			tune_level = DIMMING_VALUE; //DIMMING
		} else if (user_intensity == MAX_BRIGHTNESS_VALUE) {
			tune_level = brt_table[MAX_BRT_STAGE-1].tune_level;
		} else {
			for(i = 0; i < MAX_BRT_STAGE; i++) {
				if(user_intensity <= brt_table[i].level ) {
					tune_level = brt_table[i].tune_level;
					break;
				}
			}
		}
	}

	BLDBG("[BACKLIGHT] update_status ==> tune_level : %d\n", tune_level);

	if (user_intensity >= 0) {
		lcd_backlight_control(tune_level);
	}

	return 0;
}


#if CABC_FEATURE_ON
void backlight_update_CABC()
{
	BLDBG("[BACKLIGHT] backlight_update_CABC\n");
	ktd_backlight_update_status(&g_bl);
}
EXPORT_SYMBOL(backlight_update_CABC);

extern void backlight_cabc_on(void);
extern void backlight_cabc_off(void);
#endif

static int ktd_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] backlight_get_brightness\n");
	return current_intensity;
}

static struct backlight_ops ktd_backlight_ops = {
	.update_status	= ktd_backlight_update_status,
	.get_brightness	= ktd_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ktd_backlight_earlysuspend(struct early_suspend *desc)
{
	struct timespec ts;
	struct rtc_time tm;

	backlight_mode = BACKLIGHT_SUSPEND;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
        
        if(1 == gpio_get_value(backlight_pin)){
	gpio_set_value(backlight_pin, 0);
	mdelay(3);
        }

	BLDBG("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n",
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static void ktd_backlight_lateresume(struct early_suspend *desc)
{
	struct ktd_bl_data *ktd = container_of(desc, struct ktd_bl_data,
							early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(ktd->pdev);
	struct timespec ts;
	struct rtc_time tm;


	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	backlight_mode = BACKLIGHT_RESUME;

#if CABC_FEATURE_ON
	if (cabc_status)
		backlight_cabc_on();
	else
		backlight_cabc_off();

#endif
	BLDBG("[%02d:%02d:%02d.%03lu][BACKLIGHT]  late resume\n",
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

        if(0 != bl->props.brightness)
	backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int ktd_backlight_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd_bl_data *ktd = dev_get_drvdata(&bl->dev);

	BLDBG("[BACKLIGHT] backlight_suspend\n");

	return 0;
}

static int ktd_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	BLDBG("[BACKLIGHT] backlight_resume\n");

	backlight_update_status(bl);

	return 0;
}
#else
#define ktd_backlight_suspend  NULL
#define ktd_backlight_resume   NULL
#endif
#endif

#if CABC_FEATURE_ON
static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	BLDBG("auto_brightness_store called\n");

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__,
					lcd->auto_brightness, value);
			lcd->auto_brightness = value;

			if (lcd->auto_brightness == 0) {
				cabc_status = 0;
				backlight_cabc_off();
			} else if (lcd->auto_brightness >= 1
					&& lcd->auto_brightness < 5) {
				cabc_status = 1;
				backlight_cabc_on();
			} else if (lcd->auto_brightness >= 5) {
				cabc_status = 0;
				backlight_cabc_off();
			}


		}
	}
	return size;
}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show,
					auto_brightness_store);
#endif

#if LCD_REGISTER
static int pulse_value;

static ssize_t brightness_pulse_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", pulse_value);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t brightness_pulse_setting(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	int rc;
	BLDBG("[BACKLIGHT] brightness_pulse_setting\n");

	rc = strict_strtoul(buf, (unsigned int)0,
				(unsigned long *)&pulse_value);

	if (rc < 0)
		return rc;
	else {

		if (pulse_value <= 32 && pulse_value >= 1) {
			BLDBG("brightness_pulse_setting, pulse value=%d\n",
							pulse_value);
			lcd_backlight_control(pulse_value);
		} else
			BLDBG("out of range !!\n");
	}
	return size;
}

static DEVICE_ATTR(manual_pulseset, 0644, brightness_pulse_show,
						brightness_pulse_setting);
#endif

static int get_platform_data(struct platform_device *pdev,
	struct platform_ktd2801_backlight_data *pdata)
{
	unsigned int val;
	if (of_property_read_u32(pdev->dev.of_node, "max-brightness", &val)) {
		dev_err(&pdev->dev, "max_brightness not getting from dts\n");
		return -1;
	}
	pdata->max_brightness = val;
	if (of_property_read_u32(pdev->dev.of_node,
				"default-brightness", &val)) {
		dev_err(&pdev->dev, "dft_brightness not getting from dts\n");
		return -1;
	}
	pdata->dft_brightness = val;
	pdata->ctrl_pin = of_get_gpio(pdev->dev.of_node, 0);
	pdev->dev.platform_data = pdata;
	return 0;
}

static const struct of_device_id ktd2801_of_match[] = {
	{.compatible = "kinetic,ktd2801",},
	{},
};

MODULE_DEVICE_TABLE(of, ktd2801_of_match);

static int ktd_backlight_probe(struct platform_device *pdev)
{
	struct platform_ktd2801_backlight_data *data = NULL;
	struct backlight_device *bl;
	struct ktd_bl_data *ktd;
	struct backlight_properties props;
	int ret=0;

	BLDBG("[BACKLIGHT] backlight driver probe\n");

	if (pdev->dev.of_node) {
		data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
		if (!data) {
			dev_err(&pdev->dev, "no memory for state\n");
			return -ENOMEM;
		}
		if (get_platform_data(pdev, data)) {
			dev_err(&pdev->dev, "get_platform_data error\n");
			return -EINVAL;
		}
	} else {
		dev_info(&pdev->dev, "No Device Tree\n");
		data = pdev->dev.platform_data;
	}

	ktd = devm_kzalloc(&pdev->dev, sizeof(*ktd), GFP_KERNEL);
	if (!ktd) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	//backlight_pin = data->ctrl_pin;
      backlight_pin = 24;

      ret = gpio_request(backlight_pin, "BL_CTRL");

      if(ret)
         BLDBG("BACKLIGHT GPIO REQUEST FAIL!\n");

      gpio_direction_output(backlight_pin, 1);

      BLDBG("[BACKLIGHT] backlight_pin == %d\n", backlight_pin);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	props.type = BACKLIGHT_PLATFORM;

	bl = backlight_device_register("panel", &pdev->dev,
			ktd, &ktd_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

#if CABC_FEATURE_ON
	lcd_device_register("panel", &pdev->dev, NULL, NULL);

	ret = device_create_file(&bl->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to add sysfs entries\n");
#endif
#if LCD_REGISTER
	lcd_device_register("panel", &pdev->dev, NULL, NULL);

	ret = device_create_file(&bl->dev, &dev_attr_manual_pulseset);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to add sysfs entries\n");
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ktd->pdev = pdev;
	ktd->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ktd->early_suspend_desc.suspend = ktd_backlight_earlysuspend;
	ktd->early_suspend_desc.resume = ktd_backlight_lateresume;
	register_early_suspend(&ktd->early_suspend_desc);
#endif

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;

	platform_set_drvdata(pdev, bl);

	return 0;
}

static int ktd_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd_bl_data *ktd = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);


#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ktd->early_suspend_desc);
#endif
        BLDBG("[BACKLIGHT] ktd_backlight_remove\n");
	return 0;
}

static void ktd_backlight_shutdown(struct platform_device *pdev)
{
	BLDBG("[BACKLIGHT] ktd_backlight_shutdown\n");
	gpio_set_value(backlight_pin, 0);
	mdelay(3);

}


static struct platform_driver ktd_backlight_driver = {
	.driver		= {
		.name	= "panel",
		.owner	= THIS_MODULE,
		.of_match_table = ktd2801_of_match,
	},
	.probe		= ktd_backlight_probe,
	.remove		= ktd_backlight_remove,
	.shutdown       = ktd_backlight_shutdown,

#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = ktd_backlight_suspend,
	.resume         = ktd_backlight_resume,
#endif

};

module_platform_driver(ktd_backlight_driver);

MODULE_DESCRIPTION("ktd based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd-backlight");

