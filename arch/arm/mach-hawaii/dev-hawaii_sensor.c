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

#include <mach/pinmux.h>
#include "devices.h"
#include <asm/system_info.h>

#ifdef CONFIG_I2C_GPIO
#include <linux/i2c-gpio.h>
#endif

#if defined  (CONFIG_SENSORS_BMC150)
#include <linux/bst_sensor_common.h>
#endif

#if defined(CONFIG_SENSORS_K3DH) || defined(CONFIG_SENSORS_K2HH)
#include <linux/k3dh_dev.h>
#endif

#if defined(CONFIG_SENSORS_GP2AP002)
#include <linux/gp2ap002_dev.h>
#endif

#if defined(CONFIG_OPTICAL_TAOS_TRITON)
#include <linux/i2c/taos.h>
#endif

#if defined(CONFIG_SENSORS_ASP01)
#include <linux/asp01.h>
#endif

#if defined  (CONFIG_SENSORS_BMC150)
#define BMC150_SENSOR_PLACE 4

#define BMC150_SENSOR_PLACE 4

#if defined(CONFIG_SENSORS_BMA2X2)
#define ACC_INT_GPIO_PIN  92
static struct bosch_sensor_specific bss_bma2x2 = {
	.name = "bma2x2" ,
        .place = BMC150_SENSOR_PLACE,
        .irq = ACC_INT_GPIO_PIN,        
};
#endif
#if defined(CONFIG_SENSORS_BMM050)
static struct bosch_sensor_specific bss_bmm050 = {
	.name = "bmm050" ,
        .place = BMC150_SENSOR_PLACE,
};
#endif
#endif

