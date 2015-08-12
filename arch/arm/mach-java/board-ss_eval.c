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
#include <asm/gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/i2c-kona.h>
#include <linux/i2c.h>
#ifdef CONFIG_TOUCHSCREEN_TANGO
#include <linux/i2c/tango_ts.h>
#endif
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

#if (defined(CONFIG_BCM_RFKILL) || defined(CONFIG_BCM_RFKILL_MODULE))
#include <linux/broadcom/bcmbt_rfkill.h>
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

#if defined(CONFIG_SENSORS_BMA2X2)
#include <linux/bma222.h>
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
#ifdef CONFIG_SOC_CAMERA
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


#ifdef CONFIG_KONA_SECURE_MEMC
#include <plat/kona_secure_memc.h>
#include <plat/kona_secure_memc_settings.h>
#endif
#ifdef CONFIG_BRCM_UNIFIED_DHD_SUPPORT
#include "java_wifi.h"
extern int hawaii_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
			    void *dev_id);
#endif

void send_usb_insert_event(enum bcmpmu_event_t event, void *para);
void send_chrgr_insert_event(enum bcmpmu_event_t event, void *para);

#include <linux/bmm050.h>
#include <linux/bma150.h>

#include <linux/gp2ap002_dev.h>
#include <linux/gp2ap002.h>
#include <linux/broadcom/secure_memory.h>

extern void java_timer_init(void);

/* NFC */
#define NFC_INT	90
#define NFC_WAKE 25
#define NFC_ENABLE 100

int reset_pwm_padcntrl(void)
{
	struct pin_config new_pin_config;
	int ret;
	new_pin_config.name = PN_GPIO24;
	new_pin_config.func = PF_GPIO24;
	ret = pinmux_set_pin_config(&new_pin_config);
	return ret;
}

#ifdef CONFIG_SOC_MAIN_CAMERA
#define CAMDRV_SS_MAIN_I2C_ADDRESS (0xAC>>1)
static struct i2c_board_info hawaii_i2c_camera = 
{
	I2C_BOARD_INFO("camdrv_ss", CAMDRV_SS_MAIN_I2C_ADDRESS),

};
static int hawaii_camera_power(struct device *dev, int on)
{
	static struct pi_mgr_dfs_node unicam_dfs_node;
	int ret;
	printk(KERN_INFO "%s:camera power %s, %d\n", __func__,
		(on ? "on" : "off"), unicam_dfs_node.valid);
	if (!unicam_dfs_node.valid) {
		ret = pi_mgr_dfs_add_request(&unicam_dfs_node, "unicam",
						PI_MGR_PI_ID_MM,
						PI_MGR_DFS_MIN_VALUE);
		if (ret) {
			printk(
			KERN_ERR "%s: failed to register PI DFS request\n",
			__func__
			);
			return -1;
		}
	}

	if (on) {
		if (pi_mgr_dfs_request_update(&unicam_dfs_node, PI_OPP_TURBO)) {
			printk(
			KERN_ERR "%s:failed to update dfs request for unicam\n",
			__func__
			);
			return -1;
		}
	}

	if (!camdrv_ss_power(0, (bool)on)) {
		printk(
		KERN_ERR "%s,camdrv_ss_power failed for MAIN CAM!!\n",
		__func__
		);
		return -1;
	}

	if (!on) {
		if (pi_mgr_dfs_request_update(&unicam_dfs_node,
						PI_MGR_DFS_MIN_VALUE)) {
			printk(
			KERN_ERR"%s: failed to update dfs request for unicam\n",
			__func__);
		}
	}
	return 0;
}
static int hawaii_camera_reset(struct device *dev)
{
	/* reset the camera gpio */
	printk(KERN_INFO "%s:camera reset\n", __func__);
	return 0;
}


static struct v4l2_subdev_sensor_interface_parms camdrv_ss_main_if_params = {
	.if_type = V4L2_SUBDEV_SENSOR_SERIAL,
	.if_mode = V4L2_SUBDEV_SENSOR_MODE_SERIAL_CSI2,
	.orientation = V4L2_SUBDEV_SENSOR_ORIENT_90,
	.facing = V4L2_SUBDEV_SENSOR_BACK,
	.parms.serial = {
		 .lanes = 2,
		 .channel = 0,
		 .phy_rate = 0,
		 .pix_clk = 0,
		 .hs_term_time = 0x7
	},
};

static struct soc_camera_desc iclink_camdrv_ss_main = {
   .host_desc = {
   .bus_id = 0,
   .board_info = &hawaii_i2c_camera,
   .i2c_adapter_id = 0,
   .module_name = "camdrv_ss",
   },
   .subdev_desc = {
   .power = &hawaii_camera_power,
   .reset = &hawaii_camera_reset,
           .drv_priv =  &camdrv_ss_main_if_params,
           .flags = 0,
   },
};

struct platform_device hawaii_camera = {
	.name = "soc-camera-pdrv",
	 .id = 0,
	 .dev = {
		 .platform_data = &iclink_camdrv_ss_main,
	 },
};
#endif /*CONFIG_SOC_MAIN_CAMERA*/


#ifdef CONFIG_SOC_SUB_CAMERA
#define CAMDRV_SS_SUB_I2C_ADDRESS (0x60>>1)
static struct i2c_board_info hawaii_i2c_camera_sub = 
{
		I2C_BOARD_INFO("camdrv_ss_sub", CAMDRV_SS_SUB_I2C_ADDRESS),
};

static int hawaii_camera_power_sub(struct device *dev, int on)
{
	static struct pi_mgr_dfs_node unicam_dfs_node;
	int ret;

	printk(KERN_INFO "%s:camera power %s, %d\n", __func__,
		(on ? "on" : "off"), unicam_dfs_node.valid);

	if (!unicam_dfs_node.valid) {
		ret = pi_mgr_dfs_add_request(&unicam_dfs_node, "unicam",
						PI_MGR_PI_ID_MM,
						PI_MGR_DFS_MIN_VALUE);
		if (ret) {
			printk(
			KERN_ERR "%s: failed to register PI DFS request\n",
			__func__
			);
			return -1;
		}
	}

	if (on) {
		if (pi_mgr_dfs_request_update(&unicam_dfs_node, PI_OPP_TURBO)) {
			printk(
			KERN_ERR "%s:failed to update dfs request for unicam\n",
			 __func__
			);
			return -1;
		}
	}

	if (!camdrv_ss_power(1, (bool)on)) {
		printk(KERN_ERR "%s, camdrv_ss_power failed for SUB CAM!!\n",
		__func__);
		return -1;
	}

	if (!on) {
		if (pi_mgr_dfs_request_update(&unicam_dfs_node,
						PI_MGR_DFS_MIN_VALUE)) {
			printk(
			KERN_ERR"%s: failed to update dfs request for unicam\n",
			__func__);
		}
	}

	return 0;
}


