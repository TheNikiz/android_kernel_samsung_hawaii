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
#include <asm/hardware/gic.h>
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

#include <trace/stm.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include <mach/pinmux.h>

#ifdef CONFIG_I2C_GPIO
#include <linux/i2c-gpio.h>
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

void send_usb_insert_event(enum bcmpmu_event_t event, void *para);
void send_chrgr_insert_event(enum bcmpmu_event_t event, void *para);






static enum cable_type_t set_cable_status;
static void tsu6111_usb_cb(bool attached)
{
	enum bcmpmu_chrgr_type_t chrgr_type;
	enum bcmpmu_usb_type_t usb_type;

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
			break;
	case CABLE_TYPE_NONE:
			usb_type = PMU_USB_TYPE_NONE;
			chrgr_type = PMU_CHRGR_TYPE_NONE;
			spa_data = POWER_SUPPLY_TYPE_BATTERY;
			pr_info("%s USB removed\n", __func__);
			break;
	}
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION, &chrgr_type);
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
	int spa_data = POWER_SUPPLY_TYPE_BATTERY;
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
		if (!wake_lock_active(&tsu6111_jig_wakelock))
			wake_lock(&tsu6111_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node, 0);
#endif
#endif
	} else {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
		if (wake_lock_active(&tsu6111_jig_wakelock))
			wake_unlock(&tsu6111_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
		pi_mgr_qos_request_update(&qos_node,
			PI_MGR_QOS_DEFAULT_VALUE);
#endif
#endif
	}
}
static void tsu6111_uart_cb(bool attached)
{
	pr_info("%s attached %d\n", __func__, attached);
		if (attached) {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
			if (!wake_lock_active(&tsu6111_jig_wakelock))
			wake_lock(&tsu6111_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node, 0);
#endif
#endif
	} else {
#ifndef CONFIG_SEC_MAKE_LCD_TEST
#ifdef CONFIG_HAS_WAKELOCK
			if (wake_lock_active(&tsu6111_jig_wakelock))
			wake_unlock(&tsu6111_jig_wakelock);
#endif
#ifdef CONFIG_KONA_PI_MGR
			pi_mgr_qos_request_update(&qos_node,
				PI_MGR_QOS_DEFAULT_VALUE);
#endif
#endif
		}
}

void uas_jig_force_sleep_tsu6111(void)
{
#ifdef CONFIG_HAS_WAKELOCK
	if (wake_lock_active (&tsu6111_jig_wakelock)) {
		wake_unlock (&tsu6111_jig_wakelock);
		pr_info ("Force unlock jig_uart_wl\n");
	}
#else
	pr_info ("Warning : %s - Empty function!!!\n", __func__);
#endif
#ifdef CONFIG_KONA_PI_MGR
	pi_mgr_qos_request_update (&qos_node, PI_MGR_QOS_DEFAULT_VALUE);
#endif
	return;
}


static struct fsa9485_platform_data fsa9485_pdata = {
	.usb_cb = fsa9485_usb_cb,
	.charger_cb = fsa9485_charger_cb,
	.jig_cb = fsa9485_jig_cb,
	.uart_cb = fsa9485_uart_cb,
};

/*
struct i2c_board_info  __initdata micro_usb_i2c_devices_info[]  = {
		{
				I2C_BOARD_INFO("tsu6111", 0x28 >> 1),
				.platform_data = &tsu6111_pdata,
				.irq = gpio_to_irq(GPIO_USB_INT),
		},
};
*/

static struct i2c_board_info  __initdata micro_usb_i2c_devices_info[]  = {
	{
		I2C_BOARD_INFO("fsa9485", 0x4A >> 1),
		.platform_data = &fsa9485_pdata,
		.irq = gpio_to_irq(GPIO_USB_INT),
	},
};

static struct i2c_gpio_platform_data mUSB_i2c_gpio_data = {
		.sda_pin		= GPIO_USB_I2C_SDA,
				.scl_pin = GPIO_USB_I2C_SCL,
				.udelay					= 2,
		};
static struct i2c_gpio_platform_data fsa_i2c_gpio_data = {
	.sda_pin        = GPIO_USB_I2C_SDA,
	.scl_pin = GPIO_USB_I2C_SCL,
	.udelay                 = 2,
	};

/*
static struct platform_device mUSB_i2c_gpio_device = {
		.name					= "i2c-gpio",
		.id						= TSU6111_I2C_BUS_ID,
		.dev					 = {
				.platform_data	 = &mUSB_i2c_gpio_data,
		},
};
*/

static struct platform_device fsa_i2c_gpio_device = {
	.name                   = "i2c-gpio",
	.id                     = FSA9485_I2C_BUS_ID,
	.dev                    = {
	.platform_data  = &fsa_i2c_gpio_data,
	},
};

/*
static struct platform_device *mUSB_i2c_devices[] __initdata = {
		&mUSB_i2c_gpio_device,
};
*/

static struct platform_device *mUSB_i2c_devices[] __initdata = {
	&fsa_i2c_gpio_device,
};

extern void fsa9485_wakelock_init(void);

void __init dev_muic_tsu6111_init(void)
{
		pr_info("tsu6111\n");
#if defined (CONFIG_HAS_WAKELOCK) || defined (CONFIG_KONA_PI_MGR)
		/* tsu6111_wakelock_init();   */
	fsa9485_wakelock_init();
#endif
		i2c_register_board_info(FSA9485_I2C_BUS_ID, micro_usb_i2c_devices_info,
		ARRAY_SIZE(micro_usb_i2c_devices_info));
	pr_info("mUSB_i2c_devices\n");
	platform_add_devices(mUSB_i2c_devices, ARRAY_SIZE(mUSB_i2c_devices));
		return;
}

