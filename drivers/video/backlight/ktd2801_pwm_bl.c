/*
 * linux/drivers/video/backlight/ktd2801_pwm_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/ktd2801_pwm_bl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/broadcom/lcd.h>
#include <linux/spinlock.h>
#include <linux/broadcom/PowerManager.h>
#include <linux/rtc.h>

int current_intensity;

static int backlight_mode=1;

#define DIMMING_VALUE		8
#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	20
#define BACKLIGHT_DEBUG 1
#define BACKLIGHT_SUSPEND 0
#define BACKLIGHT_RESUME 1

#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

struct ktd2801_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};

struct brt_value{
	int level;				/* Platform setting values */ 
	int tune_level;           /* Chip Setting values */
};

extern void lcd_clock_on();
extern void lcd_clock_off();

struct brt_value brt_table[] = {
   { MIN_BRIGHTNESS_VALUE,  8 }, 
   { 25,  11 },
   { 30,  14 },
   { 35,  17 },  
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
   { 95,  54 }, 
   { 100,  58 },   
   { 105,  62 },
   { 110,  66 }, 
   { 115,  70 }, 
   { 120,  74 },
   { 125,  78 },
   { 130,  82 },
   { 135,  85 },
   { 140,  100 }, /* default 143 */
   { 145,  105 },  
   { 150,  110 },
   { 155,  115 },
   { 160,  120 },
   { 165,  125 },
   { 170,  130 },
   { 175,  135 },
   { 180,  140 },
   { 185,  145 }, 
   { 190,  150 },
   { 195,  155 },
   { 200,  160 },
   { 205,  164 },
   { 210,  168 },
   { 215,  172 },
   { 220,  176 },
   { 225,  180 },
   { 230,  184 },
   { 235,  188 },
   { 240,  192 },
   { 245,  197 },
   { 250,  202 },
   { MAX_BRIGHTNESS_VALUE,  207 }, 
};


#define MAX_BRT_STAGE (int)(sizeof(brt_table)/sizeof(struct brt_value))

extern void panel_write(u8 *buff);

static u8 set_bl_seq[] = {
	2, 0x51, 0xFF, 0x00 /* The Last 0x00 : Sequence End Mark */
};

void backlight_control(int brigtness)
{
	set_bl_seq[2] = brigtness;
	lcd_clock_on();
	panel_write(set_bl_seq);
	lcd_clock_off();
}
EXPORT_SYMBOL(backlight_control);

int tune_level = 0;
EXPORT_SYMBOL(tune_level);

/* input: intensity in percentage 0% - 100% */
static int ktd2801_backlight_update_status(struct backlight_device *bd)
{
	int user_intensity = bd->props.brightness;
	int i;
  
	/* BLDBG("[BACKLIGHT] ktd2801_backlight_update_status ==> user_intensity  : %d\n", user_intensity); */
		
	if (bd->props.power != FB_BLANK_UNBLANK)
		BLDBG("[BACKLIGHT] Power is not FB_BLANK_UNBLANK\n");

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		BLDBG("[BACKLIGHT] FB_blank in not FB_BLANK_UNBLANK\n");

	if (bd->props.state & BL_CORE_SUSPENDED)
		BLDBG("[BACKLIGHT] BL_CORE_SUSPENDED\n");
		
	if(backlight_mode != BACKLIGHT_RESUME){
		BLDBG("[BACKLIGHT] Returned with invalid backlight mode %d\n", user_intensity);
		return 0;
	}

	if(user_intensity > 0) {
		if(user_intensity < MIN_BRIGHTNESS_VALUE) {
			tune_level = DIMMING_VALUE;
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
		
	if(user_intensity==0)
		tune_level=0;
	
	BLDBG("[BACKLIGHT] ktd2801_backlight_update_status ==> user_intensity = %d , tune_level : %d\n", user_intensity, tune_level);
	
	backlight_control(tune_level);  
	
	return 0;
}

static int ktd2801_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] ktd2801_backlight_get_brightness\n");
    
	return current_intensity;
}

