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
#include <linux/i2c.h>
#ifdef CONFIG_I2C_GPIO
#include <linux/i2c-gpio.h>
#endif
#include <plat/pi_mgr.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#if defined(CONFIG_RT8969) || defined(CONFIG_RT8973)
#include <linux/mfd/bcmpmu.h>
#include <linux/power_supply.h>
#include <linux/mfd/rt8973.h>
#endif

#if defined(CONFIG_SEC_CHARGING_FEATURE)
/* Samsung charging feature */
#include <linux/spa_power.h>
#endif

void send_chrgr_insert_event(enum bcmpmu_event_t event, void *para);

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock rt8973_jig_wakelock;
#endif
#ifdef CONFIG_KONA_PI_MGR
static struct pi_mgr_qos_node qos_node;
#endif

enum cable_type_t{
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
#define MUSB_I2C_BUS_ID 8
#define GPIO_USB_I2C_SDA 113
#define GPIO_USB_I2C_SCL 114
#define GPIO_USB_INT 56

#ifdef CONFIG_KONA_PI_MGR
static struct pi_mgr_qos_node qos_node;
#endif

static enum cable_type_t set_cable_status;
extern int musb_info_handler(struct notifier_block *nb, unsigned long event, void *para);

#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
extern void ist30xx_set_ta_mode(bool charging);
#endif

static void usb_attach(uint8_t attached)
{
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		int spa_data;
#endif
	enum bcmpmu_chrgr_type_t chrgr_type;
	enum bcmpmu_usb_type_t usb_type;

	printk (attached ? "USB attached\n" : "USB detached\n");

#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif
	pr_info("%s attached %d\n",__func__,attached);

	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;

	switch (set_cable_status) {
	case CABLE_TYPE_USB:
			usb_type = PMU_USB_TYPE_SDP;
			chrgr_type = PMU_CHRGR_TYPE_SDP;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		if(attached == 1)
	  		{
	  			spa_data = POWER_SUPPLY_TYPE_USB;
	  		}
	  		else if(attached == 2)
	  		{
				spa_data = POWER_SUPPLY_TYPE_USB_CDP;
	  		}
#endif
#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
			ist30xx_set_ta_mode(1);
#endif
			pr_info("%s USB attached\n" , __func__);
		/* send_usb_insert_event(BCMPMU_USB_EVENT_USB_DETECTION,
			&usb_type); */
			break;
	case CABLE_TYPE_NONE:
			usb_type = PMU_USB_TYPE_NONE;
			chrgr_type = PMU_CHRGR_TYPE_NONE;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
			spa_data = POWER_SUPPLY_TYPE_BATTERY;
#endif
#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
			ist30xx_set_ta_mode(0);
#endif
			pr_info("%s USB removed\n" , __func__);
		/* set_usb_connection_status(&usb_type);  // only set status */
			break;
	default:
		pr_info("%s other cases\n", __func__);
	}
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION , &chrgr_type);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_CHARGER, (u32 *)spa_data);
#endif
	pr_info("rt8973_usb_cb attached %d\n", attached);
}

static void uart_attach(uint8_t attached)
{
	u32 uart_para;
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
		uart_para = 1;
		musb_info_handler(NULL, 0, (u32 *)uart_para);
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
		uart_para = 0;
		musb_info_handler(NULL, 0, (u32 *)uart_para);
		}
}

static void charger_attach(int32_t attached)  
{
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data;
#endif
	enum bcmpmu_chrgr_type_t chrgr_type;

#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_data = POWER_SUPPLY_TYPE_BATTERY;
	chrgr_type = PMU_CHRGR_TYPE_NONE;
#endif
	pr_info("%s charger type %d\n",__func__,attached);

	switch (attached) {
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
	case MUIC_RT8973_CABLE_TYPE_ATT_TA:
	case MUIC_RT8973_CABLE_TYPE_0x15:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
			chrgr_type = PMU_CHRGR_TYPE_DCP;
			pr_info("%s TA attached\n", __func__);
#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
				ist30xx_set_ta_mode(1);
			#endif
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		spa_data = POWER_SUPPLY_TYPE_USB_DCP;
#endif
			break;
	case MUIC_RT8973_CABLE_TYPE_NONE:
			chrgr_type = PMU_CHRGR_TYPE_NONE;
			pr_info("%s TA removed\n", __func__);
#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
				ist30xx_set_ta_mode(0);
			#endif
			break;
	default:
			pr_info("%s None support\n", __func__);
#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
			ist30xx_set_ta_mode(0);
#endif
			break;
	}
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION,
			&chrgr_type);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_event_handler(SPA_EVT_CHARGER, (u32 *)spa_data);
