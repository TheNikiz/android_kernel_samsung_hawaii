/************************************************************************/
/*                                                                      */
/*  Copyright 2012  Broadcom Corporation                                */
/*                                                                      */
/* Unless you and Broadcom execute a separate written software license  */
/* agreement governing use of this software, this software is licensed  */
/* to you under the terms of the GNU General Public License version 2   */
/* (the GPL), available at						*/
/*                                                                      */
/*          http://www.broadcom.com/licenses/GPLv2.php                  */
/*                                                                      */
/*  with the following added to such license:                           */
/*                                                                      */
/*  As a special exception, the copyright holders of this software give */
/*  you permission to link this software with independent modules, and  */
/*  to copy and distribute the resulting executable under terms of your */
/*  choice, provided that you also meet, for each linked independent    */
/*  module, the terms and conditions of the license of that module. An  */
/*  independent module is a module which is not derived from this       */
/*  software.  The special   exception does not apply to any            */
/*  modifications of the software.					*/
/*									*/
/*  Notwithstanding the above, under no circumstances may you combine	*/
/*  this software in any way with any other Broadcom software provided	*/
/*  under a license other than the GPL, without Broadcom's express	*/
/*  prior written consent.						*/
/*									*/
/************************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/bcm590xx/pmic.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <mach/pinmux.h>
#ifdef CONFIG_IOMMU_API
#include <plat/bcm_iommu.h>
#endif
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#ifdef CONFIG_ION_BCM_NO_DT
#include <linux/ion.h>
#include <linux/broadcom/bcm_ion.h>
#endif
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c-kona.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/bootmem.h>
#include <linux/input.h>
#include <linux/mfd/bcm590xx/core.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/i2c-kona.h>
#include <linux/i2c.h>
#include <linux/i2c/tango_ts.h>
#include <linux/i2c/melfas_ts.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <mach/hardware.h>
#include <mach/hardware.h>
#include <mach/kona_headset_pd.h>
#include <mach/kona.h>
#include <mach/sdio_platform.h>
#include <mach/hawaii.h>
#include <mach/io_map.h>
#include <mach/irqs.h>
#include <mach/rdb/brcm_rdb_uartb.h>
#include <mach/clock.h>
#include <plat/spi_kona.h>
#include <plat/chal/chal_trace.h>
#include <plat/pi_mgr.h>
#include <plat/spi_kona.h>
#include <plat/kona_smp.h>

#include <trace/stm.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include "devices.h"

#ifdef CONFIG_KEYBOARD_BCM
#include <mach/bcm_keypad.h>
#endif
#ifdef CONFIG_DMAC_PL330
#include <mach/irqs.h>
#include <plat/pl330-pdata.h>
#include <linux/dma-mapping.h>
#endif

#if defined(CONFIG_SPI_GPIO)
#include <linux/spi/spi_gpio.h>
#endif

#if defined(CONFIG_HAPTIC)
#include <linux/haptic.h>
#endif

#if defined(CONFIG_BCM_BT_RFKILL)
#include <linux/broadcom/bcm-bt-rfkill.h>
#endif

#ifdef CONFIG_BCM_BZHW
#include <linux/broadcom/bcm_bzhw.h>
#endif

#if defined(CONFIG_BCM_BT_LPM)
#include <linux/broadcom/bcm-bt-lpm.h>
#endif

#if defined(CONFIG_BCMI2CNFC)
#include <linux/bcmi2cnfc.h>
#endif

#if defined  (CONFIG_SENSORS_BMC150)
#include <linux/bst_sensor_common.h>
#endif

#if defined(CONFIG_SENSORS_GP2AP002)
#include <linux/gp2ap002_dev.h>
#endif

#if defined(CONFIG_BMP18X) || defined(CONFIG_BMP18X_I2C)
	|| defined(CONFIG_BMP18X_I2C_MODULE)
#include <linux/bmp18x.h>
#include <mach/bmp18x_i2c_settings.h>
#endif

#if defined(CONFIG_AL3006) || defined(CONFIG_AL3006_MODULE)
#include <linux/al3006.h>
#include <mach/al3006_i2c_settings.h>
#endif

#if defined(CONFIG_MPU_SENSORS_MPU6050B1)
	|| defined(CONFIG_MPU_SENSORS_MPU6050B1_MODULE)
#include <linux/mpu.h>
#include <mach/mpu6050_settings.h>
#endif

#if defined(CONFIG_MPU_SENSORS_AMI306)
	|| defined(CONFIG_MPU_SENSORS_AMI306_MODULE)
#include <linux/ami306_def.h>
#include <linux/ami_sensor.h>
#include <mach/ami306_settings.h>
#endif

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
#include <linux/input.h>
#include <linux/gpio_keys.h>
#endif


#ifdef CONFIG_BACKLIGHT_PWM
#include <linux/pwm_backlight.h>
#endif

#ifdef CONFIG_BACKLIGHT_KTD259B
#include <linux/ktd259b_bl.h>
#endif

#ifdef CONFIG_FB_BRCM_KONA
#include <video/kona_fb.h>
#endif

#if defined(CONFIG_BCM_ALSA_SOUND)
#include <mach/caph_platform.h>
#include <mach/caph_settings.h>
#endif
#ifdef CONFIG_UNICAM_CAMERA
#include <media/soc_camera.h>
#include "../../../drivers/media/i2c/soc_camera/camdrv_ss.h"
#endif

#ifdef CONFIG_VIDEO_KONA
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/kona-unicam.h>
#endif

#ifdef CONFIG_WD_TAPPER
#include <linux/broadcom/wd-tapper.h>
#endif

#ifdef CONFIG_I2C_GPIO
#include <linux/i2c-gpio.h>
#endif

#if defined(CONFIG_RT8969) || defined(CONFIG_RT8973)
#include <linux/mfd/bcmpmu.h>
#include <linux/power_supply.h>
#include <linux/platform_data/rtmusc.h>
#endif

#ifdef CONFIG_USB_SWITCH_TSU6111
#include <linux/mfd/bcmpmu.h>
#include <linux/power_supply.h>
#include <linux/i2c/tsu6111.h>
#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
/* Samsung charging feature */
#include <linux/spa_power.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_BCM915500)
	|| defined(CONFIG_TOUCHSCREEN_BCM915500_MODULE)