static int hawaii_camera_reset_sub(struct device *dev)
{
	/* reset the camera gpio */
	printk(KERN_INFO "%s:camera reset\n", __func__);
	return 0;

}
static struct v4l2_subdev_sensor_interface_parms camdrv_ss_sub_if_params = {
	.if_type = V4L2_SUBDEV_SENSOR_SERIAL,
	.if_mode = V4L2_SUBDEV_SENSOR_MODE_SERIAL_CSI2,
	.orientation = V4L2_SUBDEV_SENSOR_ORIENT_270,
	.facing = V4L2_SUBDEV_SENSOR_FRONT,
	.parms.serial = {
		.lanes = 1,
		.channel = 1,
		.phy_rate = 0,
		.pix_clk = 0,
		.hs_term_time = 0x7
	},
};
static struct soc_camera_desc iclink_camdrv_ss_sub = {
	.host_desc = {
		.bus_id = 0,
		.board_info = &hawaii_i2c_camera_sub,
		.i2c_adapter_id = 0,
		.module_name = "camdrv_ss",
	},
	.subdev_desc = {
		.power = &hawaii_camera_power_sub,
		.reset = &hawaii_camera_reset_sub,
		.drv_priv =  &camdrv_ss_sub_if_params,
		.flags = 1,
   },
};

struct platform_device hawaii_camera_sub = {
	.name	= "soc-camera-pdrv",
	.id		= 1,
	.dev	= {
		.platform_data = &iclink_camdrv_ss_sub,
	},
};
#endif /*CONFIG_SOC_SUB_CAMERA*/





static struct spi_kona_platform_data hawaii_ssp0_info = {
#ifdef CONFIG_DMAC_PL330
	.enable_dma = 1,
#else
	.enable_dma = 0,
#endif
	.cs_line = 1,
	.mode = SPI_LOOP | SPI_MODE_3,
};

static struct spi_kona_platform_data hawaii_ssp1_info = {
#ifdef CONFIG_DMAC_PL330
	.enable_dma = 1,
#else
	.enable_dma = 0,
#endif
};

#ifdef CONFIG_STM_TRACE
static struct stm_platform_data hawaii_stm_pdata = {
	.regs_phys_base = STM_BASE_ADDR,
	.channels_phys_base = SWSTM_BASE_ADDR,
	.id_mask = 0x0,		/* Skip ID check/match */
	.final_funnel = CHAL_TRACE_FIN_FUNNEL,
};
#endif

#if defined(CONFIG_USB_DWC_OTG)
static struct bcm_hsotgctrl_platform_data hsotgctrl_plat_data = {
	.hsotgctrl_virtual_mem_base = KONA_USB_HSOTG_CTRL_VA,
	.chipreg_virtual_mem_base = KONA_CHIPREG_VA,
	.irq = BCM_INT_ID_HSOTG_WAKEUP,
	.usb_ahb_clk_name = USB_OTG_AHB_BUS_CLK_NAME_STR,
	.mdio_mstr_clk_name = MDIOMASTER_PERI_CLK_NAME_STR,
};
#endif

struct platform_device *hawaii_common_plat_devices[] __initdata = {

	&pmu_device,
	&hawaii_pwm_device,
	&hawaii_ssp0_device,

#ifdef CONFIG_SENSORS_KONA
	&thermal_device,
#endif

#ifdef CONFIG_STM_TRACE
	&hawaii_stm_device,
#endif

#if defined(CONFIG_HW_RANDOM_KONA)
	&rng_device,
#endif

#if defined(CONFIG_USB_DWC_OTG)
#ifndef CONFIG_OF
	&hawaii_usb_phy_platform_device,
	&hawaii_hsotgctrl_platform_device,
	&hawaii_otg_platform_device,
#endif
#endif

#ifdef CONFIG_KONA_AVS
	&avs_device,
#endif

#ifdef CONFIG_KONA_CPU_FREQ_DRV
	&kona_cpufreq_device,
#endif

#ifdef CONFIG_CRYPTO_DEV_BRCM_SPUM_HASH
	&hawaii_spum_device,
#endif

#ifdef CONFIG_CRYPTO_DEV_BRCM_SPUM_AES
	&hawaii_spum_aes_device,
#endif

#ifdef CONFIG_BRCM_CDC
        &brcm_cdc_device,
#endif

#ifdef CONFIG_UNICAM
	&hawaii_unicam_device,
#endif
#ifdef CONFIG_UNICAM_CAMERA
	&hawaii_camera_device,
#endif
#ifdef CONFIG_SOC_MAIN_CAMERA
	&hawaii_camera,
#endif
#ifdef CONFIG_SOC_SUB_CAMERA
	&hawaii_camera_sub,
#endif

#ifdef CONFIG_SND_BCM_SOC
	&hawaii_audio_device,
	&caph_i2s_device,
	&caph_pcm_device,
	&spdif_dit_device,

#endif
#ifdef CONFIG_KONA_SECURE_MEMC
	&kona_secure_memc_device,
#endif

};


#define GPIO_KEYS_SETTINGS { \
	{ KEY_HOME, 10, 1, "HOME", EV_KEY, 0, 64},	\
}

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_button board_gpio_keys[] = GPIO_KEYS_SETTINGS;

static struct gpio_keys_platform_data gpio_keys_data = {
	.nbuttons = ARRAY_SIZE(board_gpio_keys),
	.buttons = board_gpio_keys,
};

static struct platform_device board_gpio_keys_device = {
	.name = "gpio-keys",
	.id = -1,
	.dev = {
		.platform_data = &gpio_keys_data,
	},
};
#endif

#if defined(CONFIG_BCMI2CNFC)
static int bcmi2cnfc_gpio_setup(void *);
static int bcmi2cnfc_gpio_clear(void *);
static struct bcmi2cnfc_i2c_platform_data bcmi2cnfc_pdata = {
	.i2c_pdata	= {ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_400K),
		SET_CLIENT_FUNC(TX_FIFO_ENABLE | RX_FIFO_ENABLE)},
	.irq_gpio = NFC_INT,
	.en_gpio = NFC_ENABLE,
	.wake_gpio = NFC_WAKE,
	.init = bcmi2cnfc_gpio_setup,
	.reset = bcmi2cnfc_gpio_clear,
};

static int bcmi2cnfc_gpio_setup(void *this)
{
	return 0;
}
static int bcmi2cnfc_gpio_clear(void *this)
{
	return 0;
}

static struct i2c_board_info __initdata bcmi2cnfc[] = {
	{
		I2C_BOARD_INFO("bcmi2cnfc", 0x1F0),
		.flags = I2C_CLIENT_TEN,
		.platform_data = (void *)&bcmi2cnfc_pdata,
		.irq = gpio_to_irq(NFC_INT),
	},
};
#endif