#if defined(CONFIG_SENSORS_K3DH) || defined(CONFIG_SENSORS_K2HH)
#define ACCEL_INT_GPIO_PIN 92 
static struct k3dh_platform_data k3dh_platform_data = {
        .orientation = {
        -1, 0, 0,
        0, -1, 0,
        0, 0, -1
        },
        .irq_gpio = ACCEL_INT_GPIO_PIN,
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

#if defined(CONFIG_OPTICAL_TAOS_TRITON)
static void taos_power_on(int onoff)
{
	if (onoff) {
            struct regulator *power_regulator = NULL;
            int ret=0;
            power_regulator = regulator_get(NULL, "tcxldo_uc");
            if (IS_ERR(power_regulator)){
                printk(KERN_ERR "[TAOS] can not get prox_regulator (SENSOR_3.3V) \n");
            } else {
                ret = regulator_set_voltage(power_regulator,3300000,3300000);
                printk(KERN_INFO "[TAOS] regulator_set_voltage : %d\n", ret);
                ret = regulator_enable(power_regulator);
                printk(KERN_INFO "[TAOS] regulator_enable : %d\n", ret);
                regulator_put(power_regulator);
                mdelay(5);
            }
	}	
}
static void taos_led_onoff(int onoff)
{            
        struct regulator *led_regulator = NULL;
        int ret=0;
            
	if (onoff) {	
                led_regulator = regulator_get(NULL, "gpldo3_uc");
                if (IS_ERR(led_regulator)){
                    printk(KERN_ERR "[TAOS] can not get prox_regulator (SENSOR_LED_3.3V) \n");
                } else {
                    ret = regulator_set_voltage(led_regulator,3300000,3300000);
                    printk(KERN_INFO "[TAOS] regulator_set_voltage : %d\n", ret);
                    ret = regulator_enable(led_regulator);
                    printk(KERN_INFO "[TAOS] regulator_enable : %d\n", ret);
                    regulator_put(led_regulator);
                    mdelay(5);
                }
	} else {
                led_regulator = regulator_get(NULL, "gpldo3_uc");
		ret = regulator_disable(led_regulator); 
                printk(KERN_INFO "[TAOS] regulator_disable : %d\n", ret);
                regulator_put(led_regulator);
	}
}

#define PROXI_INT_GPIO_PIN  89
static struct taos_platform_data taos_pdata = {
	.power = taos_power_on,
	.led_on = taos_led_onoff,
	.als_int = PROXI_INT_GPIO_PIN,
	.prox_thresh_hi = 750,
	.prox_thresh_low = 580,
	.prox_th_hi_cal = 530,
	.prox_th_low_cal = 400,
	.als_time = 0xED,
	.intr_filter = 0x33,
	.prox_pulsecnt = 0x0c,
	.prox_gain = 0x28,
	.coef_atime = 50,
	.ga = 97,
	.coef_a = 1000,
	.coef_b = 1880,
	.coef_c = 642,
	.coef_d = 1140,
};
#endif

#if defined(CONFIG_SENSORS_ASP01)
#define RF_TOUCH_INT_PIN 86
#define RF_TOUCH_EN_PIN 94
static void asp01_power_onoff(int onoff)
{
	if (onoff) {	
		gpio_direction_output(RF_TOUCH_EN_PIN , 0 );
	} else {
		gpio_direction_output(RF_TOUCH_EN_PIN , 1 );
	}
}

static struct asp01_platform_data asp01_pdata = {
    	.power_on = asp01_power_onoff,
	.t_out = RF_TOUCH_INT_PIN,
	.adj_det = RF_TOUCH_INT_PIN,
    	.power_gpio = RF_TOUCH_EN_PIN,        	
};

static void grip_init_code_set(void)
{
	asp01_pdata.cr_divsr = 10;
	asp01_pdata.cr_divnd = 13;
	asp01_pdata.cs_divsr = 10;
	asp01_pdata.cs_divnd = 13;
	
	asp01_pdata.init_code[SET_UNLOCK] =	0x5a;
	asp01_pdata.init_code[SET_RST_ERR] =	0x33;
	asp01_pdata.init_code[SET_PROX_PER] =	0x30;
	asp01_pdata.init_code[SET_PAR_PER] =	0x30;
	asp01_pdata.init_code[SET_TOUCH_PER] =	0x3c;
	asp01_pdata.init_code[SET_HI_CAL_PER] =	0x18;
	asp01_pdata.init_code[SET_BSMFM_SET] =	0x31;
	asp01_pdata.init_code[SET_ERR_MFM_CYC] =0x33;
	asp01_pdata.init_code[SET_TOUCH_MFM_CYC] =0x37;
	asp01_pdata.init_code[SET_HI_CAL_SPD] =	0x19;
	asp01_pdata.init_code[SET_CAL_SPD] =	0x03;
	asp01_pdata.init_code[SET_BFT_MOT] =	0x40;
	asp01_pdata.init_code[SET_TOU_RF_EXT] =	0x00;
	asp01_pdata.init_code[SET_SYS_FUNC] =	0x10;
	asp01_pdata.init_code[SET_OFF_TIME] =	0x30;
	asp01_pdata.init_code[SET_SENSE_TIME] =	0x48;
	asp01_pdata.init_code[SET_DUTY_TIME] =	0x50;
	asp01_pdata.init_code[SET_HW_CON1] =	0x78;
	asp01_pdata.init_code[SET_HW_CON2] =	0x27;
	asp01_pdata.init_code[SET_HW_CON3] =	0x20;
	asp01_pdata.init_code[SET_HW_CON5] =	0x83;
	asp01_pdata.init_code[SET_HW_CON6] =	0x3f;
	asp01_pdata.init_code[SET_HW_CON8] =	0x20;
	asp01_pdata.init_code[SET_HW_CON10] =	0x27;
	asp01_pdata.init_code[SET_HW_CON11] =	0x00;
}
#endif

#if defined(CONFIG_SENSORS_BMC150)  || defined(CONFIG_SENSORS_GP2AP002) \
    || defined(CONFIG_OPTICAL_TAOS_TRITON) || defined(CONFIG_SENSORS_ASP01) \
    || defined(CONFIG_SENSORS_K2HH)

#if defined(CONFIG_SENSORS_BMM050)
#define BMA2X2_SLAVE_ADDR  0x10
#else
#define BMA2X2_SLAVE_ADDR  0x18
#endif

static struct i2c_board_info __initdata bsc3_i2c_boardinfo[] =
{
#if defined(CONFIG_SENSORS_BMC150)
#if defined(CONFIG_SENSORS_BMA2X2)
        {
        	I2C_BOARD_INFO("bma2x2", BMA2X2_SLAVE_ADDR),
		.platform_data = &bss_bma2x2,
        },
#endif
#if defined(CONFIG_SENSORS_BMM050)
        {
        	I2C_BOARD_INFO("bmm050", 0x12),
		.platform_data = &bss_bmm050,                
        },
#endif
#endif

#if defined(CONFIG_SENSORS_K2HH)
	{
                I2C_BOARD_INFO("k3dh", 0x1d),
		.platform_data = &k3dh_platform_data,
        },
#endif

#if defined(CONFIG_SENSORS_GP2AP002)
        {
             I2C_BOARD_INFO("gp2ap002",0x44),
             .platform_data = &gp2ap002_platform_data,
        },
#endif

#if defined(CONFIG_OPTICAL_TAOS_TRITON)
        {
		I2C_BOARD_INFO("taos", 0x39),
		.platform_data = &taos_pdata,
        },
#endif

#if defined(CONFIG_SENSORS_ASP01)
{
            I2C_BOARD_INFO("asp01", (0x48 >> 1)),
            .platform_data = &asp01_pdata,
	},
#endif
};
		
#endif

void __init hawaii_sensor_init(void)
{

#if defined(CONFIG_SENSORS_ASP01)
	grip_init_code_set();
#endif

#if defined(CONFIG_SENSORS_BMC150)  || defined(CONFIG_SENSORS_GP2AP002) \
    || defined(CONFIG_OPTICAL_TAOS_TRITON) ||defined(CONFIG_SENSORS_ASP01) \
    || defined(CONFIG_SENSORS_K2HH)
	i2c_register_board_info(2, bsc3_i2c_boardinfo, ARRAY_SIZE(bsc3_i2c_boardinfo));
#endif

	return;
}
