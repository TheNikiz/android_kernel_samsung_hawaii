#ifndef __KONA_FB_H__
#define __KONA_FB_H__


#ifdef KONA_FB_DEBUG
#define konafb_debug(fmt, arg...)	\
	printk(KERN_INFO"%s:%d " fmt, __func__, __LINE__, ##arg)
#else
#define konafb_debug(fmt, arg...)	\
	do {	} while (0)
#endif /* KONA_FB_DEBUG */


#define konafb_info(fmt, arg...)	\
	printk(KERN_INFO"%s:%d " fmt, __func__, __LINE__, ##arg)

#define konafb_error(fmt, arg...)	\
	printk(KERN_ERR"%s:%d " fmt, __func__, __LINE__, ##arg)

#define konafb_warning(fmt, arg...)	\
	printk(KERN_WARNING"%s:%d " fmt, __func__, __LINE__, ##arg)

#define konafb_alert(fmt, arg...)	\
	printk(KERN_ALERT"%s:%d " fmt, __func__, __LINE__, ##arg)


extern void *DISP_DRV_GetFuncTable(void);
#ifdef CONFIG_FB_BRCM_CP_CRASH_DUMP_IMAGE_SUPPORT
extern int crash_dump_ui_on;
extern unsigned ramdump_enable;
#endif

#if defined(CONFIG_MACH_HAWAII_SS_HEAT3G) || defined(CONFIG_MACH_HAWAII_SS_HEATNFC3G)
#include "lcd/nt35510_boe.h"
#include "lcd/ili9806_boe.h"
#endif
#ifdef CONFIG_LCD_HX8369_CS02_SUPPORT
#include "lcd/hx8369_cs02.h"
#endif
#ifdef CONFIG_LCD_SC7798_CS02_SUPPORT
#include "lcd/sc7798_cs02.h"
#endif
#ifdef CONFIG_LCD_HX8369_BOE_PANEL_SUPPORT
#include "lcd/hx8369.h"
#endif
#ifdef CONFIG_LCD_S6E63M0X_SUPPORT
#include "lcd/s6e63m0x3.h"
#endif

#include "lcd/nt35510.h"
#include "lcd/nt35512.h"
#include "lcd/nt35516.h"
#include "lcd/nt35517.h"
#include "lcd/nt35596.h"
#include "lcd/otm1281a.h"
#include "lcd/otm1283a.h"
#include "lcd/otm8018b.h"
#include "lcd/otm8018b_tm.h"
#include "lcd/otm8009a.h"
#include "lcd/ili9806c.h"
#include "lcd/hx8379_tm.h"
#include "lcd/hx8379_hsd.h"
#include "lcd/nt35512_gp.h"
#include "lcd/auo080100.h"
#include "lcd/r63311.h"
#include "lcd/simulator.h"
#include "lcd/s6e63m0x3.h"
#include "lcd/s6d04k2x01.h"

#ifdef CONFIG_LCD_SC7798_I9060_SUPPORT
#include "lcd/sc7798_I9060.h"
#endif

#ifdef CONFIG_LCD_HX8369_BOE_PANEL_SUPPORT
#include "lcd/hx8369_boe_panel.h"
#endif

#ifdef CONFIG_LCD_HX8369_VIVALTO3G_SUPPORT
#include "lcd/hx8369_boe_1.h"
#include "lcd/hx8369_boe_2.h"
#endif


int lcd_exist = 1; //  1 means LCD exist
EXPORT_SYMBOL(lcd_exist);

static struct lcd_config *cfgs[] __initdata = {
#ifdef CONFIG_LCD_HX8369_BOE_PANEL_SUPPORT
      &hx8369_cfg,
#endif

#ifdef CONFIG_LCD_HX8369_VIVALTO3G_SUPPORT
	&hx8369_boe_cfg,
	&hx8369_boe_2_cfg,
#endif
#ifdef CONFIG_LCD_HX8369_CS02_SUPPORT
	&hx8369_cfg,
#endif
#if (defined(CONFIG_LCD_SC7798_I9060_SUPPORT)|| \
defined(CONFIG_LCD_SC7798_CS02_SUPPORT))
	&sc7798_cfg,
#endif
	&nt35512_cfg,
	&nt35516_cfg,
	&nt35596_cfg,
#ifdef CONFIG_LCD_S6E63M0X_SUPPORT
	&s6e63m0x3_cfg,
#endif

#if defined(CONFIG_MACH_HAWAII_SS_HEAT3G) || defined(CONFIG_MACH_HAWAII_SS_HEATNFC3G)
	&ili9806_cfg,
	&nt35510_cfg,
#endif

	&nt35517_cfg,
	&otm1281a_cfg,
	&otm1283a_cfg,
	&otm8018b_cfg,
	&otm8018b_tm_cfg,
	&otm8009a_cfg,
	&ili9806c_cfg,

	&hx8379_tm_cfg,
	&hx8379_hsd_cfg,
	&nt35512_gp_cfg,
	&auo080100_cfg,
	&r63311_cfg,
	&simulator_cfg,
	&s6d04k2x01_cfg,
};

static struct lcd_config * __init get_dispdrv_cfg(const char *name)
{
	int i;
	void *ret = NULL;
	i = sizeof(cfgs) / sizeof(struct lcd_config *);
	while (i--) {
		if (!strcmp(name, cfgs[i]->name)) {
			ret = cfgs[i];
			pr_err("Found a match for %s\n", cfgs[i]->name);
			break;
		}
	}
	return ret;
}

#endif /* __KONA_FB_H__ */