#if  defined(CONFIG_BMP18X) || defined(CONFIG_BMP18X_I2C)
	|| defined(CONFIG_BMP18X_I2C_MODULE)
static struct i2c_board_info __initdata i2c_bmp18x_info[] = {
	{
		I2C_BOARD_INFO(BMP18X_NAME, BMP18X_I2C_ADDRESS),
	},
};
#endif

#if defined(CONFIG_AL3006) || defined(CONFIG_AL3006_MODULE)
static struct al3006_platform_data al3006_pdata = {
#ifdef AL3006_IRQ_GPIO
	.irq_gpio = AL3006_IRQ_GPIO,
#else
	.irq_gpio = -1,
#endif
};

static struct i2c_board_info __initdata i2c_al3006_info[] = {
	{
		I2C_BOARD_INFO("al3006", AL3006_I2C_ADDRESS),
		.platform_data = &al3006_pdata,
	},
};
#endif

#if defined(CONFIG_MPU_SENSORS_AMI306)
	|| defined(CONFIG_MPU_SENSORS_AMI306_MODULE)
static struct ami306_platform_data ami306_data = AMI306_DATA;
static struct i2c_board_info __initdata i2c_ami306_info[] = {
	{
		I2C_BOARD_INFO(AMI_DRV_NAME, AMI_I2C_ADDRESS),
		.platform_data = &ami306_data,
	},
};
#endif

#if defined(CONFIG_MPU_SENSORS_MPU6050B1)
	|| defined(CONFIG_MPU_SENSORS_MPU6050B1_MODULE)

static struct mpu_platform_data mpu6050_platform_data = {
	.int_config  = MPU6050_INIT_CFG,
	.level_shifter = 0,
	.orientation = MPU6050_DRIVER_ACCEL_GYRO_ORIENTATION,
};

/*static struct ext_slave_platform_data mpu_compass_data =
{
//	.bus = EXT_SLAVE_BUS_SECONDARY,
	.orientation = MPU6050_DRIVER_COMPASS_ORIENTATION,
};*/


static struct i2c_board_info __initdata inv_mpu_i2c0_boardinfo[] = {
	{
		I2C_BOARD_INFO("mpu6050", MPU6050_SLAVE_ADDR),
		.platform_data = &mpu6050_platform_data,
	},
/*	{
		I2C_BOARD_INFO("ami_sensor", MPU6050_COMPASS_SLAVE_ADDR),
		.platform_data = &mpu_compass_data,
		.irq =  gpio_to_irq(3),
	},*/
};

#endif /* CONFIG_MPU_SENSORS_MPU6050B1 */



#if defined  (CONFIG_SENSORS_BMC150)
#define ACC_INT_GPIO_PIN  92
static struct bosch_sensor_specific bss_bma2x2 = {
	.name = "bma2x2" ,
        .place = 4,
        .irq = ACC_INT_GPIO_PIN,
};

static struct bosch_sensor_specific bss_bmm050 = {
	.name = "bmm050" ,
        .place = 4,
};
#endif

#if defined(CONFIG_SENSORS_GP2AP002)
static void gp2ap002_power_onoff(bool onoff)
{
	if (onoff) {
            struct regulator *power_regulator = NULL;
            int ret=0;
            power_regulator = regulator_get(NULL, "tcxldo_uc");
            if (IS_ERR(power_regulator)){
                printk(KERN_ERR "[GP2A] can not get prox_regulator (SENSOR_3.3V) \n");
            } else {
                ret = regulator_set_voltage(power_regulator,3300000,3300000);
                printk(KERN_INFO "[GP2A] regulator_set_voltage : %d\n", ret);
                ret = regulator_enable(power_regulator);
                printk(KERN_INFO "[GP2A] regulator_enable : %d\n", ret);
                regulator_put(power_regulator);
                mdelay(5);
            }
	}
}

static void gp2ap002_led_onoff(bool onoff)
{
        struct regulator *led_regulator = NULL;
        int ret=0;

	if (onoff) {
                led_regulator = regulator_get(NULL, "gpldo1_uc");
                if (IS_ERR(led_regulator)){
                    printk(KERN_ERR "[GP2A] can not get prox_regulator (SENSOR_LED_3.3V) \n");
                } else {
                    ret = regulator_set_voltage(led_regulator,3300000,3300000);
                    printk(KERN_INFO "[GP2A] regulator_set_voltage : %d\n", ret);
                    ret = regulator_enable(led_regulator);
                    printk(KERN_INFO "[GP2A] regulator_enable : %d\n", ret);
                    regulator_put(led_regulator);
                    mdelay(5);
                }
	} else {
                led_regulator = regulator_get(NULL, "gpldo1_uc");
		ret = regulator_disable(led_regulator);
                printk(KERN_INFO "[GP2A] regulator_disable : %d\n", ret);
                regulator_put(led_regulator);
	}
}

#define PROXI_INT_GPIO_PIN  89
static struct gp2ap002_platform_data gp2ap002_platform_data = {
         .power_on = gp2ap002_power_onoff,
         .led_on = gp2ap002_led_onoff,
         .irq_gpio = PROXI_INT_GPIO_PIN,
         .irq = gpio_to_irq(PROXI_INT_GPIO_PIN),
};
#endif

#if defined(CONFIG_SENSORS_BMC150) || defined(CONFIG_SENSORS_GP2AP002)
static struct i2c_board_info __initdata bsc3_i2c_boardinfo[] =
{
#if defined(CONFIG_SENSORS_BMC150)
        {
			I2C_BOARD_INFO("bma2x2", 0x10),
			.platform_data = &bss_bma2x2,
        },
        {
			I2C_BOARD_INFO("bmm050", 0x12),
			.platform_data = &bss_bmm050,
        },
#endif

#if defined(CONFIG_SENSORS_GP2AP002)
        {
             I2C_BOARD_INFO("gp2ap002",0x44),
             .platform_data = &gp2ap002_platform_data,
        }
#endif

};
#endif

#ifdef CONFIG_KONA_HEADSET_MULTI_BUTTON

#define HS_IRQ		gpio_to_irq(121)
#define HSB_IRQ		BCM_INT_ID_AUXMIC_COMP2
#define HSB_REL_IRQ	BCM_INT_ID_AUXMIC_COMP2_INV
static unsigned int hawaii_button_adc_values[3][2] = {
	/* SEND/END Min, Max */
	{0, 10},
	/* Volume Up  Min, Max */
	{11, 30},
	/* Volue Down Min, Max */
	{30, 680},
};

