/*******************************************************************************
* Copyright 2011 Broadcom Corporation.	All rights reserved.
*
* @file drivers/video/broadcom/lcd/vivaltonfc_dsi.c
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <video/kona_fb.h>
#include <linux/broadcom/mobcom_types.h>
#include "dispdrv_common.h"

#undef dev_dbg
#define dev_dbg dev_info
#define ESD_OPERATION
#define ESD_PORT_NUM 1

extern int panel_read(UInt8 reg, UInt8 *rxBuff, UInt8 buffLen);
extern int panel_read_HSmode(UInt8 reg, UInt8 *rxBuff, UInt8 buffLen);
extern void kona_fb_esd_hw_reset(void);
extern int kona_fb_obtain_lock(void);
extern int kona_fb_release_lock(void);
#ifdef CONFIG_BACKLIGHT_KTD2801		
extern int tune_level;
extern void backlight_control(int brigtness);
#endif
#ifdef CONFIG_LCD_CLASS_DEVICE
extern struct class *lcd_class;
struct device *lcd_dev;
#endif

#ifdef CONFIG_FB_BRCM_LCD_EXIST_CHECK
extern int lcd_exist;
#endif

struct vivaltonfc_dsi_lcd {
	struct device	*dev;
	struct mutex	lock;
	bool			panel_awake;	
#ifdef ESD_OPERATION
	unsigned int	lcd_connected;
	unsigned int	esd_enable;
	unsigned int	esd_port;
	bool	esd_processing; 	
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	earlysuspend;
#endif
};

struct vivaltonfc_dsi_lcd *lcd = NULL;
u8 gPanelID[3];
int multi_lcd_num = 3;
int panel1_id_checked;
int panel2_id_checked;
int panel3_id_checked;
int id1, id2, id3=0;

#define PANEL1_ID_CHECK_CONDITION (id1==0x55)&&(id2==0xBF)&&(id3==0x90)
#define PANEL2_ID_CHECK_CONDITION (id1==0x55)&&(id2==0xBC)&&(id3==0x90)
static int panel_read_id_verify (void)
{
	int ret=-1;
	if(PANEL1_ID_CHECK_CONDITION){ // BOE_2
		ret=LCD_PANEL_ID_ONE;
		printk("[LCD] BOE_2 panel, ret=%d\n", ret);
	}
	if(PANEL2_ID_CHECK_CONDITION){ // BOE_1
		ret=LCD_PANEL_ID_TWO;
		printk("[LCD] BOE_1 panel, ret=%d\n", ret);
	}
	return ret;
}

int sBoot_panel_read_id(char *boot_id)
{
	int ret = -1;
	static char boot_panel_id[6];
	/* Read panel ID*/
	if (boot_id[0] != 0xFF) {
		int i;
		for (i=0; i < 6 ; i++) {
			if (boot_id[i] >47 && boot_id[i] <58)
				boot_panel_id[i] = boot_id[i] -48;
			else
				boot_panel_id[i] = boot_id[i] -87;
		}
		id1 = boot_panel_id[0]*16 + boot_panel_id[1];
		id2 = boot_panel_id[2]*16 + boot_panel_id[3];
		id3 = boot_panel_id[4]*16 + boot_panel_id[5];
		printk("[LCD] from bootloder id1 = %x, id2 = %x, id3 = %x,\n", id1, id2, id3);
		ret = panel_read_id_verify ();
	}
	return ret;
}
EXPORT_SYMBOL(sBoot_panel_read_id);
int panel_read_id(char *panel_name)
{
	int ret=0;
	int read_HS = 0;
	multi_lcd_num = multi_lcd_num -1;
	/* Read panel ID*/
	kona_fb_obtain_lock();
	/* For NT35510_BOE read panel id in HS mode */
	if(strcmp(panel_name, "NT35510") == 0) {
		read_HS = 1;
	}
	if(read_HS)
		ret = panel_read_HSmode(0xDA, gPanelID, 1);
	else
	ret=panel_read(0xDA, gPanelID, 1);	
	
	if(ret<0)
		goto read_error;	
	id1= gPanelID[0];

	if(read_HS)
		ret=panel_read_HSmode(0xDB, gPanelID, 1);	
	else
	ret=panel_read(0xDB, gPanelID, 1);	
	
	if(ret<0)
		goto read_error;
	id2= gPanelID[0];
	
	if(read_HS)
		ret=panel_read_HSmode(0xDC, gPanelID, 1);
	else
	ret=panel_read(0xDC, gPanelID, 1);

	if(ret<0)
		goto read_error;
	id3= gPanelID[0];	
	
	printk("[LCD] id1 = %x, id2 = %x, id3 = %x,\n", id1, id2, id3);	

	if(!panel1_id_checked){
		if((id1==0x55)&&(id2==0xBC)&&(id3==0x90)){ // BOE_1	
			ret=LCD_PANEL_ID_ONE;
			printk("[LCD] BOE_1 panel, ret=%d\n", ret);
		}
		else{
			ret= -1;
		}
		panel1_id_checked=1;
		goto read_error;
	}
	
	if(!panel2_id_checked){
		if((id1==0x55)&&(id2==0xBE)&&(id3==0x90)){ // BOE_2
			ret=LCD_PANEL_ID_TWO;
			printk("[LCD] BOE_2 panel, ret=%d\n", ret);			
		}
		else{
			ret= -1;
		}
		panel2_id_checked=1;
		goto read_error;
	}

	if(!panel3_id_checked){
		if((id1==0x55)&&(id2==0xBF)&&(id3==0x90)){ // BOE_3
			ret=LCD_PANEL_ID_THREE;
			printk("[LCD] BOE_3 panel, ret=%d\n", ret);			
		}
		else{
			ret= -1;
		}
		panel3_id_checked=1;
		goto read_error;
	}
	read_error:
	kona_fb_release_lock();	
	if((multi_lcd_num==0)&&(ret<0))
		ret=LCD_PANEL_NOT_CONNECTION;
	printk("[LCD] %s : ret=%d\n", __func__, ret);
	return ret;	
}
EXPORT_SYMBOL(panel_read_id);

