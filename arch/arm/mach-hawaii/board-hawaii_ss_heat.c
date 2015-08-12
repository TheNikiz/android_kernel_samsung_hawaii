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




#include <mach/dev-hawaii_heat_bt.h>
#include <mach/dev-hawaii_heat_camera.h>
#include <mach/dev-hawaii_heat_nfc.h>
#include <mach/dev-hawaii_heat_tsp.h>
#include <mach/dev-hawaii_sensor.h>


extern struct platform_device hawaii_camera;
extern struct platform_device hawaii_camera_sub;

extern void hawaii_bt_init(void);
extern void hawaii_keyboard_init(void);
extern void hawaii_touchscreen_init(void);
extern void hawaii_muic_init(void);
extern void hawaii_nfc_init(void);
extern void hawaii_sensor_init(void);
extern void hawaii_cam_init(void);



/* SD */
#define SD_CARDDET_GPIO_PIN	91



/* NFC */
//#define NFC_INT	90
//#define NFC_WAKE 25
//#define NFC_ENABLE 100

#define KONA_UART0_PA   UARTB_BASE_ADDR
#define KONA_UART1_PA   UARTB2_BASE_ADDR
#define KONA_UART2_PA   UARTB3_BASE_ADDR

#define HAWAII_8250PORT(name, clk, freq, uart_name, power_save_en)\
{								\
	.membase    = (void __iomem *)(KONA_##name##_VA),	\
	.mapbase    = (resource_size_t)(KONA_##name##_PA),	\
	.irq        = BCM_INT_ID_##name,			\
	.uartclk    = freq,					\
	.regshift   = 2,					\
	.iotype     = UPIO_MEM32,				\
	.type       = PORT_16550A,				\
	.flags      = UPF_BOOT_AUTOCONF | UPF_BUG_THRE |	\
			UPF_FIXED_TYPE | UPF_SKIP_TEST,		\
	.private_data = power_save_en,				\
	.clk_name = clk,					\
	.port_name = uart_name,					\
}

/*This flag is added for saving power for UART GPS. If you want to save power
 * pass the address of this flag as a parameter to power_save_en*/
static bool power_save_enable = 1;

int reset_pwm_padcntrl(void)
{
	struct pin_config new_pin_config;
	int ret;
	new_pin_config.name = PN_GPIO24;
	new_pin_config.func = PF_GPIO24;
	ret = pinmux_set_pin_config(&new_pin_config);
	return ret;
}

#ifdef CONFIG_ION_BCM_NO_DT
struct ion_platform_data ion_system_data = {
	.nr = 1,
#ifdef CONFIG_IOMMU_API
	.pdev_iommu = &iommu_mm_device,
#endif
#ifdef CONFIG_BCM_IOVMM
	.pdev_iovmm = &iovmm_mm_256mb_device,
#endif
	.heaps = {
		[0] = {
			.id    = 0,
			.type  = ION_HEAP_TYPE_SYSTEM,
			.name  = "ion-system",
			.base  = 0,
			.limit = 0,
			.size  = 0,
		},
	},
};

struct ion_platform_data ion_system_extra_data = {
	.nr = 1,
#ifdef CONFIG_IOMMU_API
	.pdev_iommu = &iommu_mm_device,
#endif
#ifdef CONFIG_BCM_IOVMM
	.pdev_iovmm = &iovmm_mm_device,
#endif
	.heaps = {
		[0] = {
			.id    = 1,
			.type  = ION_HEAP_TYPE_SYSTEM,
			.name  = "ion-system-extra",
			.base  = 0,
			.limit = 0,
			.size  = 0,
		},
	},
};

struct ion_platform_data ion_carveout_data = {
	.nr = 2,
#ifdef CONFIG_IOMMU_API
	.pdev_iommu = &iommu_mm_device,
#endif
#ifdef CONFIG_BCM_IOVMM
	.pdev_iovmm = &iovmm_mm_256mb_device,
#endif
	.heaps = {
		[0] = {
			.id    = 3,
			.type  = ION_HEAP_TYPE_CARVEOUT,
			.name  = "ion-carveout",
			.base  = 0x90000000,
			.limit = 0xa0000000,
			.size  = (64 * SZ_1M),
#ifdef CONFIG_ION_OOM_KILLER
			.lmk_enable = 0,
			.lmk_min_score_adj = 411,
			.lmk_min_free = 32,
#endif
		},
		[1] = {
			.id    = 1,
			.type  = ION_HEAP_TYPE_CARVEOUT,
			.name  = "ion-carveout-extra",
			.base  = 0,
			.limit = 0xa0000000,
			.size  = (0 * SZ_1M),
#ifdef CONFIG_ION_OOM_KILLER
			.lmk_enable = 0,
			.lmk_min_score_adj = 411,
			.lmk_min_free = 32,
#endif
		},
	},
};

#ifdef CONFIG_CMA
struct ion_platform_data ion_cma_data = {
	.nr = 2,
#ifdef CONFIG_IOMMU_API
	.pdev_iommu = &iommu_mm_device,
#endif
#ifdef CONFIG_BCM_IOVMM
	.pdev_iovmm = &iovmm_mm_256mb_device,
#endif
	.heaps = {
		[0] = {
			.id = 2,
			.type  = ION_HEAP_TYPE_DMA,
			.name  = "ion-cma",
			.base  = 0x90000000,
			.limit = 0xa0000000,
			.size  = (0 * SZ_1M),
#ifdef CONFIG_ION_OOM_KILLER
			.lmk_enable = 1,
			.lmk_min_score_adj = 411,
			.lmk_min_free = 32,
#endif
		},
		[1] = {
			.id = 0,
			.type  = ION_HEAP_TYPE_DMA,
			.name  = "ion-cma-extra",
			.base  = 0x00000000,
			.limit = 0xa0000000,
			.size  = (0 * SZ_1M),
#ifdef CONFIG_ION_OOM_KILLER
			.lmk_enable = 1,
			.lmk_min_score_adj = 411,
			.lmk_min_free = 32,
#endif
		},
	},
};
#endif /* CONFIG_CMA */
#endif /* CONFIG_ION_BCM_NO_DT */




static struct plat_serial8250_port hawaii_uart_platform_data[] = {
	HAWAII_8250PORT(UART0, UARTB_PERI_CLK_NAME_STR, 48000000,
					"bluetooth", NULL),
	HAWAII_8250PORT(UART1, UARTB2_PERI_CLK_NAME_STR, 26000000,
				"gps", &power_save_enable),
	HAWAII_8250PORT(UART2, UARTB3_PERI_CLK_NAME_STR, 26000000,
				"console", NULL),
	{
		.flags = 0,
	},
};

static struct bsc_adap_cfg bsc_i2c_cfg[] = {
	{
		.speed = BSC_BUS_SPEED_400K,
		.dynamic_speed = 1,
		.bsc_clk = "bsc1_clk",
		.bsc_apb_clk = "bsc1_apb_clk",
		.retries = 1,
		.is_pmu_i2c = false,
		.fs_ref = BSC_BUS_REF_13MHZ,
		.hs_ref = BSC_BUS_REF_104MHZ,
	},

	{
		.speed = BSC_BUS_SPEED_400K,
		.dynamic_speed = 1,
		.bsc_clk = "bsc2_clk",
		.bsc_apb_clk = "bsc2_apb_clk",
		.retries = 3,
		.is_pmu_i2c = false,
		.fs_ref = BSC_BUS_REF_13MHZ,
		.hs_ref = BSC_BUS_REF_104MHZ,
	},

	{
		.speed = BSC_BUS_SPEED_400K,
		.dynamic_speed = 1,
		.bsc_clk = "bsc3_clk",
		.bsc_apb_clk = "bsc3_apb_clk",
		.retries = 1,
		.is_pmu_i2c = false,
		.fs_ref = BSC_BUS_REF_13MHZ,
		.hs_ref = BSC_BUS_REF_104MHZ,
	},

	{
		.speed = BSC_BUS_SPEED_400K,
		.dynamic_speed = 1,
		.bsc_clk = "bsc4_clk",
		.bsc_apb_clk = "bsc4_apb_clk",
		.retries = 1,
		.is_pmu_i2c = false,
		.fs_ref = BSC_BUS_REF_13MHZ,
		.hs_ref = BSC_BUS_REF_104MHZ,
	},

	{
#if defined(CONFIG_KONA_PMU_BSC_HS_MODE)
		.speed = BSC_BUS_SPEED_HS,
		/* No dynamic speed in HS mode */
		.dynamic_speed = 0,
		/*
		 * PMU can NAK certain I2C read commands, while write
		 * is in progress; and it takes a while to synchronise
		 * writes between HS clock domain(3.25MHz) and
		 * internal clock domains (32k). In such cases, we retry
		 * PMU reads until the writes are through. PMU need more
		 * retry counts in HS mode to handle this.
		 */
		.retries = 5,
#elif defined(CONFIG_KONA_PMU_BSC_HS_1MHZ)
		.speed = BSC_BUS_SPEED_HS_1MHZ,
		.dynamic_speed = 0,
		.retries = 5,
#elif defined(CONFIG_KONA_PMU_BSC_HS_1625KHZ)
		.speed = BSC_BUS_SPEED_HS_1625KHZ,
		.dynamic_speed = 0,
		.retries = 5,
#else
		.speed = BSC_BUS_SPEED_50K,
		.dynamic_speed = 1,
		.retries = 3,
#endif
		.bsc_clk = "pmu_bsc_clk",
		.bsc_apb_clk = "pmu_bsc_apb",
		.is_pmu_i2c = true,
		.fs_ref = BSC_BUS_REF_13MHZ,
		.hs_ref = BSC_BUS_REF_26MHZ,
	 },
};

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
	.hsotgctrl_virtual_mem_base = (u32)KONA_USB_HSOTG_CTRL_VA,
	.chipreg_virtual_mem_base = (u32)KONA_CHIPREG_VA,
	.irq = BCM_INT_ID_HSOTG_WAKEUP,
	.usb_ahb_clk_name = USB_OTG_AHB_BUS_CLK_NAME_STR,
	.mdio_mstr_clk_name = MDIOMASTER_PERI_CLK_NAME_STR,
};
#endif


struct regulator_consumer_supply hv6_supply[] = {
	{.supply = "vdd_sdxc"},
	{.supply = "sddat_debug_bus"},
};

struct regulator_consumer_supply sd_supply[] = {
	{.supply = "sdldo_uc"},
	REGULATOR_SUPPLY("vddmmc", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "vdd_sdio"},
};

struct regulator_consumer_supply sdx_supply[] = {
	{.supply = "sdxldo_uc"},
	REGULATOR_SUPPLY("vddo", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "vdd_sdxc"},
};


#define GPIO_KEYS_SETTINGS { \
	{ KEY_HOME, 10, 1, "HOME", EV_KEY, 0, 64}, \
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

#ifdef CONFIG_KONA_HEADSET_MULTI_BUTTON
#define HS_IRQ			gpio_to_irq(121)
#define HSB_IRQ		BCM_INT_ID_AUXMIC_COMP2
#define HSB_REL_IRQ	BCM_INT_ID_AUXMIC_COMP2_INV
#define DEBOUNCE_TIME		(64000)
#define ACCESSORY_INSERTION_SETTLE_TIME 	(300)
#define ACCESSORY_REMOVE_SETTLE_TIME 		(0)
#define ADC_CHECK_OFFSET 	(3)
#define COMP1_THRESHOLD		(650)
#define COMP2_THRESHOLD		(2100)

static unsigned int hawaii_button_adc_values_high[3][2] = {
	/* SEND/END Min, Max*/
	{0,     110},
	/* Volume Up  Min, Max*/
	{111,   250},
	/* Volue Down Min, Max*/
	{251,   500},
};

static unsigned int hawaii_accessory_adc_values[3][2] = {
	/* 3 Pole Min, Max*/
	{0,     910},
	/* 4 Pole  Min, Max*/
	{911,   4096},
	/* Open cable Min, Max*/
	{1950,  5000},
};

static void headset_enable_external_ldo(void)
{
	pr_info("[HEADSET]:%s\n", __func__);
}

static void headset_disable_external_ldo(void)
{
	pr_info("[HEADSET]:%s\n", __func__); 
}

static struct kona_headset_pd hawaii_headset_data = {
	/* GPIO state read is 0 on HS insert and 1 for
	 * HS remove
	 */

	.hs_default_state = 1,
	/*
	 * Because of the presence of the resistor in the MIC_IN line.
	 * The actual ground may not be 0, but a small offset is added to it.
	 * This needs to be subtracted from the measured voltage to determine
	 * the correct value. This will vary for different HW based
	 * on the resistor values used.
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
	.button_adc_values_low = NULL,

	/*
	 * Pass the board specific button detection range
	 */
	.button_adc_values_high = hawaii_button_adc_values_high,
	.accessory_adc_values = hawaii_accessory_adc_values,
	.gpio_debounce = DEBOUNCE_TIME,
	.jack_insertion_settle_time = ACCESSORY_INSERTION_SETTLE_TIME,
	.jack_remove_settle_time = ACCESSORY_REMOVE_SETTLE_TIME,
	.adc_check_offset = ADC_CHECK_OFFSET,
	.comp1_threshold = COMP1_THRESHOLD,
	.comp2_threshold = COMP2_THRESHOLD,
	.use_external_micbias = TRUE,
	.enable_external_ldo = headset_enable_external_ldo,
	.disable_external_ldo = headset_disable_external_ldo,
	.audio_ldo_id = "audldo_uc",
};
#endif /* CONFIG_KONA_HEADSET_MULTI_BUTTON */

#ifdef CONFIG_DMAC_PL330
static struct kona_pl330_data hawaii_pl330_pdata =	{
	/* Non Secure DMAC virtual base address */
	.dmac_ns_base = (u32) KONA_DMAC_NS_VA,
	/* Secure DMAC virtual base address */
	.dmac_s_base = (u32)KONA_DMAC_S_VA,
	/* # of PL330 dmac channels 'configurable' */
	.num_pl330_chans = 8,
	/* irq number to use */
	.irq_base = BCM_INT_ID_DMAC0,
	/* # of PL330 Interrupt lines connected to GIC */
	.irq_line_count = 8,
};
#endif



#ifdef CONFIG_BYPASS_WIFI_DEVTREE
static struct board_wifi_info brcm_wifi_data = {
	.wl_reset_gpio = 3,
	.host_wake_gpio = 74,
	.board_nvram_file = "/system/vendor/firmware/fw_wifi_nvram_4330.txt",
	.module_name = "bcm-wifi",
};
static struct platform_device board_wifi_driver_device = {

	.name = "bcm_wifi",
	.id = -1,
	.dev = {
		.platform_data = &brcm_wifi_data,
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
	.name   = "samsung_pwm_haptic",
	.id     = -1,
	.dev	=	 {	.platform_data = &haptic_control_data,}
};

#endif /* CONFIG_HAPTIC_SAMSUNG_PWM */

static struct sdio_platform_cfg hawaii_sdio_param[] = {
	{
		.id = 0,
		.data_pullup = 0,
		.cd_gpio = SD_CARDDET_GPIO_PIN,
		.devtype = SDIO_DEV_TYPE_SDMMC,
		.flags = KONA_SDIO_FLAGS_DEVICE_REMOVABLE,
		.peri_clk_name = "sdio1_clk",
		.ahb_clk_name = "sdio1_ahb_clk",
		.sleep_clk_name = "sdio1_sleep_clk",
		.peri_clk_rate = 48000000,
		/*The SD card regulator*/
		.vddo_regulator_name = "vdd_sdio",
		/*The SD controller regulator*/
		.vddsdxc_regulator_name = "vdd_sdxc",
		.configure_sdio_pullup = configure_sdio_pullup,
	},
	{
		.id = 1,
		.data_pullup = 0,
		.is_8bit = 1,
		.devtype = SDIO_DEV_TYPE_EMMC,
		.flags = KONA_SDIO_FLAGS_DEVICE_NON_REMOVABLE ,
		.peri_clk_name = "sdio2_clk",
		.ahb_clk_name = "sdio2_ahb_clk",
		.sleep_clk_name = "sdio2_sleep_clk",
		.peri_clk_rate = 52000000,
		},
#ifdef CONFIG_BRCM_UNIFIED_DHD_SUPPORT
	{
		.id = 2,
		.data_pullup = 0,
		.devtype = SDIO_DEV_TYPE_WIFI,
		.flags = KONA_SDIO_FLAGS_DEVICE_REMOVABLE,
		.peri_clk_name = "sdio3_clk",
		.ahb_clk_name = "sdio3_ahb_clk",
		.sleep_clk_name = "sdio3_sleep_clk",
		.peri_clk_rate = 48000000,
		.register_status_notify = hawaii_wifi_status_register,
	},
#else
	{
		.id = 2,
		.data_pullup = 0,
		.devtype = SDIO_DEV_TYPE_WIFI,
		.wifi_gpio = {
			.reset = 3,
			.reg = -1,
			.host_wake = 74,
			.shutdown = -1,
		},
		.flags = KONA_SDIO_FLAGS_DEVICE_REMOVABLE,
		.peri_clk_name = "sdio3_clk",
		.ahb_clk_name = "sdio3_ahb_clk",
		.sleep_clk_name = "sdio3_sleep_clk",
		.peri_clk_rate = 48000000,
	},
#endif

};


#ifdef CONFIG_BACKLIGHT_PWM

static struct platform_pwm_backlight_data hawaii_backlight_data = {
/* backlight */
	.pwm_id		= 2,
	.max_brightness	= 32,   /* Android calibrates to 32 levels*/
	.dft_brightness	= 32,
	.polarity	= 1,    /* Inverted polarity */
	.pwm_period_ns	= 1000000,
};

#endif /*CONFIG_BACKLIGHT_PWM */

#ifdef CONFIG_BACKLIGHT_KTD259B

static struct platform_ktd259b_backlight_data bcm_ktd259b_backlight_data = {
	.max_brightness = 255,
	.dft_brightness = 160,
	.ctrl_pin = 24,
};

struct platform_device hawaii_backlight_device = {
	.name = "panel",
	.id = -1,
	.dev = {
		.platform_data = &bcm_ktd259b_backlight_data,
	},
};

#endif /* CONFIG_BACKLIGHT_KTD259B */

/* Remove this comment when camera data for Hawaii is updated */

#ifdef CONFIG_WD_TAPPER
static struct wd_tapper_platform_data wd_tapper_data = {
	/* Set the count to the time equivalent to the time-out in seconds
	 * required to pet the PMU watchdog to overcome the problem of reset in
	 * suspend*/
	.count = 120,
	.ch_num = 1,
	.name = "aon-timer",
};

static struct platform_device wd_tapper = {
	.name = "wd_tapper",
	.id = 0,
	.dev = {
		.platform_data = &wd_tapper_data,
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

static struct platform_device board_caph_device = {
	.name = "brcm_caph_device",
	.id = -1, /*Indicates only one device */
	.dev = {
		.platform_data = &board_caph_platform_cfg,
	},
};

#endif /* CONFIG_BCM_ALSA_SOUND */

#ifdef CONFIG_RT8973


extern int bcmpmu_accy_chrgr_type_notify(int chrgr_type);

void send_chrgr_insert_event(enum bcmpmu_event_t event, void *para)
{
	bcmpmu_accy_chrgr_type_notify(*(u32 *)para);
}

#endif

static struct platform_device *hawaii_devices[] __initdata = {
#ifdef CONFIG_KEYBOARD_BCM
	&hawaii_kp_device,
#endif
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

/*
bt init
*/

#ifdef CONFIG_BYPASS_WIFI_DEVTREE
	&board_wifi_driver_device,
#endif

#ifdef CONFIG_VIDEO_KONA
	&hawaii_unicam_device,
#endif

#ifdef CONFIG_WD_TAPPER
	&wd_tapper,
#endif

#if defined(CONFIG_BCM_ALSA_SOUND)
	&board_caph_device,
#endif

#if defined(CONFIG_TOUCHKEY_BACKLIGHT)
	&touchkeyled_device
#endif
};


#ifdef CONFIG_IOMMU_API
struct bcm_iommu_pdata iommu_mm_pdata = {
	.name        = "iommu-mm",
	.iova_begin  = 0x80000000,
	.iova_size   = 0x80000000,
	.errbuf_size = 0x1000,
	.skip_enable = 1,
};
#endif
#ifdef CONFIG_BCM_IOVMM
struct bcm_iovmm_pdata iovmm_mm_pdata = {
	.name = "iovmm-mm",
	.base = 0xc0000000,
	.size = 0x30000000,
	.order = 0,
};
struct bcm_iovmm_pdata iovmm_mm_256mb_pdata = {
	.name = "iovmm-mm-256mb",
	.base = 0xf0000000,
	.size = 0x0ff00000,
	.order = 0,
};
#endif

/* The GPIO used to indicate accessory insertion in this board */
#define HS_IRQ		gpio_to_irq(121)




#ifdef CONFIG_MOBICORE_OS
static void hawaii_mem_reserve(void)
{
	mobicore_device.dev.platform_data = &mobicore_plat_data;
	hawaii_reserve();
}
#endif


#ifdef CONFIG_FB_BRCM_KONA
/*
 * KONA FRAME BUFFER DISPLAY DRIVER PLATFORM CONFIG
 */
struct kona_fb_platform_data konafb_devices[] __initdata = {
	{
		.name = "NT35510",
		.reg_name = "cam2",
		.rst =  {
			.gpio = 22,
			.setup = 5,
			.pulse = 20,
			.hold = 10000,
			.active = false,
		},
		.vmode = false,
		.vburst = false,
		.cmnd_LP = false,
		.te_ctrl = true,
		.col_mod_i = 3,  /*DISPDRV_FB_FORMAT_xBGR8888*/
		.col_mod_o = 2, /*DISPDRV_FB_FORMAT_xRGB8888*/
		.width = 480,
		.height = 800,
		.fps = 60,
		.lanes = 2,
		.hs_bps = 500000000,
		.lp_bps = 5000000,
#ifdef CONFIG_IOMMU_API
		.pdev_iommu = &iommu_mm_device,
#endif
#ifdef CONFIG_BCM_IOVMM
		.pdev_iovmm = &iovmm_mm_256mb_device,
#endif
	},
	{
		.name = "ILI9806",
		.reg_name = "cam2",
		.rst =  {
			.gpio = 22,
			.setup = 5,
			.pulse = 20,
			.hold = 10000,
			.active = false,
		},
		.vmode = false,
		.vburst = false,
		.cmnd_LP = false,
		.te_ctrl = true,
		.col_mod_i = 3,  /*DISPDRV_FB_FORMAT_xBGR8888*/
		.col_mod_o = 2, /*DISPDRV_FB_FORMAT_xRGB8888*/
		.width = 480,
		.height = 800,
		.fps = 60,
		.lanes = 2,
		.hs_bps = 300000000,
		.lp_bps = 5000000,
#ifdef CONFIG_IOMMU_API
		.pdev_iommu = &iommu_mm_device,
#endif
#ifdef CONFIG_BCM_IOVMM
		.pdev_iovmm = &iovmm_mm_256mb_device,
#endif
	},	
};

#include "kona_fb_init.c"
#endif /* #ifdef CONFIG_FB_BRCM_KONA */

static struct platform_device nt35510_panel_device = {
	.name = "nt35510",
	.id = -1,
};

	struct platform_device *hawaii_common_plat_devices[] __initdata = {
	&hawaii_serial_device,
	&hawaii_i2c_adap_devices[0],
	&hawaii_i2c_adap_devices[1],
	&hawaii_i2c_adap_devices[2],
	&hawaii_i2c_adap_devices[3],
	&hawaii_i2c_adap_devices[4],
	&pmu_device,
	&hawaii_pwm_device,
	&hawaii_ssp0_device,

#ifdef CONFIG_SENSORS_KONA
	&thermal_device,
#endif

#ifdef CONFIG_STM_TRACE
/* &hawaii_stm_device, */
#endif

#if defined(CONFIG_HW_RANDOM_KONA)
	&rng_device,
#endif

#if defined(CONFIG_USB_DWC_OTG)
	&hawaii_usb_phy_platform_device,
	&hawaii_hsotgctrl_platform_device,
	&hawaii_otg_platform_device,
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

#ifdef CONFIG_UNICAM
	&hawaii_unicam_device,
#endif


#ifdef CONFIG_SND_BCM_SOC
	&hawaii_audio_device,
	&caph_i2s_device,
	&caph_pcm_device,
	&spdif_dit_device,

#endif

#ifdef CONFIG_KONA_MEMC
	&kona_memc_device,
#endif
#ifdef CONFIG_KONA_TMON
	&kona_tmon_device,
#endif
};




static void __init hawaii_init(void)
{
	
		hawaii_serial_device.dev.platform_data = &hawaii_uart_platform_data;
		hawaii_i2c_adap_devices[0].dev.platform_data = &bsc_i2c_cfg[0];
		hawaii_i2c_adap_devices[1].dev.platform_data = &bsc_i2c_cfg[1];
		hawaii_i2c_adap_devices[2].dev.platform_data = &bsc_i2c_cfg[2];
		hawaii_i2c_adap_devices[3].dev.platform_data = &bsc_i2c_cfg[3];
		hawaii_i2c_adap_devices[4].dev.platform_data = &bsc_i2c_cfg[4];
		hawaii_sdio1_device.dev.platform_data = &hawaii_sdio_param[0];
		hawaii_sdio2_device.dev.platform_data = &hawaii_sdio_param[1];
		hawaii_sdio3_device.dev.platform_data = &hawaii_sdio_param[2];
		hawaii_ssp0_device.dev.platform_data = &hawaii_ssp0_info;
		hawaii_ssp1_device.dev.platform_data = &hawaii_ssp1_info;
		hawaii_stm_device.dev.platform_data = &hawaii_stm_pdata;
		hawaii_headset_device.dev.platform_data = &hawaii_headset_data;
		/* The resource in position 2 (starting from 0) is used to fill
		 * the GPIO number. The driver file assumes this. So put the
		 * board specific GPIO number here
		 */
		hawaii_headset_device.resource[2].start = HS_IRQ;
		hawaii_headset_device.resource[2].end	= HS_IRQ;
		hawaii_pl330_dmac_device.dev.platform_data = &hawaii_pl330_pdata;
#ifdef CONFIG_BACKLIGHT_PWM
		hawaii_backlight_device.dev.platform_data = &hawaii_backlight_data;
#endif
	
#ifdef CONFIG_USB_DWC_OTG
		hawaii_hsotgctrl_platform_device.dev.platform_data
			= &hsotgctrl_plat_data;
		hawaii_usb_phy_platform_device.dev.platform_data
			= &hsotgctrl_plat_data;
#endif
	
#ifdef CONFIG_IOMMU_API
		iommu_mm_device.dev.platform_data = &iommu_mm_pdata;
#endif
#ifdef CONFIG_BCM_IOVMM
		iovmm_mm_device.dev.platform_data = &iovmm_mm_pdata;
		iovmm_mm_256mb_device.dev.platform_data = &iovmm_mm_256mb_pdata;
		ion_system_device.dev.platform_data = &ion_system_data;
		ion_system_extra_device.dev.platform_data = &ion_system_extra_data;
#endif
	
#ifdef CONFIG_ION_BCM_NO_DT
#ifdef CONFIG_IOMMU_API
		platform_device_register(&iommu_mm_device);
#endif
#ifdef CONFIG_BCM_IOVMM
		platform_device_register(&iovmm_mm_device);
		platform_device_register(&iovmm_mm_256mb_device);
		platform_device_register(&ion_system_device);
		platform_device_register(&ion_system_extra_device);
#endif
		platform_device_register(&ion_carveout_device);
#ifdef CONFIG_CMA
		platform_device_register(&ion_cma_device);
#endif
#endif
	
	/* keyboard init */
	hawaii_keyboard_init();
	
	
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
		platform_device_register(&board_gpio_keys_device);
#endif
	
		platform_add_devices(hawaii_devices, ARRAY_SIZE(hawaii_devices));
	hawaii_bt_init();
	











#ifdef CONFIG_VIDEO_ADP1653
			i2c_register_board_info(0, adp1653_flash, ARRAY_SIZE(adp1653_flash));
#endif
		

		/* tsp init I2C*/
		hawaii_touchscreen_init();
		
		
		/*  init usb */
		hawaii_muic_init();
		
		/*  init nfc */
		hawaii_nfc_init();

		/*  init sensor */
		hawaii_sensor_init();
		
		
		/*  init camera */
		hawaii_cam_init();
		
		platform_device_register(&nt35510_panel_device);

		spi_register_board_info(spi_slave_board_info,
					ARRAY_SIZE(spi_slave_board_info));

#ifdef CONFIG_FB_BRCM_KONA
	konafb_init();
#endif

platform_add_devices(hawaii_common_plat_devices,
		ARRAY_SIZE(hawaii_common_plat_devices));

	return;
}




struct platform_device *hawaii_sdio_devices[] __initdata = {
	&hawaii_sdio2_device,
	&hawaii_sdio3_device,
	&hawaii_sdio1_device,
};


static int __init hawaii_add_lateinit_devices(void)
{
	platform_add_devices(hawaii_sdio_devices,
		ARRAY_SIZE(hawaii_sdio_devices));
/*
&hawaii_sdio2_device,
&hawaii_sdio3_device,
&hawaii_sdio1_device,
*/

#ifdef CONFIG_BRCM_UNIFIED_DHD_SUPPORT
	hawaii_wlan_init();
#endif
	return 0;
}



late_initcall(hawaii_add_lateinit_devices);

MACHINE_START(HAWAII, "hawaii_ss_heat3g")
	.atag_offset = 0x100,
	.smp = smp_ops(kona_smp_ops),
	.map_io = hawaii_map_io,
	.init_irq = kona_init_irq,
	.init_time = hawaii_timer_init,
	.init_machine = hawaii_init,
#ifdef CONFIG_MOBICORE_OS
	.reserve = hawaii_mem_reserve,
#else
	.reserve = hawaii_reserve,
#endif
	.restart = hawaii_restart,
MACHINE_END