static unsigned int hawaii_button_adc_values_2_1[3][2] = {
	/* SEND/END Min, Max */
	{0,     110},
	/* Volume Up  Min, Max */
	{111,   250},
	/* Volue Down Min, Max */
	{251,   500},
};
static struct kona_headset_pd hawaii_headset_data = {
	/* GPIO state read is 0 on HS insert and 1 for
	 * HS remove
	 */

	.hs_default_state = 1,
	/*
	 * Because of the presence of the resistor in the MIC_IN line.
	 * The actual ground may not be 0, but a small offset is added to it.
	 * This needs to be subtracted from the measured voltage to determine
	 * the correct value. This will vary for different HW based on the
	 * resistor values used.
	 *
	 * if there is a resistor present on this line, please measure the load
	 * value and put it here, otherwise 0.
	 *
	 */
	.phone_ref_offset = 0,

	/*
	 * Inform the driver whether there is a GPIO present on the board to
	 * detect accessory insertion/removal _OR_ should the driver use the
	 * COMP1 for the same.
	 */
	.gpio_for_accessory_detection = 1,

	/*
	 * Pass the board specific button detection range
	 */
	/* .button_adc_values_low = hawaii_button_adc_values,
	*/
	.button_adc_values_low = 0,

	/*
	 * Pass the board specific button detection range
	 */
	.button_adc_values_high = hawaii_button_adc_values_2_1,

	/* AUDLDO supply id for changing regulator mode*/
	.ldo_id = "audldo_uc",

};
#endif /* CONFIG_KONA_HEADSET_MULTI_BUTTON */

#ifdef CONFIG_DMAC_PL330
static struct kona_pl330_data hawaii_pl330_pdata = {
	/* Non Secure DMAC virtual base address */
	.dmac_ns_base = KONA_DMAC_NS_VA,
	/* Secure DMAC virtual base address */
	.dmac_s_base = KONA_DMAC_S_VA,
	/* # of PL330 dmac channels 'configurable' */
	.num_pl330_chans = 8,
	/* irq number to use */
	.irq_base = BCM_INT_ID_DMAC0,
	/* # of PL330 Interrupt lines connected to GIC */
	.irq_line_count = 8,
};
#endif

#ifdef CONFIG_BCM_BT_LPM
#define GPIO_BT_WAKE	32
#define GPIO_HOST_WAKE	72

static struct bcm_bt_lpm_platform_data brcm_bt_lpm_data = {
	.bt_wake_gpio = GPIO_BT_WAKE,
	.host_wake_gpio = GPIO_HOST_WAKE,
};

static struct platform_device board_bcmbt_lpm_device = {
	.name = "bcmbt-lpm",
	.id = -1,
	.dev = {
		.platform_data = &brcm_bt_lpm_data,
		},
};
#endif

/*
 * SPI board info for the slaves
 */
static struct spi_board_info spi_slave_board_info[] __initdata = {
#ifdef CONFIG_SPI_SPIDEV
	{
	 .modalias = "spidev",	/* use spidev generic driver */
	 .max_speed_hz = 13000000,	/* use max speed */
	 .bus_num = 0,		/* framework bus number */
	 .chip_select = 0,	/* for each slave */
	 .platform_data = NULL,	/* no spi_driver specific */
	 .irq = 0,		/* IRQ for this device */
	 .mode = SPI_LOOP,	/* SPI mode 0 */
	 },
#endif
};

#if defined(CONFIG_HAPTIC_SAMSUNG_PWM)
void haptic_gpio_setup(void)
{
	/* Board specific configuration like pin mux & GPIO */
}

static struct haptic_platform_data haptic_control_data = {
	/* Haptic device name: can be device-specific name like ISA1000 */
	.name = "pwm_vibra",
	/* PWM interface name to request */
	.pwm_id = 2,
	/* Invalid gpio for now, pass valid gpio number if connected */
	.gpio = ARCH_NR_GPIOS,
	.setup_pin = haptic_gpio_setup,
};

struct platform_device haptic_pwm_device = {
	.name = "samsung_pwm_haptic",
	.id = -1,
	.dev = {.platform_data = &haptic_control_data,}
};

#endif /* CONFIG_HAPTIC_SAMSUNG_PWM */



#ifdef CONFIG_OF
static struct sdio_platform_cfg hawaii_sdio0_param = {
	.configure_sdio_pullup = configure_sdio_pullup,
};


static struct sdio_platform_cfg hawaii_sdio_param = {

#ifdef CONFIG_BRCM_UNIFIED_DHD_SUPPORT
	.register_status_notify = hawaii_wifi_status_register,
#endif

};


static const struct of_dev_auxdata hawaii_auxdata_lookup[] __initconst = {

	OF_DEV_AUXDATA("bcm,pwm-backlight", 0x0,
		"pwm-backlight.0", NULL),

	OF_DEV_AUXDATA("bcm,sdhci", 0x3F190000,
		"sdhci.1", NULL),

	OF_DEV_AUXDATA("bcm,sdhci", 0x3F1A0000,
		"sdhci.2", &hawaii_sdio_param),

	OF_DEV_AUXDATA("bcm,sdhci", 0x3F180000,
		"sdhci.0", &hawaii_sdio0_param),

#ifdef CONFIG_SOC_CAMERA_OV5640
	OF_DEV_AUXDATA("bcm,soc-camera", 0x3c,
		"soc-back-camera", &iclink_main),
#endif
#if 0
#ifdef CONFIG_SOC_CAMERA_OV5648
	OF_DEV_AUXDATA("bcm,soc-camera", 0x36,
		"soc-back-camera", &iclink_main),
#endif
#endif
#ifdef CONFIG_SOC_CAMERA_OV7695
	OF_DEV_AUXDATA("bcm,soc-camera", 0x21,
		"soc-front-camera", &iclink_front),
#endif
#ifdef CONFIG_SOC_CAMERA_OV8825
	OF_DEV_AUXDATA("bcm,soc-camera", 0x36,
		"soc-back-camera", &iclink_ov8825),
#endif
#ifdef CONFIG_SOC_CAMERA_GC2035
	OF_DEV_AUXDATA("bcm,soc-camera", 0x3c,
		"soc-front-camera", &iclink_front),
#endif

	{},
};
#endif

#ifdef CONFIG_BACKLIGHT_KTD259B

static struct platform_ktd259b_backlight_data bcm_ktd259b_backlight_data = {
	.max_brightness = 255,
	.dft_brightness = 200,
	.ctrl_pin = 24,
};

struct platform_device hawaii_backlight_device = {
	.name           = "panel",
	.id = -1,
	.dev = {
		.platform_data  =  &bcm_ktd259b_backlight_data,
	},
};

#endif /* CONFIG_BACKLIGHT_KTD2801 */

#if defined(CONFIG_TOUCHKEY_BACKLIGHT)
static struct platform_device touchkeyled_device = {
	.name = "touchkey-led",
	.id = -1,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_IST30XX)	\
	|| defined(CONFIG_TOUCHSCREEN_BT432_LOGAN)