#endif
	pr_info("rt8973_charger_cb attached %d\n", attached);
}

static void jig_attach(jig_type_t type, uint8_t attached)
{
	switch (type) {
	case JIG_UART_BOOT_OFF:
		printk("JIG BOOT OFF UART\n");
		break;
	case JIG_USB_BOOT_OFF:
		printk("JIG BOOT OFF USB\n");
		break;
	case JIG_UART_BOOT_ON:
		printk("JIG BOOT ON UART\n");
		break;
	case JIG_USB_BOOT_ON:
		printk("JIG BOOT ON USB\n");
		break;
	default:
		break;
		}
	pr_info("rt8973_jig_cb attached %d\n", attached);
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

static void over_temperature(void)
{
	printk("over temperature detected!\n");
}

static void over_voltage(void)
{
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	int spa_data;
#endif
	enum bcmpmu_chrgr_type_t chrgr_type;
	u32 int_detected=1;
	printk("over voltage = %d\n", int_detected);
	printk("OVP triggered by musb - %d\n", int_detected);
	spa_event_handler(SPA_EVT_OVP, (u32 *) int_detected);

#if defined (CONFIG_TOUCHSCREEN_IST30XX) || (CONFIG_TOUCHSCREEN_IST30XXB)
	ist30xx_set_ta_mode(0);
#endif
	chrgr_type = PMU_CHRGR_TYPE_NONE;
	send_chrgr_insert_event(BCMPMU_CHRGR_EVENT_CHGR_DETECTION, &chrgr_type);

#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_data = POWER_SUPPLY_TYPE_BATTERY;
	spa_event_handler(SPA_EVT_CHARGER, (u32 *)spa_data);
#endif
	pr_info("%s Cable removed\n" , __func__);
}

void uas_jig_force_sleep_rt8973(void)
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

static struct rt8973_platform_data rt8973_pdata = {
    .irq_gpio = 56,
    .cable_chg_callback = &charger_attach,
    .usb_callback = &usb_attach,
    .ocp_callback = NULL,
    .otp_callback = &over_temperature,
    .ovp_callback = &over_voltage,
    .uart_callback = &uart_attach,
    .otg_callback = NULL,
    .jig_callback = &jig_attach,

};

/* For you device setting */
static struct i2c_board_info __initdata micro_usb_i2c_devices_info[]  = {
/* Add for Ricktek RT8969 */
#ifdef CONFIG_RT8973
	{I2C_BOARD_INFO("rt8973", 0x28>>1),
	.irq = -1,
	.platform_data = &rt8973_pdata,},
#endif
};

static struct i2c_gpio_platform_data mUSB_i2c_gpio_data = {
	.sda_pin        = GPIO_USB_I2C_SDA,
	.scl_pin = GPIO_USB_I2C_SCL,
	.udelay                 = 2,
	};

static struct platform_device mUSB_i2c_gpio_device = {
		.name					= "i2c-gpio",
	.id						= MUSB_I2C_BUS_ID,
		.dev = {
				.platform_data	= &mUSB_i2c_gpio_data,
	},
};
static struct platform_device *mUSB_i2c_devices[] __initdata = {
		&mUSB_i2c_gpio_device,
};
void __init dev_muic_rt8973_init(void)
{
	pr_info("rt8973: micro_usb_i2c_devices_info\n");
	rt8973_wakelock_init();
	i2c_register_board_info(MUSB_I2C_BUS_ID, micro_usb_i2c_devices_info,
		ARRAY_SIZE(micro_usb_i2c_devices_info));
	pr_info("mUSB_i2c_devices\n");
	platform_add_devices(mUSB_i2c_devices, ARRAY_SIZE(mUSB_i2c_devices));
	return;
}