#ifdef CONFIG_LCD_CLASS_DEVICE
static ssize_t show_lcd_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[20] = { 0 };
	
	if(id2==0xBC)
	sprintf(temp, "BOE_55BC90\n");
	else if(id2==0xBE)
	sprintf(temp, "BOE_55BE90\n");	
	else if(id2==0xBF)
	sprintf(temp, "BOE_55BF90\n");	
	
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, show_lcd_info, NULL);

static struct attribute *sec_lcd_attributes[] = {
&dev_attr_lcd_type.attr,
NULL,
};

static struct attribute_group lcd_attr_group = {
.attrs = sec_lcd_attributes,
};
#endif/*CONFIG_LCD_CLASS_DEVICE*/

#ifdef ESD_OPERATION

static irqreturn_t esd_interrupt_handler(int irq, void *data)
{
	struct vivaltonfc_dsi_lcd *lcd = data;

	printk("[LCD] %s(), esd_enable=%d, esd_processing=%d, gpio_input=%d\n",
		__func__, lcd->esd_enable, lcd->esd_processing, gpio_get_value(lcd->esd_port));

	if (gpio_get_value(lcd->esd_port) == 0) {
		printk("[LCD] %s(), skip esd_interrupt_handlert because gpio_input is low\n",
			__func__);

		return IRQ_HANDLED;
	}

	if (lcd->esd_enable && !lcd->esd_processing/* && !battpwroff_charging*/) {
		pr_info("ESD PORT =[%d]\n", gpio_get_value(lcd->esd_port));
		
		lcd->esd_processing = true; 
		
		mutex_lock(&lcd->lock);		
		kona_fb_esd_hw_reset();
		mutex_unlock(&lcd->lock);			
		
		msleep(500);
	#ifdef CONFIG_BACKLIGHT_KTD2801		
		backlight_control(tune_level);
	#endif
    		lcd->esd_processing = false; 
	}
	
	return IRQ_HANDLED;
}
#endif	/*ESD_OPERATION*/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vivaltonfc_dsi_early_suspend(struct early_suspend *earlysuspend)
{
  struct vivaltonfc_dsi_lcd *lcd = container_of(earlysuspend, struct vivaltonfc_dsi_lcd, earlysuspend);

#ifdef ESD_OPERATION
#ifdef CONFIG_FB_BRCM_LCD_EXIST_CHECK
	if (lcd_exist) 
#endif
	{
	if (lcd->lcd_connected) {
		lcd->esd_enable = 0;
		disable_irq_nosync(gpio_to_irq(lcd->esd_port));	
		dev_dbg(lcd->dev,"disable esd operation : %d\n", lcd->esd_enable);
	}
	if (lcd->panel_awake) {
			lcd->panel_awake = false;		
			dev_dbg(lcd->dev, "panel goes to sleep\n");	
	}
	}
#endif
}