#define TSP_INT_GPIO_PIN	(73)
static struct i2c_board_info __initdata zinitix_i2c_devices[] = {
	  {
		I2C_BOARD_INFO("sec_touch", 0x50),
		.irq = gpio_to_irq(TSP_INT_GPIO_PIN),
	  },
	   {
		I2C_BOARD_INFO("zinitix_touch", 0x20),
		.irq = gpio_to_irq(TSP_INT_GPIO_PIN),
	  },
};
#endif

#if defined(CONFIG_BCM_ALSA_SOUND)
static struct caph_platform_cfg board_caph_platform_cfg =
#ifdef HW_CFG_CAPH
	HW_CFG_CAPH;
#else
{
	.aud_ctrl_plat_cfg = {
				.ext_aud_plat_cfg = {
					.ihf_ext_amp_gpio = -1,
					.dock_aud_route_gpio = -1,
					.mic_sel_aud_route_gpio = -1,
					}
				}
};
#endif
#endif

static struct platform_device board_caph_device = {
	.name = "brcm_caph_device",
	.id = -1, /*Indicates only one device */
	.dev = {
		.platform_data = &board_caph_platform_cfg,
	},
};


#ifdef CONFIG_KONA_SECURE_MEMC
struct kona_secure_memc_pdata k_s_memc_plat_data = {
	.kona_s_memc_base = KONA_MEMC0_S_VA,
	.num_of_memc_ports = NUM_OF_MEMC_PORTS,
	.num_of_groups = NUM_OF_GROUPS,
	.num_of_regions = NUM_OF_REGIONS,
	.cp_area_start = 0x80000000,
	.cp_area_end = 0x81FFFFFF,
	.ap_area_start = 0x82000000,
	.ap_area_end = 0xBFFFFFFF,
	.ddr_start = 0x80000000,
	.ddr_end = 0xBFFFFFFF,
	.masters = {
		MASTER_A7,
		MASTER_COMMS,
		MASTER_FABRIC,
		MASTER_MM,
	},
	.default_master_map = {
		MASTER_FABRIC,
		MASTER_A7,
		MASTER_COMMS,
		MASTER_MM,
	},
	/* following is static memc configuration.
	 * be careful with the same if you need to
	 * change.
	 * set the groupmask carefully.
	 * index of static_memc_master
	 * acts as a group number. */
	.static_memc_config = {
		{"0x80000000", "0x801FFFFF",
			"3", "0x03"},
		{"0x80200000", "0x811FFFFF",
			"3", "0x01"},
		{"0x81200000", "0x81FFFFFF",
			"3", "0x03"},
		{"0x82000000", "0xBFFFFFFF",
			"3", "0xFE"},
	},
	.static_memc_masters = {
		"comms ",
		"a7 ",
	},
	/* following enables user to
	 * enable static configuration listed above.
	 */
	.static_config = 1,
};
#endif


#ifdef CONFIG_RT8973

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock rt8973_jig_wakelock;
#endif
#ifdef CONFIG_KONA_PI_MGR
static struct pi_mgr_qos_node qos_node;
#endif

enum cable_type_t {
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_NONE
};
static void rt8973_wakelock_init(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&rt8973_jig_wakelock, WAKE_LOCK_SUSPEND,
		"rt8973_jig_wakelock");
#endif

#ifdef CONFIG_KONA_PI_MGR
	pi_mgr_qos_add_request(&qos_node, "rt8973_jig_qos",
		PI_MGR_PI_ID_ARM_SUB_SYSTEM, PI_MGR_QOS_DEFAULT_VALUE);
#endif
}

extern int bcmpmu_accy_chrgr_type_notify(int chrgr_type);

void send_chrgr_insert_event(enum bcmpmu_event_t event, void *para)
{
	bcmpmu_accy_chrgr_type_notify(*(u32 *)para);
}

static enum cable_type_t set_cable_status;

static void usb_attach(uint8_t attached)
{
	enum bcmpmu_chrgr_type_t chrgr_type;
	enum bcmpmu_usb_type_t usb_type;

	printk(attached ? "USB attached\n" : "USB detached\n");

#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif
	pr_info("ftsu6111_usb_cb attached %d\n", attached);

	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;

	switch (set_cable_status) {
	case CABLE_TYPE_USB:
		usb_type = PMU_USB_TYPE_SDP;
		chrgr_type = PMU_CHRGR_TYPE_SDP;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_USB_CDP;
#endif
		pr_info("%s USB attached\n", __func__);
		/* send_usb_insert_event(BCMPMU_USB_EVENT_USB_DETECTION,
			&usb_type); */
		break;
	case CABLE_TYPE_NONE:
		usb_type = PMU_USB_TYPE_NONE;
		chrgr_type = PMU_CHRGR_TYPE_NONE;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif
		pr_info("%s USB removed\n", __func__);
		/* set_usb_connection_status(&usb_type);  // only set status */
		break;
	}
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION, &chrgr_type);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_CHARGER, spa_data);
#endif
	pr_info("tsu6111_usb_cb attached %d\n", attached);
}

//extern int musb_info_handler(struct notifier_block *nb, unsigned long event, void *para);

static void uart_attach(uint8_t attached)
{
	printk(attached ? "UART attached\n" : "UART detached\n");

	pr_info("%s attached %d\n", __func__, attached);
	if (attached) {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if (!wake_lock_active(&rt8973_jig_wakelock))
			wake_lock(&rt8973_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
		pi_mgr_qos_request_update(&qos_node, 0);
#endif
#endif
		//musb_info_handler(NULL, 0, 1);
	} else {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if (wake_lock_active(&rt8973_jig_wakelock))
			wake_unlock(&rt8973_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
		pi_mgr_qos_request_update(&qos_node,
			PI_MGR_QOS_DEFAULT_VALUE);
#endif
#endif
		//musb_info_handler(NULL, 0, 0);
	}
}

#if defined(CONFIG_TOUCHSCREEN_IST30XX)
extern void ist30xx_set_ta_mode(bool charging);
#endif

static void charger_attach(uint8_t attached)
{
	enum bcmpmu_chrgr_type_t chrgr_type;
	enum cable_type_t set_cable_status;

	printk(attached ? "Charger attached\n" : "Charger detached\n");

#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif

	pr_info("tsu6111_charger_cb attached %d\n", attached);

	set_cable_status = attached ? CABLE_TYPE_AC : CABLE_TYPE_NONE;
	switch (set_cable_status) {
	case CABLE_TYPE_AC:
		chrgr_type = PMU_CHRGR_TYPE_DCP;
		pr_info("%s TA attached\n", __func__);
		#if defined(CONFIG_TOUCHSCREEN_IST30XX)
			ist30xx_set_ta_mode(1);
		#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_USB_DCP;
#endif
		break;
	case CABLE_TYPE_NONE:
		chrgr_type = PMU_CHRGR_TYPE_NONE;
		pr_info("%s TA removed\n", __func__);
		#if defined(CONFIG_TOUCHSCREEN_IST30XX)
			ist30xx_set_ta_mode(0);
		#endif
		break;
	default:
		break;
	}

	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION,
		&chrgr_type);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_CHARGER, spa_data);
#endif
	pr_info("tsu6111_charger_cb attached %d\n", attached);
}