#include <linux/i2c/bcmtch15xxx.h>
#endif

#ifdef CONFIG_USB_DWC_OTG
#include <linux/usb/bcm_hsotgctrl.h>
#include <linux/usb/otg.h>
#endif

#ifdef CONFIG_MOBICORE_DRIVER
#include <linux/broadcom/mobicore.h>
#endif


#ifdef CONFIG_BRCM_UNIFIED_DHD_SUPPORT
#include "hawaii_wifi.h"

/* void send_usb_insert_event(enum bcmpmu_event_t event, void *para);*/
void send_chrgr_insert_event(enum bcmpmu_event_t event, void *para);

extern int hawaii_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id);
#endif
extern void  hawaii_timer_init(void);


#if defined(CONFIG_BCM_BT_RFKILL)
#define BCMBT_VREG_GPIO		28
#define BCMBT_N_RESET_GPIO	71

static struct bcm_bt_rfkill_platform_data hawaii_bcm_bt_rfkill_cfg = {
	.bcm_bt_rfkill_vreg_gpio = BCMBT_VREG_GPIO,
	.bcm_bt_rfkill_n_reset_gpio = BCMBT_N_RESET_GPIO,
};

static struct platform_device hawaii_bcm_bt_rfkill_device = {
	.name = "bcm-bt-rfkill",
	.id = -1,
	.dev =	{
		.platform_data = &hawaii_bcm_bt_rfkill_cfg,
	},
};
#endif

#ifdef CONFIG_BCM_BZHW
#define GPIO_BT_WAKE	32
#define GPIO_HOST_WAKE	72
static struct bcm_bzhw_platform_data bcm_bzhw_data = {
	.gpio_bt_wake = GPIO_BT_WAKE,
	.gpio_host_wake = GPIO_HOST_WAKE,
};

static struct platform_device hawaii_bcm_bzhw_device = {
	.name = "bcm_bzhw",
	.id = -1,
	.dev =	{
		.platform_data = &bcm_bzhw_data,
	},
};
#endif


#if defined(CONFIG_BCM_BT_LPM)
#define GPIO_BT_WAKE	32
#define GPIO_HOST_WAKE	72

static struct bcm_bt_lpm_platform_data brcm_bt_lpm_data = {
	.bt_wake_gpio = GPIO_BT_WAKE,
	.host_wake_gpio = GPIO_HOST_WAKE,
};

static struct platform_device board_bcm_bt_lpm_device = {
	.name = "bcm-bt-lpm",
	.id = -1,
	.dev = {
		.platform_data = &brcm_bt_lpm_data,
	},
};
#endif




static struct platform_device *hawaii_bt_devices[] __initdata = {

#if defined(CONFIG_BCM_BT_RFKILL)
	&hawaii_bcm_bt_rfkill_device,
#endif

#ifdef CONFIG_BCM_BZHW
	&hawaii_bcm_bzhw_device,
#endif

#if defined(CONFIG_BCM_BT_LPM)
	&board_bcm_bt_lpm_device,
#endif

};


void __init hawaii_bt_init(void)
{
	platform_add_devices(hawaii_bt_devices, ARRAY_SIZE(hawaii_bt_devices));
return;
}