static void vivaltonfc_dsi_late_resume(struct early_suspend *earlysuspend)
{
  struct vivaltonfc_dsi_lcd *lcd = container_of(earlysuspend, struct vivaltonfc_dsi_lcd, earlysuspend);

#ifdef ESD_OPERATION
#ifdef CONFIG_FB_BRCM_LCD_EXIST_CHECK
	if (lcd_exist) 
#endif
	{
	if (!lcd->panel_awake) {
			lcd->panel_awake = true;
			dev_dbg(lcd->dev, "panel is ready\n");			
	}
#ifdef ESD_OPERATION
		if (lcd->lcd_connected) {
			enable_irq(gpio_to_irq(lcd->esd_port));
			irq_set_irq_type(gpio_to_irq(lcd->esd_port), IRQF_TRIGGER_RISING);
			lcd->esd_enable = 1;
			dev_dbg(lcd->dev, "change lcd->esd_enable :%d\n",lcd->esd_enable);
		}
#endif
	}
#endif	
	}
#endif

static int vivaltonfc_panel_probe(struct platform_device *pdev)
{
	int ret = 0;

	lcd = kzalloc(sizeof(struct vivaltonfc_dsi_lcd), GFP_KERNEL);
	if (!lcd)
		return -ENOMEM;

	printk("%s function entered\n", __func__);	
	
	lcd->dev = &pdev->dev;	
	platform_set_drvdata(pdev, lcd);	
	lcd->panel_awake = true;

	mutex_init(&lcd->lock);	

	lcd->lcd_connected = 1;	

#ifdef ESD_OPERATION
	lcd->esd_port = ESD_PORT_NUM;

	ret = gpio_request(ESD_PORT_NUM, "lcd_detect");
	if (ret) {
		printk("unable to request gpio.\n");
	}
	ret = gpio_direction_input(ESD_PORT_NUM);
#ifdef CONFIG_FB_BRCM_LCD_EXIST_CHECK	
	if (lcd_exist)	
#endif
	{
	if (request_threaded_irq(gpio_to_irq(lcd->esd_port), NULL,
			esd_interrupt_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "esd_interrupt", lcd)) {
			dev_info(lcd->dev, "esd irq request fail\n");
			free_irq(gpio_to_irq(lcd->esd_port), NULL);
			lcd->lcd_connected = 0;
	}
        }
#endif	

#ifdef CONFIG_LCD_CLASS_DEVICE
	/* sys fs */
	lcd_dev = device_create(lcd_class, NULL, 0, NULL, "panel");
	if (IS_ERR(lcd_dev))
		pr_err("Failed to create device(lcd)!\n");
	if (sysfs_create_group(&lcd_dev->kobj, &lcd_attr_group))
		pr_err("Failed to create sysfs group(%s)!\n", "lcd_type");	
#endif

	lcd->esd_processing = false; 
	lcd->esd_enable = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
	lcd->earlysuspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	lcd->earlysuspend.suspend = vivaltonfc_dsi_early_suspend;
	lcd->earlysuspend.resume  = vivaltonfc_dsi_late_resume;
	register_early_suspend(&lcd->earlysuspend);
#endif	

	return ret;
}

static int vivaltonfc_panel_remove(struct platform_device *pdev)
{
	int ret = 0;
		
	struct vivaltonfc_dsi_lcd *lcd = platform_get_drvdata(pdev);

	dev_dbg(lcd->dev, "%s function entered\n", __func__);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
  unregister_early_suspend(&lcd->earlysuspend);
#endif

	kfree(lcd);	

	return ret;
}

static struct platform_driver vivaltonfc_panel_driver = {
	.driver		= {
		.name	= "HX8369",
		.owner	= THIS_MODULE,
	},
	.probe		= vivaltonfc_panel_probe,
	.remove		= vivaltonfc_panel_remove,
};
static int __init vivaltonfc_panel_init(void)
{
	return platform_driver_register(&vivaltonfc_panel_driver);
}

static void __exit vivaltonfc_panel_exit(void)
{
	platform_driver_unregister(&vivaltonfc_panel_driver);
}

late_initcall_sync(vivaltonfc_panel_init);
module_exit(vivaltonfc_panel_exit);

MODULE_DESCRIPTION("vivaltonfc panel control driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:vivaltonfc");