static void jig_attach(uint8_t attached, uint8_t factory_mode)
{
	switch (factory_mode) {
	case RTMUSC_FM_BOOT_OFF_UART:
		printk(KERN_INFO "JIG BOOT OFF UART\n");
		break;
	case RTMUSC_FM_BOOT_OFF_USB:
		printk(KERN_INFO "JIG BOOT OFF USB\n");
		break;
	case RTMUSC_FM_BOOT_ON_UART:
		printk(KERN_INFO "JIG BOOT ON UART\n");
		break;
	case RTMUSC_FM_BOOT_ON_USB:
		printk(KERN_INFO "JIG BOOT ON USB\n");
		break;
	default:
		break;
	}
	pr_info("tsu6111_jig_cb attached %d\n", attached);
	if (attached) {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if (!wake_lock_active(&rt8973_jig_wakelock))
			wake_lock(&rt8973_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node, 0);
#endif
#endif
	} else {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if (wake_lock_active(&rt8973_jig_wakelock))
			wake_unlock(&rt8973_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
		pi_mgr_qos_request_update(&qos_node,
			PI_MGR_QOS_DEFAULT_VALUE);
#endif
#endif
	}
	printk(attached ? "Jig attached\n" : "Jig detached\n");
}

static void over_temperature(uint8_t detected)
{
	printk(KERN_INFO "over temperature detected = %d!\n", detected);
}

static void over_voltage(uint8_t detected)
{
	printk(KERN_INFO "over voltage = %d\n", (int32_t)detected);
	printk(KERN_INFO "OVP triggered by musb - %d\n", detected);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_OVP, detected);
#endif
}
static void set_usb_power(uint8_t on)
{
	printk(on ? "on resume() : Set USB on\n" :
		"on suspend() : Set USB off\n");
}

void uas_jig_force_sleep(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	if (wake_lock_active(&rt8973_jig_wakelock)) {
		wake_unlock(&rt8973_jig_wakelock);
		pr_info("Force unlock jig_uart_wl\n");
	}
#else
	pr_info("Warning : %s - Empty function!!!\n", __func__);
#endif
#ifdef CONFIG_KONA_PI_MGR
	pi_mgr_qos_request_update(&qos_node, PI_MGR_QOS_DEFAULT_VALUE);
#endif
	return;
}

static struct rtmus_platform_data __initdata rtmus_pdata = {
	.usb_callback = &usb_attach,
	.uart_callback = &uart_attach,
	.charger_callback = &charger_attach,
	.jig_callback = &jig_attach,
	.over_temperature_callback = &over_temperature,
	.charging_complete_callback = NULL,
	.over_voltage_callback = &over_voltage,
	.usb_power = &set_usb_power,
};


/* For you device setting */
static struct i2c_board_info __initdata micro_usb_i2c_devices_info[]  = {
/* Add for Ricktek RT8969 */
#ifdef CONFIG_RT8973
	{I2C_BOARD_INFO("rt8973", 0x28>>1),
	.irq = -1,
	.platform_data = &rtmus_pdata,},
#endif
};

static struct i2c_gpio_platform_data mUSB_i2c_gpio_data = {
	.sda_pin		= GPIO_USB_I2C_SDA,
	.scl_pin		= GPIO_USB_I2C_SCL,
	.udelay			= 2,
};

static struct platform_device mUSB_i2c_gpio_device = {
	.name                   = "i2c-gpio",
	.id                     = RT_I2C_BUS_ID,
	.dev                    = {
		.platform_data  = &mUSB_i2c_gpio_data,
	},
};

static struct platform_device *mUSB_i2c_devices[] __initdata = {
	&mUSB_i2c_gpio_device,
};
#endif



#ifdef CONFIG_USB_SWITCH_TSU6111
enum cable_type_t {
		CABLE_TYPE_USB,
		CABLE_TYPE_AC,
		CABLE_TYPE_NONE
};


#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock tsu6111_jig_wakelock;
#endif
#ifdef CONFIG_KONA_PI_MGR
static struct pi_mgr_qos_node qos_node;
#endif

static void tsu6111_wakelock_init(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&tsu6111_jig_wakelock, WAKE_LOCK_SUSPEND,
		"tsu6111_jig_wakelock");
#endif

#ifdef CONFIG_KONA_PI_MGR
	pi_mgr_qos_add_request(&qos_node, "tsu6111_jig_qos",
		PI_MGR_PI_ID_ARM_SUB_SYSTEM, PI_MGR_QOS_DEFAULT_VALUE);
#endif
}
static enum cable_type_t set_cable_status;
static void tsu6111_usb_cb(bool attached)
{
	enum bcmpmu_chrgr_type_t chrgr_type;
	enum bcmpmu_usb_type_t usb_type;

	#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data=POWER_SUPPLY_TYPE_BATTERY;
#endif
	pr_info("ftsu6111_usb_cb attached %d\n", attached);

	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;

	switch (set_cable_status) {
	case CABLE_TYPE_USB:
			usb_type = PMU_USB_TYPE_SDP;
			chrgr_type = PMU_CHRGR_TYPE_SDP;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_USB_CDP;
#endif
			pr_info("%s USB attached\n",__func__);
			//send_usb_insert_event(BCMPMU_USB_EVENT_USB_DETECTION, &usb_type);
			break;
		case CABLE_TYPE_NONE:
			usb_type = PMU_USB_TYPE_NONE;
			chrgr_type = PMU_CHRGR_TYPE_NONE;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
			spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif
			pr_info("%s USB removed\n",__func__);
			//set_usb_connection_status(&usb_type); // for unplug, we only set status, but not send event
			break;

	}
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION,&chrgr_type);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_CHARGER, spa_data);
#endif
	pr_info("tsu6111_usb_cb attached %d\n", attached);
}

#if defined (CONFIG_TOUCHSCREEN_IST30XX)
extern void ist30xx_set_ta_mode(bool charging);
#endif