static struct backlight_ops ktd2801_backlight_ops = {
	.update_status	= ktd2801_backlight_update_status,
	.get_brightness	= ktd2801_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ktd2801_backlight_earlysuspend(struct early_suspend *desc)
{
	struct timespec ts;
	struct rtc_time tm;
	
	backlight_mode=BACKLIGHT_SUSPEND; 
       printk("BACKLIGHT suspend ~~~~ \n");
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static void ktd2801_backlight_earlyresume(struct early_suspend *desc)
{
	struct ktd2801_bl_data *ktd2801 = container_of(desc, struct ktd2801_bl_data, early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(ktd2801->pdev);
	struct timespec ts;
	struct rtc_time tm;
	
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	backlight_mode = BACKLIGHT_RESUME;
	printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlyresume\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
       printk("BACKLIGHT resume~~~~ \n");
	backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int ktd2801_backlight_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd2801_bl_data *ktd2801 = dev_get_drvdata(&bl->dev);
	  
	BLDBG("[BACKLIGHT] ktd2801_backlight_suspend\n");
	      
	return 0;
}
static int ktd2801_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
  BLDBG("[BACKLIGHT] ktd2801_backlight_resume\n");
	    
	backlight_update_status(bl);
	    
	return 0;
}
#else
#define ktd2801_backlight_suspend  NULL
#define ktd2801_backlight_resume   NULL
#endif /* CONFIG_PM */
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int ktd2801_backlight_probe(struct platform_device *pdev)
{
	struct platform_ktd2801_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct ktd2801_bl_data *ktd2801;
	struct backlight_properties props;
	int ret;
	
	BLDBG("[BACKLIGHT] ktd2801_backlight_probe\n");
	
	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}
	
	ktd2801 = kzalloc(sizeof(*ktd2801), GFP_KERNEL);
	if (!ktd2801) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	
	/* ktd2801->ctrl_pin = data->ctrl_pin; */
    
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	props.type = BACKLIGHT_PLATFORM;
	bl = backlight_device_register(pdev->name, &pdev->dev,
			ktd2801, &ktd2801_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ktd2801->pdev = pdev;
	ktd2801->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ktd2801->early_suspend_desc.suspend = ktd2801_backlight_earlysuspend;
	ktd2801->early_suspend_desc.resume = ktd2801_backlight_earlyresume;
	register_early_suspend(&ktd2801->early_suspend_desc);
#endif

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;
	platform_set_drvdata(pdev, bl);
      /* ktd2801_backlight_update_status(bl); */
    
	return 0;
	
err_bl:
	kfree(ktd2801);
err_alloc:
	return ret;
}

static int ktd2801_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd2801_bl_data *ktd2801 = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ktd2801->early_suspend_desc);
#endif

	kfree(ktd2801);

	return 0;
}

static int ktd2801_backlight_shutdown(struct platform_device *pdev)
{
	printk("[BACKLIGHT] ktd2801_backlight_shutdown\n");
	return 0;
}

static struct platform_driver ktd2801_backlight_driver = {
	.driver		= {
		.name	= "panel",
		.owner	= THIS_MODULE,
	},
	.probe		= ktd2801_backlight_probe,
	.remove		= ktd2801_backlight_remove,
	.shutdown      = ktd2801_backlight_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = ktd2801_backlight_suspend,
	.resume         = ktd2801_backlight_resume,
#endif
};

static int __init ktd2801_backlight_init(void)
{
	return platform_driver_register(&ktd2801_backlight_driver);
}

module_init(ktd2801_backlight_init);

static void __exit ktd2801_backlight_exit(void)
{
	platform_driver_unregister(&ktd2801_backlight_driver);
}

module_exit(ktd2801_backlight_exit);
MODULE_DESCRIPTION("ktd2801 based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd2801-backlight");
