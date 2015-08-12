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

#if defined(CONFIG_BCM2079X_NFC_I2C)
#include <linux/nfc/bcm2079x.h>
#endif

#if defined(CONFIG_SEC_NFC_I2C_SB)
#include <linux/nfc/sec_nfc_sb.h>
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

#ifdef CONFIG_BACKLIGHT_TPS61158
#include <linux/tps61158_pwm_bl.h>
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

#if (defined(CONFIG_BCM_RFKILL) || defined(CONFIG_BCM_RFKILL_MODULE) || defined(CONFIG_BCM_BZHW) || defined(CONFIG_BCM_BT_LPM))
#include <mach/dev-hawaii_vivaltods5m_bt.h>
#endif
#ifdef CONFIG_VIDEO_UNICAM_CAMERA
#include <mach/dev-hawaii_vivaltods5m_camera.h>
#endif







void __init hawaii_nfc_init(void);


#if defined(CONFIG_BCM2079X_NFC_I2C)

/* NFC */
#define BCM_NFC_IRQ_GPIO	(90)	/*NFC_IRQ*/
#define BCM_NFC_WAKE_GPIO	(25)	/*NFC_WAKE*/
#define BCM_NFC_EN_GPIO		(100)	/*NFC_EN*/

#define BCM_NFC_SCL_GPIO	(16)	/*NFC_SCL*/
#define BCM_NFC_SDA_GPIO	(17)	/*NFC_SDA*/
#define BCM_NFC_BUSID		(1)


/**
 * If the UART_RXD pin is pulled low (through 0),
 * then the address set is: 0x77, 0x66, 0x1FA, and 0x2FA
 */
#define BCM_NFC_ADDR		(0x77)

static int bcm2079x_gpio_setup(void *);
static int bcm2079x_gpio_clear(void *);
static struct bcm2079x_i2c_platform_data bcm2079x_pdata = {
	.i2c_pdata	= {ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_400K),
		SET_CLIENT_FUNC(TX_FIFO_ENABLE | RX_FIFO_ENABLE)},
	.irq_gpio = BCM_NFC_IRQ_GPIO,
	.en_gpio = BCM_NFC_EN_GPIO,
	.wake_gpio = BCM_NFC_WAKE_GPIO,
	.init = bcm2079x_gpio_setup,
	.reset = bcm2079x_gpio_clear,
};

static int bcm2079x_gpio_setup(void *this)
{

	struct bcm2079x_i2c_platform_data *p;
	p = (struct bcm2079x_i2c_platform_data *) this;
	if (!p)
		return -1;
	pr_info("bcm2079x_gpio_setup nfc en %d, wake %d, irq %d\n",
		p->en_gpio, p->wake_gpio, p->irq_gpio);

	gpio_request(p->irq_gpio, "nfc_irq");
	gpio_direction_input(p->irq_gpio);

	gpio_request(p->en_gpio, "nfc_en");
	gpio_direction_output(p->en_gpio, 1);

	gpio_request(p->wake_gpio, "nfc_wake");
	gpio_direction_output(p->wake_gpio, 0);

	return 0;
}

static int bcm2079x_gpio_clear(void *this)
{

	struct bcm2079x_i2c_platform_data *p;
	p = (struct bcm2079x_i2c_platform_data *) this;
	if (!p)
		return -1;

	pr_info("bcm2079x_gpio_clear nfc en %d, wake %d, irq %d\n",
		p->en_gpio, p->wake_gpio, p->irq_gpio);

	gpio_direction_output(p->en_gpio, 0);
	gpio_direction_output(p->wake_gpio, 1);
	gpio_free(p->en_gpio);
	gpio_free(p->wake_gpio);
	gpio_free(p->irq_gpio);

	return 0;
}

static struct i2c_board_info __initdata bcm2079x[] = {
	{
		I2C_BOARD_INFO("bcm2079x", BCM_NFC_ADDR),
		.flags = I2C_CLIENT_TEN,
		.platform_data = (void *)&bcm2079x_pdata,
		.irq = gpio_to_irq(BCM_NFC_IRQ_GPIO),
	 },

};
#endif

#if defined(CONFIG_SEC_NFC_I2C_SB)
#define GPIO_NFC_IRQ		(90)
#define GPIO_NFC_EN		(25)
#define GPIO_NFC_FIRMWARE	(26)
#define GPIO_NFC_SCL		(16)
#define GPIO_NFC_SDA		(17)
#define NFC_I2C_ADDR		(0x27)

static void sec_nfc_gpio_init(void)
{
};

static struct sec_nfc_platform_data s3fnrn3_pdata = {
	.i2c_pdata	= {ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_400K),
		SET_CLIENT_FUNC(TIMEOUT_DISABLE | TX_FIFO_ENABLE | RX_FIFO_ENABLE)},
	.irq = GPIO_NFC_IRQ,
	.ven = GPIO_NFC_EN,
	.firm = GPIO_NFC_FIRMWARE,
	.cfg_gpio = sec_nfc_gpio_init,
};

static struct i2c_board_info __initdata seci2cnfc[] = {
	{
		I2C_BOARD_INFO(SEC_NFC_DRIVER_NAME, NFC_I2C_ADDR),
		.platform_data = &s3fnrn3_pdata,
		.irq = gpio_to_irq(GPIO_NFC_IRQ),
	},
};

#if defined(CONFIG_SEC_NFC_I2C_TYPE_SW)
static struct i2c_gpio_platform_data nfc_i2c_gpio_data = {
	.sda_pin = GPIO_NFC_SDA,
	.scl_pin = GPIO_NFC_SCL,
	.udelay	 = 2,
	.timeout = 100,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device nfc_i2c_gpio_device = {
	.name = "i2c-gpio",
	.id   = 7,
	.dev  = { 
	.platform_data = &nfc_i2c_gpio_data, 
	},
};
static struct platform_device *nfc_i2c_devices[] __initdata = {
	&nfc_i2c_gpio_device,
};
#endif	// CONFIG_SEC_NFC_I2C_TYPE_SW
#endif	// CONFIG_SEC_NFC_I2C_SB



void __init hawaii_nfc_init(void)
{
	pr_info("hawaii_nfc_init start");
#if defined(CONFIG_BCM2079X_NFC_I2C)
	pr_info("hawaii_nfc_init start - CONFIG_BCM2079X_NFC_I2C");
	i2c_register_board_info(BCM_NFC_BUSID, bcm2079x, ARRAY_SIZE(bcm2079x));
#endif
#if defined(CONFIG_SEC_NFC_I2C_SB)
#if defined(CONFIG_SEC_NFC_I2C_TYPE_SW)
	i2c_register_board_info(7, seci2cnfc, ARRAY_SIZE(seci2cnfc));
	platform_add_devices(nfc_i2c_devices, ARRAY_SIZE(nfc_i2c_devices));
#else
	i2c_register_board_info(1, seci2cnfc, ARRAY_SIZE(seci2cnfc));
#endif
#endif
}