static void tsu6111_charger_cb(bool attached)
{
	enum bcmpmu_chrgr_type_t chrgr_type;
	enum cable_type_t set_cable_status;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data=POWER_SUPPLY_TYPE_BATTERY;
#endif

	pr_info("tsu6111_charger_cb attached %d\n", attached);

	set_cable_status = attached ? CABLE_TYPE_AC : CABLE_TYPE_NONE;
	switch (set_cable_status) {
	case CABLE_TYPE_AC:
			chrgr_type = PMU_CHRGR_TYPE_DCP;
			pr_info("%s TA attached\n", __func__);
			#if defined (CONFIG_TOUCHSCREEN_IST30XX)
				ist30xx_set_ta_mode(1);
			#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_USB_DCP;
#endif
			break;
		case CABLE_TYPE_NONE:
			chrgr_type = PMU_CHRGR_TYPE_NONE;
			pr_info("%s TA removed\n", __func__);
			#if defined (CONFIG_TOUCHSCREEN_IST30XX)
				ist30xx_set_ta_mode(0);
			#endif
			break;
		default:
			break;
	}
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION,
			&chrgr_type);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_CHARGER, spa_data);
#endif
	pr_info("tsu6111_charger_cb attached %d\n", attached);
}

static void tsu6111_jig_cb(bool attached)
{
	pr_info("tsu6111_jig_cb attached %d\n", attached);

	if (attached) {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if ( !wake_lock_active( &tsu6111_jig_wakelock ) )
			wake_lock( &tsu6111_jig_wakelock );
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node, 0);
#endif
#endif
	} else {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if ( wake_lock_active( &tsu6111_jig_wakelock ) )
			wake_unlock( &tsu6111_jig_wakelock );
#endif
#ifdef CONFIG_KONA_PI_MGR
		pi_mgr_qos_request_update(&qos_node,
			PI_MGR_QOS_DEFAULT_VALUE);
#endif
#endif
	}
}
//extern int musb_info_handler(struct notifier_block *nb, unsigned long event, void *para);
static void tsu6111_uart_cb(bool attached)
{
	pr_info("%s attached %d\n",__func__, attached);
		if (attached) {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
			if ( !wake_lock_active( &tsu6111_jig_wakelock ) )
			wake_lock( &tsu6111_jig_wakelock );
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node, 0);
#endif
#endif
			//musb_info_handler(NULL, 0, 1);
	} else {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
			if ( wake_lock_active( &tsu6111_jig_wakelock ) )
			wake_unlock( &tsu6111_jig_wakelock );
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node,
				PI_MGR_QOS_DEFAULT_VALUE);
#endif
#endif
			//musb_info_handler(NULL, 0, 0);
		}

}

void uas_jig_force_sleep(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	if(wake_lock_active(&tsu6111_jig_wakelock))
	{
		wake_unlock(&tsu6111_jig_wakelock);
		pr_info("Force unlock jig_uart_wl\n");
	}
#else
	pr_info("Warning : %s - Empty function!!!\n", __func__);
#endif
#ifdef CONFIG_KONA_PI_MGR
	pi_mgr_qos_request_update(&qos_node, PI_MGR_QOS_DEFAULT_VALUE);
#endif
	return;
}

static struct tsu6111_platform_data tsu6111_pdata = {
        .usb_cb = tsu6111_usb_cb,
        .charger_cb = tsu6111_charger_cb,
        .jig_cb = tsu6111_jig_cb,
        .uart_cb = tsu6111_uart_cb,
};

static struct i2c_board_info  __initdata micro_usb_i2c_devices_info[]  = {
        {
				I2C_BOARD_INFO("tsu6111", 0x28 >> 1),
                .platform_data = &tsu6111_pdata,
                .irq = gpio_to_irq(GPIO_USB_INT),
        },
};

static struct i2c_gpio_platform_data mUSB_i2c_gpio_data={
        .sda_pin        = GPIO_USB_I2C_SDA,
                .scl_pin= GPIO_USB_I2C_SCL,
                .udelay                 = 2,
        };

static struct platform_device mUSB_i2c_gpio_device = {
        .name                   = "i2c-gpio",
        .id                     = TSU6111_I2C_BUS_ID,
        .dev                    ={
                .platform_data  = &mUSB_i2c_gpio_data,
        },
};

static struct platform_device *mUSB_i2c_devices[] __initdata = {
        &mUSB_i2c_gpio_device,
};

#endif



static struct platform_device *hawaii_devices[] __initdata = {


#ifdef CONFIG_KONA_HEADSET_MULTI_BUTTON
	&hawaii_headset_device,
#endif

#ifdef CONFIG_DMAC_PL330
	&hawaii_pl330_dmac_device,
#endif

#ifdef CONFIG_HAPTIC_SAMSUNG_PWM
	&haptic_pwm_device,
#endif

#if	defined(CONFIG_BACKLIGHT_PWM) || defined(CONFIG_BACKLIGHT_KTD259B)
	&hawaii_backlight_device,
#endif
#if defined(CONFIG_BCM_BT_LPM)
	&board_bcmbt_lpm_device,
#endif


#ifdef CONFIG_VIDEO_KONA
	&hawaii_unicam_device,
#endif

#if defined(CONFIG_BCM_ALSA_SOUND)
	&board_caph_device,
#endif

#if defined(CONFIG_TOUCHKEY_BACKLIGHT)
		&touchkeyled_device
#endif

};

#ifdef CONFIG_IHF_EXT_PA_TPA2026D2
static struct tpa2026d2_platform_data tpa2026d2_i2c_platform_data = {
	.i2c_bus_id = 1,
	.shutdown_gpio = 91
};

static struct i2c_board_info tpa2026d2_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("tpa2026d2", (TPA2026D2_I2C_ADDR >> 1)),
		.platform_data = &tpa2026d2_i2c_platform_data
	},
};
#endif


static void __init hawaii_add_i2c_devices(void)
{

#ifdef CONFIG_VIDEO_ADP1653
	i2c_register_board_info(0, adp1653_flash, ARRAY_SIZE(adp1653_flash));
#endif


#ifdef CONFIG_TOUCHSCREEN_TANGO
	i2c_register_board_info(3, tango_info, ARRAY_SIZE(tango_info));
#endif
#if defined(CONFIG_TOUCHSCREEN_IST30XX) \
	|| defined(CONFIG_TOUCHSCREEN_BT432_LOGAN)
	i2c_register_board_info(3, zinitix_i2c_devices,
		ARRAY_SIZE(zinitix_i2c_devices));
#endif

#ifdef CONFIG_TOUCHSCREEN_FT5306
	i2c_register_board_info(3, ft5306_info, ARRAY_SIZE(ft5306_info));
#endif


#ifdef CONFIG_SENSORS_BMA222
	i2c_register_board_info(2, bma222_accl_info,
		ARRAY_SIZE(bma222_accl_info));
#endif

#if defined(CONFIG_TOUCHSCREEN_BCM915500)
	|| defined(CONFIG_TOUCHSCREEN_BCM915500_MODULE)
	i2c_register_board_info(3, bcm915500_i2c_boardinfo,
				ARRAY_SIZE(bcm915500_i2c_boardinfo));
#endif

#ifdef CONFIG_USB_SWITCH_TSU6111
        pr_info("tsu6111\n");
#if defined( CONFIG_HAS_WAKELOCK ) || defined( CONFIG_KONA_PI_MGR )
		tsu6111_wakelock_init();
#endif
        i2c_register_board_info(TSU6111_I2C_BUS_ID, micro_usb_i2c_devices_info,
		ARRAY_SIZE(micro_usb_i2c_devices_info));
#endif

#ifdef CONFIG_RT8973
	pr_info("rt8973: micro_usb_i2c_devices_info\n");
	rt8973_wakelock_init();
	i2c_register_board_info(RT_I2C_BUS_ID, micro_usb_i2c_devices_info,
		ARRAY_SIZE(micro_usb_i2c_devices_info));
#endif

#if defined(CONFIG_USB_SWITCH_TSU6111) || defined(CONFIG_RT8973)
	pr_info("mUSB_i2c_devices\n");
	platform_add_devices(mUSB_i2c_devices, ARRAY_SIZE(mUSB_i2c_devices));
#endif

#if defined(CONFIG_BCMI2CNFC)
	i2c_register_board_info(1, bcmi2cnfc, ARRAY_SIZE(bcmi2cnfc));
#endif

#if defined(CONFIG_MPU_SENSORS_MPU6050B1)
	|| defined(CONFIG_MPU_SENSORS_MPU6050B1_MODULE)
#if defined(MPU6050_IRQ_GPIO)
	inv_mpu_i2c0_boardinfo[0].irq = gpio_to_irq(MPU6050_IRQ_GPIO);
#endif
	i2c_register_board_info(MPU6050_I2C_BUS_ID,
		inv_mpu_i2c0_boardinfo, ARRAY_SIZE(inv_mpu_i2c0_boardinfo));
#endif

#if defined(CONFIG_SENSORS_BMC150) || defined(CONFIG_SENSORS_GP2AP002)
	i2c_register_board_info(2, bsc3_i2c_boardinfo, ARRAY_SIZE(bsc3_i2c_boardinfo));
#endif

#if  defined(CONFIG_BMP18X) || defined(CONFIG_BMP18X_I2C)
	|| defined(CONFIG_BMP18X_I2C_MODULE)
	i2c_register_board_info(
#ifdef BMP18X_I2C_BUS_ID
			BMP18X_I2C_BUS_ID,
#else
			-1,
#endif
			i2c_bmp18x_info, ARRAY_SIZE(i2c_bmp18x_info));
#endif


#if defined(CONFIG_AL3006) || defined(CONFIG_AL3006_MODULE)
#ifdef AL3006_IRQ
	i2c_al3006_info[0].irq = gpio_to_irq(AL3006_IRQ_GPIO);
#else
	i2c_al3006_info[0].irq = -1;
#endif
	i2c_register_board_info(
#ifdef AL3006_I2C_BUS_ID
		AL3006_I2C_BUS_ID,
#else
		-1,
#endif
		i2c_al3006_info, ARRAY_SIZE(i2c_al3006_info));
#endif /* CONFIG_AL3006 */

#if  defined(CONFIG_MPU_SENSORS_AMI306) || defined(CONFIG_MPU_SENSORS_AMI306)
	i2c_register_board_info(
#ifdef AMI306_I2C_BUS_ID
			AMI306_I2C_BUS_ID,
#else
			-1,
#endif
			i2c_ami306_info, ARRAY_SIZE(i2c_ami306_info));
#endif


}


#define HS_IRQ		gpio_to_irq(121)

static void hawaii_add_pdata(void)
{
	hawaii_ssp0_device.dev.platform_data = &hawaii_ssp0_info;
	hawaii_ssp1_device.dev.platform_data = &hawaii_ssp1_info;
#ifdef CONFIG_BCM_STM
	hawaii_stm_device.dev.platform_data = &hawaii_stm_pdata;
#endif
	hawaii_headset_device.dev.platform_data = &hawaii_headset_data;
	/* The resource in position 2 (starting from 0) is used to fill
	 * the GPIO number. The driver file assumes this. So put the
	 * board specific GPIO number here
	 */
	hawaii_headset_device.resource[2].start = HS_IRQ;
	hawaii_headset_device.resource[2].end   = HS_IRQ;

	hawaii_pl330_dmac_device.dev.platform_data = &hawaii_pl330_pdata;
#ifdef CONFIG_USB_DWC_OTG
	hawaii_hsotgctrl_platform_device.dev.platform_data =
		&hsotgctrl_plat_data;
	hawaii_usb_phy_platform_device.dev.platform_data =
		&hsotgctrl_plat_data;
#endif
}

void __init hawaii_add_common_devices(void)
{
	platform_add_devices(hawaii_common_plat_devices,
			     ARRAY_SIZE(hawaii_common_plat_devices));
}

static void __init hawaii_add_devices(void)
{
	hawaii_add_pdata();

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
	platform_device_register(&board_gpio_keys_device);
#endif
	platform_add_devices(hawaii_devices, ARRAY_SIZE(hawaii_devices));

	hawaii_add_i2c_devices();

	spi_register_board_info(spi_slave_board_info,
				ARRAY_SIZE(spi_slave_board_info));

}


static struct of_device_id hawaii_dt_match_table[] __initdata = {
	{ .compatible = "simple-bus", },
	{}
};



static void __init java_init(void)
{
	hawaii_add_devices();


	hawaii_add_common_devices();
#ifdef CONFIG_OF
	/* Populate platform_devices from device tree data */
	of_platform_populate(NULL, hawaii_dt_match_table,
			hawaii_auxdata_lookup, &platform_bus);
#endif
	return;
}

static int __init hawaii_add_lateinit_devices(void)
{
#ifdef CONFIG_BRCM_UNIFIED_DHD_SUPPORT
	hawaii_wlan_init();
#endif
	return 0;
}

late_initcall(hawaii_add_lateinit_devices);

#ifdef CONFIG_OF
static const char * const java_dt_compat[] = { "bcm,java", NULL, };
DT_MACHINE_START(HAWAII, CONFIG_BRD_NAME)
	.dt_compat = java_dt_compat,
#else
MACHINE_START(HAWAII, CONFIG_BRD_NAME)
	.atag_offset = 0x100,
#endif
	.smp = smp_ops(kona_smp_ops),
	.map_io = hawaii_map_io,
	.init_irq = kona_init_irq,
	.init_time = java_timer_init,
	.init_machine = java_init,
	.reserve = hawaii_reserve,
	.restart = hawaii_restart,
MACHINE_END
