/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
*@file	drivers/input/misc/kona_headset_multi_button.c
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

/* This should be defined before kernel.h is included */
/*#define DEBUG*/

/*
* Control configuring MIC BIAS in PERIODIC mode using a macro.
* In case if we face some problems with MIC BIAS settings, it would be easy
* to just disable this macro and try MIC BIAS configuraiton in Continuous
* mode.
*/

/*
* Driver HW resource usage info:
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* GPIO MODE
* GPIO  - Accessory insertion/removal detection.
*
* COMP2 - When an accessory is connected, this is used for the acccessory
*                     type detection. ie. Headset/Headphone/Open Cable.
*
* COMP1 - If the connected accesory is a headset, configured to detect
*         button press.
*
* NON-GPIO MODE
* COMP2 - Accessory insertion detection.When an accessory is connected,
*                     this is used for the acccessory type detection.
*                     ie. Headset/Headphone/Open Cable.
*
* COMP2_INV - Used for Accessory removal detection.
*
* COMP1 - If the connected accesory is a headset, configured to detect
*         button press
*/

//#define CONFIG_MIC_BIAS_PERIODIC
//#define CONFIG_ACI_COMP_FILTER

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/switch.h>
#include <mach/io_map.h>
#include <mach/kona_headset_pd.h>
#include <mach/rdb/brcm_rdb_aci.h>
#include <mach/rdb/brcm_rdb_auxmic.h>
#include <mach/rdb/brcm_rdb_audioh.h>
#include <mach/rdb/brcm_rdb_khub_clk_mgr_reg.h>
#ifdef DEBUG
#include <linux/sysfs.h>
#endif
#include <plat/chal/chal_aci.h>
#include <plat/kona_mic_bias.h>

/*
* The gpio_set_debounce expects the debounce argument in micro seconds
* Previously the kona implementation which is called from gpio_set_debounce
* was expecting the argument as milliseconds which was incorrect.
* The commit 38598aea6cb57290ef6ed3518b75b9f398b7694f fixed it. Hence we have
* to change to the correct way of passing the time in microseconds resolution.
*/
#define KEY_PRESS_REF_TIME	msecs_to_jiffies(10)
#define KEY_DETECT_DELAY	msecs_to_jiffies(70)
#define FALSE_KEY_AVOID_DELAY (30)
#define WAKE_LOCK_TIME						(HZ * 5)	/* 5 sec */
#define SEND_END_WAKE_LOCK_TIME			(HZ * 2)	/* 2 sec */

/*
* After configuring the ADC, it takes different 'time' for the
* ADC to settle depending on the HS type. The time outs are
* in milli seconds
*/
/*in HAL_ACC_DET_PLUG_CONNECTED*/
#define DET_PLUG_CONNECTED_SETTLE	80

/* override print functions */
#define hs_info(args...) \
	do { \
	pr_info("[HEADSET]:"args); \
	} while (0)

#define hs_err(args...) \
	do { \
	pr_err("[HEADSET ERROR]:"args); \
	} while (0)

/*
* Button/Hook Filter configuration
*/
/* = 1024 / (Filter block frequencey) = 1024 / 32768 => 31ms */
#define ACC_HW_COMP1_FILTER_WIDTH   1024

struct mic_t {
	int gpio_irq;
	int comp2_irq;
	int comp2_inv_irq;
	int comp1_irq;
	unsigned int auxmic_base;
	unsigned int aci_base;
	int recheck_jack;
	CHAL_HANDLE aci_chal_hdl;
	struct clk *audioh_apb_clk;
	struct kona_headset_pd *headset_pd;
	struct switch_dev sdev;
	struct delayed_work accessory_detect_work_deb;
	struct delayed_work accessory_detect_work;
	struct delayed_work accessory_remove_work;
	struct delayed_work button_work_deb;	
	struct delayed_work button_work;
	struct input_dev *headset_button_idev;
	struct class* audio_class;
	struct device* headset_dev;
	int hs_state;
	int hs_detecting;
	int button_state;
	int button_pressed;
	int low_voltage_mode;
	/*
	* 1 - mic bias is ON
	* 0 - mic bias is OFF
	*/
	int mic_bias_status;
#ifdef CONFIG_HAS_WAKELOCK
	/* The wakelock is added to prevent the Baseband from going to
	* suspend state while handling accessory detection/button press
	* interrupts. Note that accessory insertion, reoval and button press
	* are wakeup sources. So the following is the scenario
	* 1) No accessory is connected the base band goes to deep sleep.
	*    a) The gpio interrupt (in case if GPIO is used) and the comp2
	*    interrupt are configured as wake up sources. So the ISR will be
	*    fired.
	*    b) In the respective ISR we'll acquire this wake lock and
	*    release it from the respective work queues once the processing
	*    is over.
	* 2) Accessory is connected and the base band goes to deep sleep
	*    a) The button press should wake up the system. So the COMP1 isr
	*    is configured as wake up source. Again the same lock can be
	*    acquired by the ISR and can be released once the work queue
	*    finishes sending the button release event to the input sub-system.
	*    b) Again the GPIO interrupt in case of GPIO based detecton and
	*    COMP2 INV in case of non GPIO based detection is configured as
	*    wakeup source for accessory removal. Again, when one of the isr
	*    is fired, we should acquire this lock and should free them once
	*    the work queue has finished its task.
	*/
	struct wake_lock accessory_wklock;
#endif
	struct regulator* audio_ldo;
};

static struct mic_t *mic_dev;

enum hs_type {
	DISCONNECTED = 0,	/* Nothing is connected  */
	HEADPHONE,		/* The one without MIC   */
	HEADSET,			/* The one with MIC      */
	OPEN_CABLE,   /* Not sent to userland  */
	/* If more HS types are required to be added
	* add here, not below HS_TYPE_MAX
	*/
	HS_TYPE_MAX,
};

enum button_name {
	BUTTON_SEND_END,
	BUTTON_VOLUME_UP,
	BUTTON_VOLUME_DOWN,
	BUTTON_NAME_MAX
};

enum button_state {
	BUTTON_RELEASED = 0,
	BUTTON_PRESSED
};

/* * Default table used if the platform does not pass one */
static unsigned int button_adc_values_no_resistor[3][2] = {
	/* SEND/END Min, Max */
	{0, 110},
	/* Volume Up  Min, Max */
	{111, 250},
	/* Volue Down Min, Max */
	{251, 500},
};

static unsigned int (*button_adc_values)[2];

/* Accessory Hardware configuration support variables */
/* ADC */
static const CHAL_ACI_filter_config_adc_t aci_filter_adc_config = { 0, 0x0B };

/* COMP1 */
static const CHAL_ACI_filter_config_comp_t comp_values_for_button_press = {
	CHAL_ACI_FILTER_MODE_INTEGRATE,
	CHAL_ACI_FILTER_RESET_FIRMWARE,
	0,			/* = S */
	0xFE,			/* = T */
	0x500,
	ACC_HW_COMP1_FILTER_WIDTH	/* = MT */
};

/* COMP2 */
static const CHAL_ACI_filter_config_comp_t comp_values_for_type_det = {
	CHAL_ACI_FILTER_MODE_INTEGRATE,
	CHAL_ACI_FILTER_RESET_FIRMWARE,
	0,			/* = S  */
	0xFE,			/* = T  */
	0x700,			/* = M  */
	0x650			/* = MT */
};

static CHAL_ACI_micbias_config_t aci_mic_bias = {
	CHAL_ACI_MIC_BIAS_OFF,
	CHAL_ACI_MIC_BIAS_2_1V,
	CHAL_ACI_MIC_BIAS_PRB_CYC_256MS,
	CHAL_ACI_MIC_BIAS_MSR_DLY_4MS,
	CHAL_ACI_MIC_BIAS_MSR_INTVL_64MS,
	CHAL_ACI_MIC_BIAS_1_MEASUREMENT
};

/*
* Based on the power consumptio analysis. Program the MIC BIAS probe cycle
* to be  * 512ms. In this the mesaurement interval is 64ms and the
* measurement delay is 16ms. This means that the duty cycle is
* approximately 15.6% *
* Mic Bias Power down control  and peridoic measurement control looks like
* this

*  --                      -------------------------------
*    | <-------------->   | <---------------------------> |
*    |     64 ms          |    512-64= 448ms              |
*     --------------------
*            -------------
*    <----->| <---------> |
*      16ms |   48ms      |
* ----------               ---------------------
*
*/
static CHAL_ACI_micbias_config_t aci_init_mic_bias = {
	CHAL_ACI_MIC_BIAS_ON,
	CHAL_ACI_MIC_BIAS_2_1V,
	CHAL_ACI_MIC_BIAS_PRB_CYC_512MS,
	CHAL_ACI_MIC_BIAS_MSR_DLY_16MS,
	CHAL_ACI_MIC_BIAS_MSR_INTVL_64MS,
	CHAL_ACI_MIC_BIAS_1_MEASUREMENT
};

static CHAL_ACI_micbias_config_t aci_init_mic_bias_low = {
	CHAL_ACI_MIC_BIAS_ON,
	CHAL_ACI_MIC_BIAS_0_45V,
	CHAL_ACI_MIC_BIAS_PRB_CYC_16MS,
	CHAL_ACI_MIC_BIAS_MSR_DLY_1MS,
	CHAL_ACI_MIC_BIAS_MSR_INTVL_8MS,
	CHAL_ACI_MIC_BIAS_1_MEASUREMENT
};

/* Vref */
static CHAL_ACI_vref_config_t aci_vref_config = { CHAL_ACI_VREF_OFF };

static int __headset_hw_init_micbias_on(struct mic_t *p);
static int __headset_hw_init_micbias_off(struct mic_t *p);
static int __headset_hw_init(struct mic_t *mic);
static void __handle_accessory_inserted(struct mic_t *p);
static void __handle_accessory_removed(struct mic_t *p);
static void __low_power_mode_config(void);
static irqreturn_t gpio_isr(int irq, void *dev_id);

static ssize_t show_headset(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t store_headset(struct device *pdev, struct device_attribute *attr, const char *buf, size_t count);

#define HEADSET_ATTR(_name)											\
{																				\
	.attr = { .name = #_name, .mode = 0444, },					\
	.show = show_headset,												\
}

#define HEADSET_ATTRW(_name)											\
{																				\
	.attr = { .name = #_name, .mode = 0220, },					\
	.store = store_headset,                                	\
}

enum {
	STATE = 0,
	KEY_STATE,
	RECHECK,
};

static struct device_attribute headset_Attrs[] = {
	HEADSET_ATTR(state),
	HEADSET_ATTR(key_state),
	HEADSET_ATTRW(reselect_jack),
};

static ssize_t show_headset(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	const ptrdiff_t off = attr - headset_Attrs;

	switch (off)
	{
	case STATE:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", (((mic_dev->hs_state == HEADPHONE) || (mic_dev->hs_state == HEADSET)) ? 1 : 0));
		break;
	case KEY_STATE:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", mic_dev->button_state);
		break;
	default :
		break;
	}

	return ret;
}

static ssize_t store_headset(struct device *pdev, struct device_attribute *attr, const char *buf, size_t count)
{
	const ptrdiff_t off = attr - headset_Attrs;

	switch(off)
	{
	case RECHECK:
		{
#if defined(CONFIG_FLOODING_PROTECTION)		
			int recheck;
			sscanf(buf, "%d", &recheck);
			if(recheck)
			{
				hs_info("%s: recheck headset\n", __func__);
				mic_dev->recheck_jack = 1;
				gpio_isr(0, mic_dev);
			}
#endif			
		}
		break;
	}

	return count;
}

/* Function to dump the HW regs */
#ifdef DEBUG
static void dump_hw_regs(struct mic_t *p)
{
	int i;

	hs_info("\r\n Dumping MIC BIAS registers \r\n");
	for (i = 0x0; i <= 0x28; i += 0x04) {
		hs_info("Addr: 0x%x  OFFSET: 0x%x  Value:0x%x \r\n",
			p->auxmic_base + i, i, readl(p->auxmic_base + i));
	}

	hs_info("\r\n \r\n");
	hs_info("Dumping ACI registers \r\n");
	for (i = 0x30; i <= 0xD8; i += 0x04) {
		hs_info("Addr: 0x%x  OFFSET: 0x%x  Value:0x%x \r\n",
			p->aci_base + i, i, readl(p->aci_base + i));
	}

	for (i = 0x400; i <= 0x420; i += 0x04) {
		hs_info("Addr: 0x%x  OFFSET: 0x%x Value:0x%x \r\n",
			p->aci_base + i, i, readl(p->aci_base + i));
	}
	hs_info("\r\n \r\n");
}
#endif

static int config_adc_for_accessory_detection(void)
{
	int time_to_settle = 0;

	if (mic_dev == NULL) {
		hs_err("%s():Invalid mic_dev handle \r\n", __func__);
		return -EFAULT;
	}

	if (mic_dev->aci_chal_hdl == NULL) {
		hs_err("%s():Invalid CHAL handle \r\n", __func__);
		return -EFAULT;
	}

	pr_debug
		("config_adc_for_accessory_detection:"
		" Configuring for headphone \r\n");

	aci_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
		CHAL_ACI_BLOCK_GENERIC, &aci_mic_bias);

	/* Power up Digital block */
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_ENABLE,
		CHAL_ACI_BLOCK_DIGITAL);

	/*
	* Power up the ADC - Follow the seqeuence Power down the ADC,
	* wait and turn it ON again. This is to ensure that if ADC
	* was in some improper state, this sequence would reset it.
	* The delay is based on recommendation from ASIC team.
	* Configure the ADC in full scale mode to measure upto
	* 2300mV
	*/
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_DISABLE,
		CHAL_ACI_BLOCK_ADC);
	usleep_range(1000, 1200);
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_ENABLE,
		CHAL_ACI_BLOCK_ADC);
	usleep_range(1000, 1200);

	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_ADC_RANGE,
		CHAL_ACI_BLOCK_ADC,
		CHAL_ACI_BLOCK_ADC_HIGH_VOLTAGE);

	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
		CHAL_ACI_BLOCK_ADC, &aci_filter_adc_config);

	time_to_settle = DET_PLUG_CONNECTED_SETTLE;
	/*
	* Wait till the ADC settles, the timings varies for different types
	* of headset.
	*
	* There is a settling time after which a call back is
	* invoked, does this call back trigger any other HW config or
	* its just a notification to the upper layer ????
	*/
	if (time_to_settle != 0)
		msleep(time_to_settle);

	return 0;
}

static int config_adc_for_bp_detection(void)
{
	if (mic_dev == NULL) {
		hs_err("%s(): invalid mic_dev handle \r\n", __func__);
		return -EFAULT;
	}

	if (mic_dev->aci_chal_hdl == NULL) {
		hs_err("%s(): Invalid CHAL handle \r\n", __func__);
		return -EFAULT;
	}
	/* Set the threshold value for button press */
	/*
	* Got the feedback from the system design team that the button press
	* threshold to programmed should be 0.12V as well.
	* With this only send/end button works, so made this 600 i.e 0.6V.
	* With the threshold level set to 0.6V all the 3 button press works
	* OK.Note that this would trigger continus COMP1 Interrupts if enabled.
	* But since we don't enable COMP1 until we identify a Headset its OK.
	*/
	if (mic_dev->low_voltage_mode)
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP1, 80);
	else
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP1, mic_dev->headset_pd->comp1_threshold);

	/*
	* TODO: As of now this function uses the same MIC BIAS settings
	* and filter settings as accessory detection, this might need
	* fine tuning.
	*/
	pr_debug("Configuring ADC for button press detection \r\n");

	if (mic_dev->low_voltage_mode) {
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
			CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias_low);
		__low_power_mode_config();
	} else {
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
			CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);
	}

	/* Power up Digital block */
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl, CHAL_ACI_BLOCK_ACTION_ENABLE,
		CHAL_ACI_BLOCK_DIGITAL);

	/* Power up the ADC */
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_DISABLE,
		CHAL_ACI_BLOCK_ADC);
	usleep_range(1000, 1200);
	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_ENABLE,
		CHAL_ACI_BLOCK_ADC);
	usleep_range(1000, 1200);

	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_ADC_RANGE, CHAL_ACI_BLOCK_ADC,
		CHAL_ACI_BLOCK_ADC_LOW_VOLTAGE);

	chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
		CHAL_ACI_BLOCK_ADC, &aci_filter_adc_config);

	/*
	* Wait till the ADC settles, the timings might need fine tuning
	*/
	msleep(20);

	return 0;
}

static int read_adc_for_accessory_detection(void)
{
	int status = -1;
	int mic_level, i;
	int level_sum = 0;
	int pre_level = 0xFFFF;

	if (mic_dev == NULL) {
		hs_err("aci_adc_config: invalid mic_dev handle \r\n");
		return -EFAULT;
	}

	if (mic_dev->aci_chal_hdl == NULL) {
		hs_err("aci_adc_config: Invalid CHAL handle \r\n");
		return -EFAULT;
	}

	/*
	* What is phone_ref_offset?
	*
	* Because of the resistor in the MIC IN line the actual ground
	* is not 0,	 * but a small offset is added to it.
	* We call this as phone_ref_offset.
	*
	* This needs to be subtracted from the measured voltage to determine
	* the correct value. This will vary for different HW based on the
	* resistor values used. So by default this variable is 0, if no one
	* initializes it. For boards on which this resistor is present this
	* value should be passed from the board specific data structure
	*
	* In the below logic, if mic_level read is less than or equal to 0
	* then we don't do anything.
	* If the read value is greater than  phone_ref_offset then subtract
	* this offset from the value read, otherwise mic_level is zero
	*/
	for (i = 0; i < 3 && i > -3;)
	{
		mic_level = chal_aci_block_read(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ADC,
			CHAL_ACI_BLOCK_ADC_RAW);
		pr_debug
			(" ++ read_adc_for_accessory_detection:"
			" mic_level before calc %d \r\n",
			mic_level);
		mic_level =
			mic_level <=
			0 ? mic_level : ((mic_level > mic_dev->headset_pd->phone_ref_offset)
			? (mic_level -
			mic_dev->headset_pd->phone_ref_offset) : 0);
		pr_debug
			(" ++ read_adc_for_accessory_detection:"
			" mic_level after calc %d \r\n", mic_level);

		if( mic_dev->headset_pd->use_external_micbias)
		{
			if(mic_level < 0)
			{
				i = 0;
				mic_level = mic_dev->headset_pd->accessory_adc_values[1][1]; /* 4 Pole MAX */
				break;				
			}
		}
		else
		{
			if (unlikely(mic_level < 0))
			{
				wake_lock_timeout(&mic_dev->accessory_wklock, WAKE_LOCK_TIME);
				config_adc_for_accessory_detection();
				i--;
				continue;
			}
		}

		if (mic_level >= (pre_level - mic_dev->headset_pd->adc_check_offset) 
			&& mic_level <= (pre_level + mic_dev->headset_pd->adc_check_offset))
		{
			level_sum += mic_level;
			i++;
		}
		else
		{
			pre_level = mic_level;
			i = 0;
			level_sum = 0;
		}

		if (mic_level > mic_dev->headset_pd->accessory_adc_values[0][1] /* 3 Pole MAX */)
		{
			i = 0;
			break;
		}

		msleep(50);
	}

	if (i > 0)
		mic_level = level_sum / i;

	hs_info("EARJACK ADC : %d\n",	mic_level);

	if (mic_level >= mic_dev->headset_pd->accessory_adc_values[0][0] /* 3 Pole MIN */
		&& mic_level <= mic_dev->headset_pd->accessory_adc_values[0][1] /* 3 Pole MAX */)
	{
		status = HEADPHONE;
	}
	else if (mic_level >= mic_dev->headset_pd->accessory_adc_values[1][0] /* 4 Pole MIN */
		&& mic_level <= mic_dev->headset_pd->accessory_adc_values[1][1] /* 4 Pole MAX */)
	{
		if((!mic_dev->headset_pd->use_external_micbias)
			&& (mic_level >= mic_dev->headset_pd->accessory_adc_values[2][0] /* OPEN MIN */)
			&& !((gpio_get_value(irq_to_gpio(mic_dev->gpio_irq))) ^ mic_dev->headset_pd->hs_default_state))
			status = OPEN_CABLE;
		else
			status = HEADSET;
	}
	
#if defined(CONFIG_FLOODING_PROTECTION)
	if(mic_dev->recheck_jack == 1)
	{
		hs_info("EARJACK RECHECK : %d\n", mic_dev->recheck_jack);
		if (mic_level >= mic_dev->headset_pd->accessory_adc_values[2][0] /* OPEN MIN */
		&& mic_level <= mic_dev->headset_pd->accessory_adc_values[2][1] /* OPEN MAX */)
			status = OPEN_CABLE;
		
		mic_dev->recheck_jack = 0;
	}
#endif

	return status;
}

/* Function that is invoked when the user tries to read from sysfs interface */
static int detect_hs_type(struct mic_t *mic_dev)
{
	int type;

	if (mic_dev == NULL) {
		hs_err("mic_dev is empty \r\n");
		return 0;
	}

	config_adc_for_accessory_detection();
	type = read_adc_for_accessory_detection();

	return type;
}

static int false_button_check(struct mic_t *p)
{
	if (p->hs_state != HEADSET) {
		hs_info("%s: Acessory is not Headset\n", __func__);
		return 1;
	}

	if(p->hs_detecting)
	{
		hs_info("%s: Acessory is detecting\n", __func__);
		return 1;
	}

	return 0;
}

static int detect_button_pressed(struct mic_t *mic_dev)
{
	int i;
	int mic_level;

	if (mic_dev == NULL) {
		hs_err("mic_dev is empty \r\n");
		return 0;
	}

	/*
	* What is phone_ref_offset?
	*
	* Because of the resistor in the MIC IN line the actual ground
	* is not 0,but a small offset is added to it. We call this as
	* phone_ref_offset.
	* This needs to be subtracted from the measured voltage to determine
	* the correct value. This will vary for different HW based on the
	* resistor values used. So by default this variable is 0, if no one
	* initializes it. For boards on which this resistor is present this
	* value should be passed from the board specific data structure
	*
	* In the below logic, if mic_level read is less than or equal to 0
	* then we don't do anything.
	* If the read value is greater than  phone_ref_offset then
	* subtract this offset from the value read, otherwise
	* mic_level is zero
	*/
	mic_level = chal_aci_block_read(mic_dev->aci_chal_hdl,
		CHAL_ACI_BLOCK_ADC,
		CHAL_ACI_BLOCK_ADC_RAW);
	pr_debug("%s(): mic_level before calc %d \r\n", __func__, mic_level);
	mic_level = mic_level <= 0 ? mic_level :
		((mic_level > mic_dev->headset_pd->phone_ref_offset) ?
		(mic_level - mic_dev->headset_pd->phone_ref_offset) : 0);
	hs_info("%s(): mic_level after calc %d \r\n", __func__, mic_level);

	/* Find out what is the button pressed */
	/* Take the table based on what is passed from the board */
	for (i = BUTTON_SEND_END; i < BUTTON_NAME_MAX; i++) {
		if ((mic_level >=	button_adc_values[i][0]) &&
			(mic_level <= button_adc_values[i][1]))
			break;
	}

	return i;
}

static void button_work_deb_func(struct work_struct *work)
{
	struct mic_t *p = container_of(work, struct mic_t,
		button_work_deb.work);

	cancel_delayed_work_sync(&(p->button_work));
	schedule_delayed_work(&(p->button_work), KEY_DETECT_DELAY);
}

static void button_work_func(struct work_struct *work)
{
	struct mic_t *p = container_of(work, struct mic_t,
		button_work.work);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&p->accessory_wklock, SEND_END_WAKE_LOCK_TIME);
#endif
	if(false_button_check(p))
		goto out;
	if (p->hs_state != HEADSET) {
		hs_err
			("%s() ..scheduled while acessory is not Headset"
			" or spurious \r\n", __func__);
		return;
	}

	if (readl((void*)p->aci_base + ACI_COMP_DOUT_OFFSET) &
		ACI_COMP_DOUT_COMP1_DOUT_CMD_ONE) {
			if (p->button_state != BUTTON_PRESSED) {
				int err = 0;
				int button_name, button_name2;
				int loopcnt = 5;
				/* Find out the type of button pressed
				* by reading the ADC values */
				do{
					button_name = detect_button_pressed(p);
					usleep_range(10000, 12000);
					if(false_button_check(p))
						goto out;
					button_name2 = detect_button_pressed(p);
					loopcnt--;
				}while(button_name != button_name2 && loopcnt);

				msleep(FALSE_KEY_AVOID_DELAY);
				if(false_button_check(p))
					goto out;

				/*
				* Store which button is being pressed (KEY_VOLUMEUP,
				*	KEY_VOLUMEDOWN, KEY_MEDIA)
				* in the context structure
				*/
				/* We observe that when Headset is plugged out from the
				* connector, sometimes we get COMP1 interrupts indicating
				* button press/release. While removing the headset,
				* there is some disturbance on the MIC line and if
				* it happens to match the threshold programmed for
				* button press/release the interrupt is fired.
				* So practically we cannot avoid this situation.
				* To handle this, we update the state of the accessory
				* (connected / unconnected)in gpio_isr(). Once the button ISR
				* happens, we read the ADC values etc immediately but we
				* delay notifying this to the upper layers by
				* 250 msec(this value is arrived at by experiment) - tuned to 100 msec for doulbe pressing
				* So that if this is a spurious
				* notification, the GPIO ISR would be fired by this time
				* and would have updated the accessory state as disconnected.
				* In such cases the upper layer is not notified.
				*/
				if (p->hs_state != DISCONNECTED) {
					switch (button_name) {
					case BUTTON_SEND_END:
						p->button_pressed = KEY_MEDIA;
						break;
					case BUTTON_VOLUME_UP:
						p->button_pressed = KEY_VOLUMEUP;
						break;
					case BUTTON_VOLUME_DOWN:
						p->button_pressed = KEY_VOLUMEDOWN;
						break;
					default:
						hs_info("Button type not supported or spurious\n");
						err = 1;
						break;
					}

					if (err)
						goto out;
					/* Notify the same to input sub-system */
					p->button_state = BUTTON_PRESSED;
					hs_info("Button pressed = %d\n", button_name);
					input_report_key(p->headset_button_idev, p->button_pressed, 1);
					input_sync(p->headset_button_idev);
				}
			}

			/*
			* Seperate interrupt will not be fired for button release,
			* Need to periodically poll to see if the button is releaesd.
			* The content of the COMP DOUT register would tell us whether
			* the button is released or not.
			*/
			schedule_delayed_work(&(p->button_work), KEY_PRESS_REF_TIME);
	} else {
		/*
		* Find out which button is being pressed from the
		* context structure Notify the corresponding release
		* event to the input sub-system
		*/

		/* We observe that when Headset is plugged out from the
		* connector, sometimes we get COMP1 interrupts indicating
		* button press/release. While removing the headset,
		* there is some disturbance on the MIC line and if
		* it happens to match the threshold programmed for
		* button press/release the interrupt is fired.
		* So practically we cannot avoid this situation.
		* To handle this, we update the state of the accessory
		* (connected / unconnected)in gpio_isr(). Once the button ISR
		* happens, we read the ADC values etc immediately but we
		* delay notifying this to the upper layers by
		* 80 msec(this value is arrived at by experiment)
		* So that if this is a spurious
		* notification, the GPIO ISR would be fired by this time
		* and would have updated the accessory state as disconnected.
		* In such cases the upper layer is not notified.
		*/
		usleep_range(10000, 20000);

		if(p->button_state == BUTTON_PRESSED)
		{
			p->button_state = BUTTON_RELEASED;
			hs_info("Button release = %d\n", p->button_pressed);
			input_report_key(p->headset_button_idev, p->button_pressed, 0);
			input_sync(p->headset_button_idev);
		}

		if(p->hs_state == HEADSET)
		{
			/* Acknowledge & clear the interrupt */
			chal_aci_block_ctrl(p->aci_chal_hdl,
				CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
				CHAL_ACI_BLOCK_COMP1);

			/* Re-enable COMP1 Interrupts */
			chal_aci_block_ctrl(p->aci_chal_hdl,
				CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
				CHAL_ACI_BLOCK_COMP1);
		}
	}
	return ;

out:
	/* Re-enable COMP1 Interrupts */ // wait until button_work process (80 to 100 msec of delay)
	if(p->button_state == BUTTON_PRESSED)
	{
		p->button_state = BUTTON_RELEASED;
		hs_info("Button release=%d\n", p->button_pressed);
		input_report_key(p->headset_button_idev, p->button_pressed, 0);
		input_sync(p->headset_button_idev);
	}

	if(p->hs_state == HEADSET)
	{
		/* Acknowledge & clear the interrupt */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
			CHAL_ACI_BLOCK_COMP1);

		/* Re-enable COMP1 Interrupts */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
			CHAL_ACI_BLOCK_COMP1);
	}
	return;
}

static void accessory_detect_work_deb_func(struct work_struct *work)
{
	struct mic_t *p = container_of(work, struct mic_t,
		accessory_detect_work_deb.work);
	int work_delay = p->headset_pd->jack_insertion_settle_time;

	if(p->hs_state != DISCONNECTED)
		work_delay = p->headset_pd->jack_remove_settle_time;

	cancel_delayed_work_sync(&(p->accessory_detect_work));
	schedule_delayed_work(&(p->accessory_detect_work), msecs_to_jiffies(work_delay));
	p->hs_detecting = 1;
}

/*------------------------------------------------------------------------------
Function name   : accessory_detect_work_func
Description     : Work function that will set the state of the headset
switch dev and enable/disable the HS button interrupt
Return type     : void
------------------------------------------------------------------------------*/
static void accessory_detect_work_func(struct work_struct *work)
{
	struct mic_t *p = container_of(work, struct mic_t,
		accessory_detect_work.work);
	unsigned int accessory_inserted = (gpio_get_value(irq_to_gpio(p->gpio_irq)) ^ p->headset_pd->hs_default_state);
	/*
	* If the platform supports GPIO for accessory insertion/removal, use
	* the state of the GPIO to decide whether accessory is inserted or
	* removed
	*/
	hs_info("SWITCH WORK GPIO STATE: 0x%x default state 0x%x\n",
		accessory_inserted, p->headset_pd->hs_default_state);

	if (accessory_inserted == 1)
	{
		__headset_hw_init_micbias_on(p);
		__handle_accessory_inserted(p);

		/*
		* Enable the COMP1 interrupt for button press detection in
		* case if the inserted item is Headset
		*/
		if (p->hs_state == HEADSET) {

			msleep(FALSE_KEY_AVOID_DELAY);

			chal_aci_block_ctrl(p->aci_chal_hdl,
				CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
				CHAL_ACI_BLOCK_COMP);

			chal_aci_block_ctrl(p->aci_chal_hdl,
				CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
				CHAL_ACI_BLOCK_COMP1);

			msleep(FALSE_KEY_AVOID_DELAY);
		}
		else 		/* Switch OFF the MIC BIAS in case of Headphone */
			__headset_hw_init_micbias_off(p);
	}
	else
	{
		int state = p->hs_state;
		__handle_accessory_removed(p);

		if(state == HEADSET)
		{
			int msleeptime = 400;
			long unsigned int reg_val;

			if (p->audioh_apb_clk) {
				hs_info("[%s] enable AudioH_APB_CLK\n", __func__);
				clk_enable(p->audioh_apb_clk);
			}
			reg_val = readl(KONA_AUDIOH_VA + AUDIOH_AUDIORX_VRX1_OFFSET);
			if (!(reg_val & 0x1))	{ // using mic
				msleeptime = 1000;

				hs_info("Set MIC PGA by 0 at AUDIORX by force.\n");
				hs_info("... expecting to restore it at switching path in audio driver\n");

				reg_val &= ~AUDIOH_AUDIORX_VRX1_AUDIORX_VRX_GAINCTRL_MASK;
				writel(reg_val, KONA_AUDIOH_VA + AUDIOH_AUDIORX_VRX1_OFFSET);
			}
			if (p->audioh_apb_clk) {
				clk_disable(p->audioh_apb_clk);
				hs_info("[%s] disable AudioH_APB_CLK\n", __func__);
			}

			/* First inform userland about the removal.
			* A delay is necessary here to allow the
			* application to complete the routing*/
			hs_info("Micbias off delay %dms\n", msleeptime);
			msleep(msleeptime);
		}

		/* Turn Off, MIC BIAS */
		__headset_hw_init_micbias_off(p);
	}

	p->hs_detecting = 0;
}

static void no_gpio_accessory_insert_work_func(struct work_struct *work)
{
	struct mic_t *p = container_of(work, struct mic_t,
		accessory_detect_work.work);
	int comp2_level;

	pr_debug("+ %s() \r\n", __func__);

	/*
	* Set MIC BIAS to 2.1V
	* and Set COMP2 threshold to 1.9V
	*/
	aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
	aci_init_mic_bias.voltage = CHAL_ACI_MIC_BIAS_2_1V;

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
		CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP2, 1900);

	/*
	* Read COMP2 level, if accessory is plugged in low level is
	* expected
	*/
	comp2_level = chal_aci_block_read(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_COMP2,
		CHAL_ACI_BLOCK_COMP_RAW);

	if (comp2_level == CHAL_ACI_BLOCK_COMP_LINE_LOW) {

		__handle_accessory_inserted(p);

		/*
		* IMPORTANT
		* ---------
		*
		* This is done to handle spurious button press interrupt.
		* Note that when the COMP2 is used for accessory insertion
		* detection. Soon after the accessory is inserted I see two
		* spurious interrupts, one for COMP2_INV and one for COMP1
		*
		* COMP1 spurious handling
		* ------------------------
		* Luckily what I observed is that the COMP1 (button press)
		* interrupt happens earlier than the COMP2 INV. So if we just
		* delay enabling the COMP1 interrupt by 100ms, then we can
		* avoid this spurious interrupt. Also since we are
		* Acknowledging all the COMP interrupts in the below code,
		* the COMP1 interrupt that had happened would get cleared
		* below. Note that this is just a workaround and will live
		* here until we figure out the root cause for this spurious
		* interrupt. There is no side effect of  this delay because
		* the notification of accessory insertion has already been
		* passed on to the  Switch class layer. If some one presses a
		* button just after insertion within 100ms delay then we'll
		* miss the button press, but its not practical for a user to
		* press a button within 100ms of accessory insertion.
		*
		* COMP2 INV spurious handling
		* ---------------------------
		* We have not done anything special here because even though
		* the COMP2 INV work gets scheduled, since there is a level
		* check happenning in the Work queue, it promptly identifies
		* this as a spurious condition.
		*/
		msleep(100);

		/*
		* Now that accessory insertion is detected, put MIC BIAS to
		* 0.4V and COMP2 threshold to 0.36V
		*/
#ifdef CONFIG_MIC_BIAS_PERIODIC
		aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_DISCONTINUOUS;
#else
		aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
#endif

		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
			CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);

		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
			CHAL_ACI_BLOCK_COMP2, 360);

		/* Clear pending interrupts if any */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
			CHAL_ACI_BLOCK_COMP);

		/*
		* Enable COMP2_INV ISR for accessory removal
		* and in case if the connected accessory is Headset enable
		* COMP1 ISR for button press
		*/
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
			CHAL_ACI_BLOCK_COMP2_INV);

		if (p->hs_state == HEADSET) {
			chal_aci_block_ctrl(p->aci_chal_hdl,
				CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
				CHAL_ACI_BLOCK_COMP1);
		}
	} else {
		pr_debug
			("%s(): spurious COMP2 level is high (low expected)\r\n",
			__func__);

		/* Just re-enable the COMP2 ISR for further detection */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
			CHAL_ACI_BLOCK_COMP2);
	}
#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&p->accessory_wklock);
#endif
}

static void no_gpio_accessory_remove_work_func(struct work_struct *work)
{
	struct mic_t *p = container_of(work, struct mic_t,
		accessory_remove_work.work);
	int comp2_level;

	pr_debug("+ %s() \r\n", __func__);

	/*
	* Set MIC BIAS to 2.1V
	* and Set COMP2 threshold to 1.9V
	*/
	aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
	aci_init_mic_bias.voltage = CHAL_ACI_MIC_BIAS_2_1V;

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
		CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP2, p->headset_pd->comp2_threshold);

	/*
	* Read COMP2 level, if accessory is plugged out high level is
	* expected
	*/
	comp2_level = chal_aci_block_read(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_COMP2,
		CHAL_ACI_BLOCK_COMP_RAW);

	if (comp2_level == CHAL_ACI_BLOCK_COMP_LINE_HIGH) {

		__handle_accessory_removed(p);

		/*
		* Now that accessory insertion is detected, put MIC BIAS to
		* 0.4V and COMP2 to 0.36V
		*/
#ifdef CONFIG_MIC_BIAS_PERIODIC
		aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_DISCONTINUOUS;
#else
		aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
#endif
		aci_init_mic_bias.voltage = CHAL_ACI_MIC_BIAS_0_45V;

		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
			CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);

		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
			CHAL_ACI_BLOCK_COMP2, p->headset_pd->comp2_threshold);

		/* Disable comparator interrupts */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
			CHAL_ACI_BLOCK_COMP);

		/* Just keep the COMP2 enabled for next accessory detection. */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
			CHAL_ACI_BLOCK_COMP2);

	} else {
		pr_debug
			("%s(): spurious COMP2 level is low (high	expected)\r\n",
			__func__);

		/* Renable the COMP2 INV for accessory removal */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
			CHAL_ACI_BLOCK_COMP2_INV);
	}
#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&p->accessory_wklock);
#endif
}

static void __handle_accessory_inserted(struct mic_t *p)
{
	int pre_type =	p->hs_state;
	hs_info("ACCESSORY INSERTED\n");

	p->hs_state = detect_hs_type(p);

	if(pre_type == p->hs_state)
	{
		hs_info("Duplicated type=%d, skip type update\n", p->hs_state);
		return ;
	}	

	switch (p->hs_state) {

	case OPEN_CABLE:
		hs_info("ACCESSORY OPEN\n");
		__handle_accessory_removed(p);   
		break;

	case HEADSET:
		/* Put back the aci interface to be able to detect the
		* button press. Especially this functions puts the
		* COMP2 and MIC BIAS values to be able to detect
		* button press
		*/
		p->button_state = BUTTON_RELEASED;

		/* Clear pending interrupts if any */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
			CHAL_ACI_BLOCK_COMP);

		/* Configure the ADC to read button press values */
		config_adc_for_bp_detection();

		/* Fall through to send the update to userland */
	case HEADPHONE:
		/* Clear pending interrupts if any */
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
			CHAL_ACI_BLOCK_COMP);

		/* No need to enable the button press/release irq */ 
		/* prevent suspend to allow user space to respond to switch */
#ifdef CONFIG_HAS_WAKELOCK		
		wake_lock_timeout(&p->accessory_wklock, WAKE_LOCK_TIME);
#endif

		/*
		* While notifying this to the androind world we need to pass
		* the accessory typ as Androind understands. The Android
		* 'state' variable is a bitmap as defined below.
		* BIT 0 : 1 - Headset (with MIC) connected
		* BIT 1 : 1 - Headphone (the one without MIC) is connected
		*/
		switch_set_state(&(p->sdev), (p->hs_state == HEADSET) ? 1 : 2);
		hs_info("INSERTED ACCESSORY %d POLE\n", (p->hs_state == HEADSET) ? 4 : 3);
		break;
	default:
		hs_err("%s():Unknown accessory type %d \r\n", __func__, p->hs_state);
		break;
	}
}

static void __handle_accessory_removed(struct mic_t *p)
{
	hs_info("ACCESSORY REMOVED\n");
	/*
	* Now that we have moved the state handling to ISR, from the work
	* queue no need to check for the current state.
	*/
	/* Inform userland about accessory removal */
	p->hs_state = DISCONNECTED;
	if (p->button_state == BUTTON_PRESSED) {
		p->button_state = BUTTON_RELEASED;
		input_report_key(p->headset_button_idev,
			p->button_pressed, 0);
		input_sync(p->headset_button_idev);
		hs_info("Force button release %d\n", p->button_pressed);
	} else
		p->button_state = BUTTON_RELEASED;

	switch_set_state(&(p->sdev), p->hs_state);

	/* Clear pending interrupts */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP);

	/* Disable the interrupts */
	if (p->headset_pd->gpio_for_accessory_detection == 1) {
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
			CHAL_ACI_BLOCK_COMP);
	} else {
		/*
		* In case where Non GPIO based detection is used,
		* we'll disable the COMP1 that his used for button
		* press and COMP2 INV that is used for accessory
		* removal
		*/
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
			CHAL_ACI_BLOCK_COMP1);

		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
			CHAL_ACI_BLOCK_COMP2_INV);
	}
}

/*------------------------------------------------------------------------------
Function name   : hs_unregswitchdev
Description     : Unregister input device for headset
Return type     : int
------------------------------------------------------------------------------*/
int hs_unregswitchdev(struct mic_t *p)
{
	cancel_delayed_work_sync(&p->accessory_detect_work);
	if (p->headset_pd->gpio_for_accessory_detection != 1)
		cancel_delayed_work_sync(&p->accessory_remove_work);
	switch_dev_unregister(&p->sdev);
	return 0;
}

/*------------------------------------------------------------------------------
Function name   : hs_switchinit
Description     : Register sysfs device for headset
It uses class switch from kernel/common/driver/switch
Return type     : int
------------------------------------------------------------------------------*/
int hs_switchinit(struct mic_t *p)
{
	int result;
	p->sdev.name = "h2w";
	p->sdev.state = 0;

	result = switch_dev_register(&p->sdev);
	if (result < 0)
		return result;

	if (p->headset_pd->gpio_for_accessory_detection == 1)
	{
		INIT_DELAYED_WORK(&(p->accessory_detect_work_deb), accessory_detect_work_deb_func);
		INIT_DELAYED_WORK(&(p->accessory_detect_work), accessory_detect_work_func);
	}
	else {
		INIT_DELAYED_WORK(&(p->accessory_detect_work), no_gpio_accessory_insert_work_func);
		INIT_DELAYED_WORK(&(p->accessory_remove_work), no_gpio_accessory_remove_work_func);
	}
	return 0;
}

/*------------------------------------------------------------------------------
Function name   : hs_unreginputdev
Description     : unregister and free the input device for headset button
Return type     : int
------------------------------------------------------------------------------*/
static int hs_unreginputdev(struct mic_t *p)
{
	cancel_delayed_work_sync(&p->button_work);
	input_unregister_device(p->headset_button_idev);
	input_free_device(p->headset_button_idev);
	return 0;
}

/*------------------------------------------------------------------------------
Function name   : hs_inputdev
Description     : Create and Register input device for headset button
Return type     : int
------------------------------------------------------------------------------*/
static int hs_inputdev(struct mic_t *p)
{
	int result;

	/* Allocate struct for input device */
	p->headset_button_idev = input_allocate_device();
	if ((p->headset_button_idev) == NULL) {
		hs_err("%s: Not enough memory\n", __func__);
		result = -EINVAL;
		goto inputdev_err;
	}

	/* specify key event type and value for it -
	* Since we have only one button on headset,value KEY_MEDIA is sent */
	set_bit(EV_KEY, p->headset_button_idev->evbit);
	set_bit(KEY_MEDIA, p->headset_button_idev->keybit);
	set_bit(KEY_VOLUMEDOWN, p->headset_button_idev->keybit);
	set_bit(KEY_VOLUMEUP, p->headset_button_idev->keybit);

	p->headset_button_idev->name = "bcm_headset";
	p->headset_button_idev->phys = "headset/input0";
	p->headset_button_idev->id.bustype = BUS_HOST;
	p->headset_button_idev->id.vendor = 0x0001;
	p->headset_button_idev->id.product = 0x0100;

	/* Register input device for headset */
	result = input_register_device(p->headset_button_idev);
	if (result) {
		hs_err("%s: Failed to register device\n", __func__);
		goto inputdev_err;
	}

	INIT_DELAYED_WORK(&(p->button_work_deb), button_work_deb_func);
	INIT_DELAYED_WORK(&(p->button_work), button_work_func);

inputdev_err:
	return result;
}

/*------------------------------------------------------------------------------
Function name   : gpio_isr
Description     : interrupt handler
Return type     : irqreturn_t
------------------------------------------------------------------------------*/
static irqreturn_t gpio_isr(int irq, void *dev_id)
{
	struct mic_t *p = (struct mic_t *)dev_id;
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&p->accessory_wklock, WAKE_LOCK_TIME);
#endif
	p->hs_detecting = 1;
	hs_info("%s occur : GPIO STATE: 0x%x\n", __func__, gpio_get_value(irq_to_gpio(p->gpio_irq)));
	schedule_delayed_work(&(p->accessory_detect_work_deb), msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------
Function name   : comp1_isr
Description     : interrupt handler
Return type     : irqreturn_t
------------------------------------------------------------------------------*/
static irqreturn_t comp1_isr(int irq, void *dev_id)
{
	struct mic_t *p = (struct mic_t *)dev_id;

	hs_info("[%s] occur: 0x%x\n",__func__, readl((void*)p->aci_base + ACI_INT_OFFSET));
	if ((readl((void*)p->aci_base + ACI_INT_OFFSET) & 0x01) != 0x01) {
		hs_info("Spurious comp1 isr \r\n");
		return IRQ_HANDLED;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&p->accessory_wklock, SEND_END_WAKE_LOCK_TIME);
#endif

	//Prevent  [when remove the earjack, occur comp1_isr]
	if(gpio_get_value(irq_to_gpio(p->gpio_irq)))
	{ 
		hs_info("Spurious_2 comp1 isr\n");
		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
			CHAL_ACI_BLOCK_COMP1);

		chal_aci_block_ctrl(p->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
			CHAL_ACI_BLOCK_COMP1);

		return IRQ_HANDLED;
	}

	/* Acknowledge & clear the interrupt */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP1);

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
		CHAL_ACI_BLOCK_COMP1);

	schedule_delayed_work(&(p->button_work_deb), msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------
Function name   : comp2_isr
Description     : interrupt handler
Return type     : irqreturn_t
------------------------------------------------------------------------------*/

irqreturn_t comp2_isr(int irq, void *dev_id)
{
	struct mic_t *p = (struct mic_t *)dev_id;
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&p->accessory_wklock, WAKE_LOCK_TIME);
#endif
	hs_info("[%s] occur", __func__);
	pr_debug("In COMP 2  ISR 0x%x ... \r\n",
		readl((void*)p->aci_base + ACI_INT_OFFSET));

	/* Acknowledge & clear the interrupt */

	/*
	* Note that we are clearing all the interrupts here instead of just
	* COMP2. There is a reason for it.
	*
	* What we observed is that, with the existing configuration where
	* accessory detection happens via COMP2 and not GPIO, when an
	* accessory is plugged in we get a COMP1 interrupt as well.
	*
	* We have to root cause the same and fix it. In the interim as a SW
	* workaround, when we get a COMP2 interrupt (which can happen only
	* when an accessory is plugged in) we clear all the pending
	* interrupts because they are spurious (there cannot be an button
	* press when we are plugging in the accessory). But since the GIC
	* would any way raise the COMP1 or COMP2 INV interrupt, in the
	* respective handles we check whether the respective bits are ON in
	* the ACI_INT register and if not, we'll just return.
	*/
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP);

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
		CHAL_ACI_BLOCK_COMP2);

	pr_debug("COMP2 After clearing 0x%x ... \r\n",
		readl((void*)p->aci_chal_hdl + ACI_INT_OFFSET));

	schedule_delayed_work(&(p->accessory_detect_work),
		msecs_to_jiffies(p->headset_pd->jack_insertion_settle_time));

	return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------
Function name   : comp2_inv_isr
Description     : interrupt handler
Return type     : irqreturn_t
------------------------------------------------------------------------------*/
irqreturn_t comp2_inv_isr(int irq, void *dev_id)
{
	struct mic_t *p = (struct mic_t *)dev_id;
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&p->accessory_wklock, WAKE_LOCK_TIME);
#endif
	pr_debug("In INV COMP 2 ISR 0x%x ... \r\n",
		readl((void*)p->aci_base + ACI_INT_OFFSET));

	if ((readl((void*)p->aci_base + ACI_INT_OFFSET) & 0x04) != 0x04) {
		hs_info("Spurious comp2 inv isr \r\n");
#ifdef CONFIG_HAS_WAKELOCK
		wake_unlock(&p->accessory_wklock);
#endif
		return IRQ_HANDLED;
	}

	/* Acknowledge & clear the interrupt */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP2_INV);

	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
		CHAL_ACI_BLOCK_COMP2_INV);

	pr_debug("INV COMP2 After clearing 0x%x ... \r\n",
		readl((void*)p->aci_chal_hdl + ACI_INT_OFFSET));
	schedule_delayed_work(&(p->accessory_remove_work),
		msecs_to_jiffies(p->headset_pd->jack_remove_settle_time));
	return IRQ_HANDLED;
}

static int __headset_hw_init_micbias_off(struct mic_t *p)
{
	if (p == NULL)
		return -1;

	/* First disable all the interrupts */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
		CHAL_ACI_BLOCK_COMP);

	/* Clear pending interrupts if any */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP);

	/*
	* Connect P_MIC_DATA_IN to P_MIC_OUT  and P_MIC_OUT to COMP2
	* Note that one API can do this.
	*/
	chal_aci_set_mic_route(p->aci_chal_hdl, CHAL_ACI_MIC_ROUTE_MIC);

	/* Power down the COMP blocks */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_DISABLE,
		CHAL_ACI_BLOCK_COMP);

	aci_vref_config.mode = CHAL_ACI_VREF_OFF;
	chal_aci_block_ctrl(p->aci_chal_hdl, CHAL_ACI_BLOCK_ACTION_VREF,
		CHAL_ACI_BLOCK_GENERIC, &aci_vref_config);

	/* Switch OFF Mic BIAS only if its not already OFF */
	if (p->mic_bias_status == 1) {
		kona_mic_bias_off();
		hs_info("called kona_mic_bias_off\n");
		p->mic_bias_status = 0;
	}
	
	if(mic_dev->headset_pd->use_external_micbias)
	{
		mic_dev->headset_pd->disable_external_ldo();
	}

	/* Turning MICBIAS LDO OPMODE to OFF in DSM */
	if (p->audio_ldo) {
		hs_info("Turning regulator off\n");
		regulator_set_mode(p->audio_ldo, REGULATOR_MODE_STANDBY);
		regulator_put(p->audio_ldo);
		p->audio_ldo = NULL;
	}

	return 0;
}

static int __headset_hw_init_micbias_on(struct mic_t *p)
{
	if (p == NULL)
		return -1;

	if (!p->headset_pd->audio_ldo_id) {
		hs_err("%s: No LDO id passed in pdata\n", __func__);
	} else {
		p->audio_ldo = regulator_get(NULL, p->headset_pd->audio_ldo_id);
		if (IS_ERR_OR_NULL(p->audio_ldo)) {
			hs_err("%s: ERR/NULL getting MICBIAS LDO\n", __func__);
			p->audio_ldo = NULL;
			return -ENOENT;
		}
		hs_info("Setting regulator mode to low power\n");
		regulator_set_mode(p->audio_ldo, REGULATOR_MODE_IDLE);
	}

	/*
	* IMPORTANT
	*---------
	* Configuring these AUDIOH MIC registers was required to get ACI interrupts
	*for button press. Really not sure about the connection.
	* But this logic was taken from the BLTS code and if this is not
	*done then we were not getting the ACI interrupts for button press.
	*
	* Looks like if the Audio driver init happens this is not required, in
	*case if Audio driver is not included in the build then this macro should
	*be included to get headset working.
	*
	* Ideally if a macro is used to control brcm audio driver inclusion that does
	* AUDIOH init, then we	 don't need another macro here, it can be something
	*like #ifndef CONFING_BRCM_AUDIOH
	*
	*/
#ifdef CONFIG_HS_PERFORM_AUDIOH_SETTINGS
	/* AUDIO settings */
	writel(AUDIOH_AUDIORX_VRX1_AUDIORX_VRX_SEL_MIC1B_MIC2_MASK,
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_VRX1_OFFSET);
	writel(0x0, KONA_AUDIOH_VA + AUDIOH_AUDIORX_VRX2_OFFSET);
	writel((AUDIOH_AUDIORX_VREF_AUDIORX_VREF_POWERCYCLE_MASK |
		AUDIOH_AUDIORX_VREF_AUDIORX_VREF_FASTSETTLE_MASK),
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_VREF_OFFSET);
	writel((AUDIOH_AUDIORX_VMIC_AUDIORX_VMIC_CTRL_MASK |
		AUDIOH_AUDIORX_VMIC_AUDIORX_MIC_EN_MASK),
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_VMIC_OFFSET);
	writel(AUDIOH_AUDIORX_BIAS_AUDIORX_BIAS_PWRUP_MASK,
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_BIAS_OFFSET);
#endif
	/* Power up the COMP blocks */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_ENABLE,
		CHAL_ACI_BLOCK_COMP);

	/* First disable all the interrupts */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
		CHAL_ACI_BLOCK_COMP);

	/* Clear pending interrupts if any */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP);

	pr_debug("=== aci_interface_init:"
		" Interrupts disabled \r\n");

	/* Turn ON only if its not already ON */
	if (p->mic_bias_status == 0) {
		kona_mic_bias_on();
		hs_info("called kona_mic_bias_on\n");
		p->mic_bias_status = 1;
	}

	if(mic_dev->headset_pd->use_external_micbias)
	{
		mic_dev->headset_pd->enable_external_ldo();
	}

	aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
		CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);

#ifdef CONFIG_ACI_COMP_FILTER
	/* Configure comparator 1 for button press */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
		CHAL_ACI_BLOCK_COMP1,
		&comp_values_for_button_press);

	pr_debug("=== %s: ACI Block1 comprator1 configured \r\n", __func__);

	/* Configure the comparator 2 for accessory detection */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
		CHAL_ACI_BLOCK_COMP2, &comp_values_for_type_det);

	pr_debug("=== %s: ACI Block2 comprator2 configured \r\n", __func__);
#endif

	/*
	* Connect P_MIC_DATA_IN to P_MIC_OUT  and P_MIC_OUT to COMP2
	* Note that one API can do this.
	*/
	chal_aci_set_mic_route(p->aci_chal_hdl, CHAL_ACI_MIC_ROUTE_MIC);

	pr_debug("=== %s: Configured MIC route \r\n", __func__);

	/* Fast power up the Vref of ADC block */
	/*
	* NOTE:
	* This chal call was failing becuase internally this call
	*was configuring AUDIOH registers as well. We have commmented
	*configuring AUDIOH reigsrs in CHAL and it works OK
	*/
	aci_vref_config.mode = CHAL_ACI_VREF_FAST_ON;
	chal_aci_block_ctrl(p->aci_chal_hdl, CHAL_ACI_BLOCK_ACTION_VREF,
		CHAL_ACI_BLOCK_GENERIC, &aci_vref_config);

	pr_debug("=== %s: Configured Vref and ADC \r\n", __func__);

	/* Set the threshold value for accessory type detection */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP2, p->headset_pd->comp2_threshold);

	pr_debug("===%s Configured the threshold value for "
		"button press\r\n", __func__);

	return 0;
}

static int __headset_hw_init(struct mic_t *p)
{
	if (p == NULL)
		return -1;

	/*
	* IMPORTANT
	* ---------
	* Configuring these AUDIOH MIC registers was required to get ACI interrupts
	* for button press. Really not sure about the connection.
	* But this logic was taken from the BLTS code and if this is not
	* done then we were not getting the ACI interrupts for button press.
	*
	* Looks like if the Audio driver init happens this is not required, in
	* case if Audio driver is not included in the build then this macro should
	* be included to get headset working.
	*
	* Ideally if a macro is used to control brcm audio driver inclusion that does
	* AUDIOH init, then we	 don't need another macro here, it can be something
	* like #ifndef CONFING_BRCM_AUDIOH
	*
	*/
#ifdef CONFIG_HS_PERFORM_AUDIOH_SETTINGS
	/* AUDIO settings */
	writel(AUDIOH_AUDIORX_VRX1_AUDIORX_VRX_SEL_MIC1B_MIC2_MASK,
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_VRX1_OFFSET);
	writel(0x0, KONA_AUDIOH_VA + AUDIOH_AUDIORX_VRX2_OFFSET);
	writel((AUDIOH_AUDIORX_VREF_AUDIORX_VREF_POWERCYCLE_MASK |
		AUDIOH_AUDIORX_VREF_AUDIORX_VREF_FASTSETTLE_MASK),
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_VREF_OFFSET);
	writel((AUDIOH_AUDIORX_VMIC_AUDIORX_VMIC_CTRL_MASK |
		AUDIOH_AUDIORX_VMIC_AUDIORX_MIC_EN_MASK),
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_VMIC_OFFSET);
	writel(AUDIOH_AUDIORX_BIAS_AUDIORX_BIAS_PWRUP_MASK,
		KONA_AUDIOH_VA + AUDIOH_AUDIORX_BIAS_OFFSET);
#endif

	/* First disable all the interrupts */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_DISABLE,
		CHAL_ACI_BLOCK_COMP);

	/* Clear pending interrupts if any */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP);

	pr_debug("=== __headset_hw_init: Interrupts disabled \r\n");

	/*
	* For Non GPIO case, switch it ON permanantly
	*/
	kona_mic_bias_on();
	p->mic_bias_status = 1;

	if(mic_dev->headset_pd->use_external_micbias)
	{
		mic_dev->headset_pd->enable_external_ldo();
	}

	/*
	* This ensures that the timing parameters are configured
	* properly. Note that only when the mode is
	* CHAL_ACI_MIC_BIAS_DISCONTINUOUS, this is done.
	*/

	/*
	* In case where COMP1 is used for accessory insertion/removal mic
	* bias is to be configured as 0.4V
	*/
	if (p->headset_pd->gpio_for_accessory_detection != 1)
		aci_init_mic_bias.voltage = CHAL_ACI_MIC_BIAS_0_45V;

	/* Turn ON the MIC BIAS and put it in continuous mode */
	/*
	* NOTE:
	* This chal call was failing becuase internally this call
	* was configuring AUDIOH registers as well. We have commmented
	* configuring AUDIOH reigsrs in CHAL and it works OK
	*/
#ifdef CONFIG_MIC_BIAS_PERIODIC
	/*
	* Optional, but for efficient Power Mgmt enabled
	* periodic mic bias mode
	*/
	hs_info("%s(): ***************** CONFIG_MIC_BIAS_PERIODIC"
		" enabled \r\n", __func__);	/* tbr */
	aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_DISCONTINUOUS;
#else
	aci_init_mic_bias.mode = CHAL_ACI_MIC_BIAS_ON;
#endif
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
		CHAL_ACI_BLOCK_GENERIC, &aci_init_mic_bias);

	pr_debug("=== __headset_hw_init:"
		" MIC BIAS settings done \r\n");

#ifdef CONFIG_ACI_COMP_FILTER
	/* Configure comparator 1 for button press */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
		CHAL_ACI_BLOCK_COMP1,
		&comp_values_for_button_press);

	pr_debug
		("=== __headset_hw_init:"
		" ACI Block1 comprator1 configured \r\n");

	/* Configure the comparator 2 for accessory detection */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
		CHAL_ACI_BLOCK_COMP2, &comp_values_for_type_det);

	pr_debug
		("=== __headset_hw_init:"
		" ACI Block2 comprator2 configured \r\n");
#endif

	/*
	* Connect P_MIC_DATA_IN to P_MIC_OUT  and P_MIC_OUT to COMP2
	* Note that one API can do this.
	*/
	chal_aci_set_mic_route(p->aci_chal_hdl, CHAL_ACI_MIC_ROUTE_MIC);

	/*
	* Ensure that P_MIC_DATA_IN is connected to both COMP1
	* and COMP2. On Rhea by default P_MIC_DATA_IN is always routed to
	* COMP1, the above code routes it to COMP2 also.
	* Note that COMP2 is used for accessory detection and COMP1 is used
	* for button press detection.
	*/

	pr_debug("=== __headset_hw_init:"
		" Configured MIC route \r\n");

	/* Fast power up the Vref of ADC block */
	/*
	* NOTE:
	* This chal call was failing becuase internally this call
	* was configuring AUDIOH registers as well. We have commmented
	* configuring AUDIOH reigsrs in CHAL and it works OK
	*/
	aci_vref_config.mode = CHAL_ACI_VREF_FAST_ON;
	chal_aci_block_ctrl(p->aci_chal_hdl, CHAL_ACI_BLOCK_ACTION_VREF,
		CHAL_ACI_BLOCK_GENERIC, &aci_vref_config);

	pr_debug("=== __headset_hw_init:"
		" Configured Vref and ADC \r\n");

	/* Power down the MIC Bias and put in HIZ */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_MIC_POWERDOWN_HIZ_IMPEDANCE,
		CHAL_ACI_BLOCK_GENERIC, TRUE);

	pr_debug
		("=== __headset_hw_init: powered down MIC BIAS"
		" and put in High impedence state \r\n");

	/* Set the threshold value for button press */
	/*
	* Got the feedback from the system design team that the button press
	* threshold to programmed should be 0.12V as well.
	* With this only send/end button works, so made this 600 i.e 0.6V.
	* With the threshold level set to 0.6V all the 3 button press works
	* OK.Note that this would trigger continus COMP1 Interrupts if enabled.
	* But since we don't enable COMP1 until we identify a Headset its OK.
	*/
	if (p->low_voltage_mode)
		chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP1, 80);
	else
		chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP1, p->headset_pd->comp1_threshold);

	pr_debug
		("=== __headset_hw_init:"
		" Configured the threshold value for button press\r\n");

	/* Set the threshold value for accessory type detection */
	/* COMP2 used for accessory insertion/removal detection */
	chal_aci_block_ctrl(p->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
		CHAL_ACI_BLOCK_COMP2, p->headset_pd->comp2_threshold);

	pr_debug
		("=== __headset_hw_init:"
		" Configured the threshold value for type detection\r\n");

	return 0;
}

/*------------------------------------------------------------------------------
Function name   : headset_hw_init
Description     : Hardware initialization sequence
Return type     : int
------------------------------------------------------------------------------*/
static int headset_hw_init(struct mic_t *mic)
{
	int status = 0;

	/* Configure AUDIOH CCU for clock policy */
	/*
	* Remove the entire Audio CCU config policy settings and AUDIOH
	* initialization sequence once they are being done as a part of PMU and
	* audio driver settings
	*/
#ifdef CONFIG_HS_PERFORM_AUDIOH_SETTINGS
	writel(0xa5a501, KONA_HUB_CLK_VA);
	writel(0x6060606,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY_FREQ_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY0_MASK1_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY1_MASK1_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY2_MASK1_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY3_MASK1_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY0_MASK2_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY1_MASK2_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY2_MASK2_OFFSET);
	writel(0x7fffffff,
		KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY3_MASK2_OFFSET);
	writel(0x3, KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_POLICY_CTL_OFFSET);
	writel(0x1ff, KONA_HUB_CLK_VA + KHUB_CLK_MGR_REG_AUDIOH_CLKGATE_OFFSET);
	writel(0x100A00, KONA_HUB_CLK_VA + 0x104);
	writel(0x40000100, KONA_HUB_CLK_VA + 0x124);
#endif

	__headset_hw_init (mic);

	/*
	* If the platform uses GPIO for insetion/removal detection configure
	* the same. Otherwise use COMP2 for insertion and COMP2 INV for
	* removal detection
	*/
	if (mic->headset_pd->gpio_for_accessory_detection == 1) {
		unsigned int hs_gpio = irq_to_gpio(mic->gpio_irq);

		hs_info("gpio %d for accessory insertion detection\n", mic->gpio_irq);

		/* Request the gpio
		* Note that this is an optional call for setting
		* direction/debounce values.
		* But set debounce will throw out warning messages if we
		* call gpio_set_debounce without calling gpio_request.
		* Note that it just throws out Warning messages and proceeds
		* to auto request the same. We are adding this call here to
		* suppress the warning message.
		*/
		status = gpio_request(hs_gpio, "hs_detect");
		if (status < 0) {
			hs_err("%s: gpio request failed\n", __func__);
			return status;
		}

		if(mic->headset_pd->gpio_debounce > 0)
		{
			/* Set the GPIO debounce */
			status = gpio_set_debounce(hs_gpio, mic->headset_pd->gpio_debounce);
			if (status < 0) {
				hs_err("%s: gpio set debounce failed\n", __func__);
				return status;
			}
		}

		/* Set the GPIO direction input */
		status = gpio_direction_input(hs_gpio);
		if (status < 0) {
			hs_err("%s: gpio set direction input failed\n",
				__func__);
			return status;
		}

		hs_info("headset_hw_init: gpio config done\n");		
				
		__headset_hw_init_micbias_off(mic);
		
	} else {
		/*
		* This platform does not have GPIO for accessory
		* insertion/removal detection, use COMP2 for accessory
		* insertion detection.
		*/
		chal_aci_block_ctrl(mic->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_INTERRUPT_ENABLE,
			CHAL_ACI_BLOCK_COMP2);
	}

	return status;
}

int switch_bias_voltage(int mic_status)
{
	/* Force high bias for platforms not
	* supporting 0.45V mode
	*/
	if (!mic_dev->low_voltage_mode)
		mic_status = 1;

	switch (mic_status) {
	case 1:
		/*Mic will be used. Boost voltage */
		/* Set the threshold value for button press */
		hs_info("Setting Bias to 2.1V\r\n");
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
			CHAL_ACI_BLOCK_GENERIC,
			&aci_init_mic_bias);

		/* Power up Digital block */
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl, CHAL_ACI_BLOCK_ACTION_ENABLE,
			CHAL_ACI_BLOCK_DIGITAL);

		/* Power up the ADC */
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_DISABLE,
			CHAL_ACI_BLOCK_ADC);
		usleep_range(1000, 1200);
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_ENABLE,
			CHAL_ACI_BLOCK_ADC);
		usleep_range(1000, 1200);

		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_ADC_RANGE, CHAL_ACI_BLOCK_ADC,
			CHAL_ACI_BLOCK_ADC_HIGH_VOLTAGE);

		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_CONFIGURE_FILTER,
			CHAL_ACI_BLOCK_ADC, &aci_filter_adc_config);

		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
			CHAL_ACI_BLOCK_COMP1, mic_dev->headset_pd->comp1_threshold);
		button_adc_values =
			mic_dev->headset_pd->button_adc_values_high;
		break;
	case 0:
		hs_info("Setting Bias to 0.45V \r\n");

		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_MIC_BIAS,
			CHAL_ACI_BLOCK_GENERIC,
			&aci_init_mic_bias_low);
		__low_power_mode_config();
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_ADC_RANGE,
			CHAL_ACI_BLOCK_ADC,
			CHAL_ACI_BLOCK_ADC_LOW_VOLTAGE);
		chal_aci_block_ctrl(mic_dev->aci_chal_hdl,
			CHAL_ACI_BLOCK_ACTION_COMP_THRESHOLD,
			CHAL_ACI_BLOCK_COMP1,
			80);
		button_adc_values =
			mic_dev->headset_pd->button_adc_values_low;
		break;

	default:
		break;
	}
	return 0;
}

static void __low_power_mode_config(void)
{
	/*Use this for lowest
	* current consumption & button
	* functionality in 0.45V mode
	*
	* Offset 0x28: Disable weak sleep mode
	* Offset 0xC4: Impedance set to low
	*/
	writel(1, (void*)mic_dev->aci_base + 0xD4);
	writel(0, (void*)mic_dev->aci_base + 0xD8);
	writel(1, (void*)mic_dev->aci_base + 0xc4);
}

static int hs_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mic_t *mic = platform_get_drvdata(pdev);

	chal_aci_block_ctrl(mic->aci_chal_hdl,
		CHAL_ACI_BLOCK_ACTION_INTERRUPT_ACKNOWLEDGE,
		CHAL_ACI_BLOCK_COMP);

	return 0;
}

static int hs_resume(struct platform_device *pdev)
{
	return 0;
}

static int hs_remove(struct platform_device *pdev)
{
	struct mic_t *mic = platform_get_drvdata(pdev);

	wake_lock_destroy(&mic->accessory_wklock);
	device_destroy(mic->audio_class, pdev->dev.devt);
	class_destroy(mic->audio_class);

	free_irq(mic->comp1_irq, mic);
	if (mic->headset_pd->gpio_for_accessory_detection == 1)
		free_irq(mic->gpio_irq, mic);
	else
	{
		free_irq(mic->comp2_irq, mic);
		free_irq(mic->comp2_inv_irq, mic);
	}

	hs_unreginputdev(mic);
	hs_unregswitchdev(mic);

	kfree(mic);
	return 0;
}

static int __init hs_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *mem_resource;
	struct mic_t *mic;
	int irq_resource_num = 0;

	mic = kzalloc(sizeof(struct mic_t), GFP_KERNEL);
	if (!mic)
		return -ENOMEM;

	mic_dev = mic;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&mic->accessory_wklock, WAKE_LOCK_SUSPEND, "HS_det_wklock");
#endif
	if (pdev->dev.platform_data)
		mic->headset_pd = pdev->dev.platform_data;
	else {
		/* The driver depends on the platform data (board specific)
		* information to know two things
		* 1) The GPIO state that determines accessory insertion (HIGH or LOW)
		* 2) The resistor value put on the MIC_IN line.
		*
		* So if the platform data is not present, do not proceed.
		*/
		hs_err("Platform data not present, could not proceed\n");
		ret = EINVAL;
		goto err3;
	}

	/* Initialize the switch dev for headset */
	ret = hs_switchinit(mic);
	if (ret < 0)
		goto err2;

	/* Initialize a input device for the headset button */
	ret = hs_inputdev(mic);
	if (ret < 0) {
		hs_unregswitchdev(mic);
		goto err2;
	}

	mic->audio_class = class_create(THIS_MODULE, "audio");
	mic->headset_dev = device_create(mic->audio_class, NULL, pdev->dev.devt, NULL, "earjack");
	device_create_file(mic->headset_dev, &headset_Attrs[STATE]);
	device_create_file(mic->headset_dev, &headset_Attrs[KEY_STATE]);
	device_create_file(mic->headset_dev, &headset_Attrs[RECHECK]);

	/*
	* If on the given platform GPIO is used for accessory
	* insertion/removal detection get the GPIO IRQ to be
	* used for the same.
	*/
	if (mic->headset_pd->gpio_for_accessory_detection == 1) {
		/* Insertion detection irq */
		mic->gpio_irq = platform_get_irq(pdev, irq_resource_num);
		if (!mic->gpio_irq) {
			ret = -EINVAL;
			goto err1;
		}
		hs_info("HS GPIO irq %d\n", mic->gpio_irq);
	}
	irq_resource_num++;

	/*
	* Assume that mic bias is OFF, so that while initialization we can
	* put it in known state.
	*/
	mic->mic_bias_status = 0;
	mic->audio_ldo = NULL;

	mic->audioh_apb_clk = clk_get(&pdev->dev, "audioh_apb_clk");
	if (IS_ERR(mic->audioh_apb_clk))
		mic->audioh_apb_clk = NULL;

	if (mic->headset_pd->button_adc_values_low != NULL) {
		button_adc_values =
			mic->headset_pd->button_adc_values_low;
		mic->low_voltage_mode = true;
	} else if (mic->headset_pd->button_adc_values_high != NULL) {
		button_adc_values =
			mic->headset_pd->button_adc_values_high;
		mic->low_voltage_mode = false;
	} else {
		hs_info("%s(): WARNING Board specific button adc values are not passed"
			"using the default one, this may not work correctly for your"
			"platform\n", __func__);
		button_adc_values =
			button_adc_values_no_resistor;
	}

	if(mic->headset_pd->accessory_adc_values == NULL)
	{
		hs_info("%s(): ERROR Board specific accessory adc values are not passed\n", __func__);
		goto err1;
	}

	/* COMP2 irq */
	mic->comp2_irq = platform_get_irq(pdev, irq_resource_num);
	if (!mic->comp2_irq) {
		ret = -EINVAL;
		goto err1;
	}
	irq_resource_num++;
	hs_info("HSB press COMP2 irq %d\n", mic->comp2_irq);

	/* COMP2 INV irq */
	mic->comp2_inv_irq = platform_get_irq(pdev, irq_resource_num);
	if (!mic->comp2_inv_irq) {
		ret = -EINVAL;
		goto err1;
	}
	irq_resource_num++;
	hs_info("HSB release COMP2 irq inv %d\n", mic->comp2_inv_irq);

	/* Get COMP1 IRQ */
	mic->comp1_irq = platform_get_irq(pdev, irq_resource_num);
	if (!mic->comp1_irq) {
		ret = -EINVAL;
		goto err1;
	}
	hs_info("COMP1 irq %d\n", mic->comp1_irq);

	/* Get the base address for AUXMIC and ACI control registers */
	mem_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_resource) {
		ret = -ENXIO;
		goto err1;
	}
	mic->auxmic_base = (unsigned int)HW_IO_PHYS_TO_VIRT(mem_resource->start);

	hs_info("auxmic base is 0x%x\n", mic->auxmic_base);

	mem_resource = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!mem_resource) {
		ret = -ENXIO;
		goto err1;
	}
	mic->aci_base = (unsigned int)HW_IO_PHYS_TO_VIRT(mem_resource->start);

	hs_info("aci base is 0x%x\n", mic->aci_base);

	/* Perform CHAL initialization */
	mic->aci_chal_hdl = chal_aci_init((void*)mic->aci_base);

	/* Hardware initialization */
	ret = headset_hw_init(mic);
	if (ret < 0)
		goto err1;

	hs_info("Headset HW init done\n");

#ifdef DEBUG
	dump_hw_regs(mic);
#endif

	mic->hs_detecting = 0;
	mic->recheck_jack = 0;
	mic->hs_state = DISCONNECTED;
	mic->button_state = BUTTON_RELEASED;

	/* Store the mic structure data as private driver data for later use */
	platform_set_drvdata(pdev, mic);

	/*
	* Please note that all the HS accessory interrupts
	* should be requested with _NO_SUSPEND option because even if
	* the system goes to suspend we want this interrupts to be active
	*/
	/* Request for COMP1 IRQ */
	ret = request_threaded_irq(mic->comp1_irq, NULL ,comp1_isr,
		(IRQF_NO_SUSPEND | IRQF_ONESHOT), "COMP1", mic);
	if (ret < 0) {
		hs_err("COMP1 request_irq() failed for headset %d\n", ret);
		goto err1;
	}

	if (mic->headset_pd->gpio_for_accessory_detection == 1) {
		/* Request the IRQ for accessory insertion detection */
		ret = request_threaded_irq(mic->gpio_irq, NULL , gpio_isr,
			(IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND),
			"aci_accessory_detect", mic);		
		if (ret < 0) {
			hs_err("GPIO request_irq() failed for headset %d\n", ret);
			free_irq(mic->comp1_irq, mic);
			goto err1;
		}
		/*
		* Its important to understand why we schedule the
		* accessory detection work queue from here.
		*
		* From the schematics the GPIO status should be
		* 1 - nothing iserted
		* 0 - accessory inserted
		*
		* The pull up for the GPIO is connected to 1.8 V that
		* is source by the PMU.
		* But the PM chip's init happens after headset insertion, so
		* reading the GPIO value during init may not give us the
		* correct status (will read 0 always). Later after the init
		* when the GPIO gets the 1.8 V an interrupt would be
		* triggered for rising edge and the GPIO ISR would
		* schedule the work queue.
		* But if the accessory is kept connected assuming the PMU
		* to trigger the ISR is like taking a chance.
		* Also, if for some reason PMU init is moved before headset
		* driver init then the GPIO state would not change after
		* headset driver init and the GPIO interrupt may not
		* be triggered.
		* Its safe to schedule detection work here becuase
		* during bootup, irrespective of the GPIO interrupt we'll
		* detect the accessory type.
		* (Even if the interrupt occurs no harm done since
		* the work queue will be any way executed only once).
		*/

	} else {
		/* Request for COMP2 IRQ */
		ret =
			request_irq(mic->comp2_irq, comp2_isr, IRQF_NO_SUSPEND,
			"COMP2", mic);
		if (ret < 0) {
			hs_err("COMP2 request_irq() failed for headset %d\n", ret);
			free_irq(mic->comp1_irq, mic);
			goto err1;
		}	

		/*
		* If GPIO is not used for accessory insertion/removal
		* detection COMP2 INV IRQ is needed for accessory removal
		* detection.
		*/
		ret =
			request_irq(mic->comp2_inv_irq, comp2_inv_isr,
			IRQF_NO_SUSPEND, "COMP2 INV", mic);
		if (ret < 0) {
			hs_err("COMP2 INV request_irq() failed for headset %d\n", ret);
			free_irq(mic->comp1_irq, mic);
			free_irq(mic->comp2_irq, mic);
			goto err1;
		}
	}

	schedule_delayed_work(&(mic->accessory_detect_work),
		msecs_to_jiffies(mic->headset_pd->jack_insertion_settle_time));

#ifdef DEBUG
	dump_hw_regs(mic);
#endif

	hs_info("Headset probe done.\n");

	return ret;
err1:
	hs_unregswitchdev(mic);
	hs_unreginputdev(mic);
err2:
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&mic->accessory_wklock);
#endif
err3:
	kfree(mic);
	return ret;
}

/*
* Note that there is a __refdata added to the headset_driver platform driver
* structure. What is the meaning for it and why its required.
*
* The probe function:
* From the platform driver documentation its advisable to keep the probe
* function of a driver in the __init section if the device is NOT hot
* pluggable.Note that in headset case even though the headset is hot
* pluggable, the driver is not. That is a new node will not be created and
* the probe will not be called again. So it makes sense to keep the hs_probe
* in __init section so as to reduce the driver's run time foot print.
*
* The Warning message:
* But since the functions address (reference) is stored in a structure that
* will be available even after init (in case of remove, suspend etc) there
* is a Warning message from the compiler
*
* The __refdata keyword can be used to suppress this warning message. Tells the
* compiler not to throw out this warning. And in this scenario even though
* we store the function pointer from __init section to the platform driver
* structure that lives after __init, we wont be referring the probe function
* in the life time until headset_driver lives, so its OK to suppress.
*/
static struct platform_driver __refdata headset_driver = {
	.probe = hs_probe,
	.remove = hs_remove,
	.suspend = hs_suspend,
	.resume = hs_resume,
	.driver = {
		.name = "konaaciheadset",
		.owner = THIS_MODULE,
	},
};

#ifdef DEBUG

struct kobject *hs_kobj;

static ssize_t
	hs_regdump_func(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t n)
{
	dump_hw_regs(mic_dev);
	return n;
}

static ssize_t
	hs_regwrite_func(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t n)
{
	unsigned int reg_off;
	unsigned int val;

	if (sscanf(buf, "%x %x", &reg_off, &val) != 2) {
		hs_info("Usage: echo reg_offset value > /sys/hs_debug/hs_regwrite \r\n");
		return n;
	}
	hs_info("Writing 0x%x to Address 0x%x \r\n", val, mic_dev->aci_base + reg_off);
	writel(val, mic_dev->aci_base + reg_off);
	return n;
}

static DEVICE_ATTR(hs_regdump, 0666, NULL, hs_regdump_func);
static DEVICE_ATTR(hs_regwrite, 0666, NULL, hs_regwrite_func);

static struct attribute *hs_attrs[] = {
	&dev_attr_hs_regdump.attr,
	&dev_attr_hs_regwrite.attr,
	NULL,
};

static struct attribute_group hs_attr_group = {
	.attrs = hs_attrs,
};

static int __init hs_sysfs_init(void)
{
	hs_kobj = kobject_create_and_add("hs_debug", NULL);
	if (!hs_kobj)
		return -ENOMEM;
	return sysfs_create_group(hs_kobj, &hs_attr_group);
}

static void __exit hs_sysfs_exit(void)
{
	sysfs_remove_group(hs_kobj, &hs_attr_group);
}
#endif

/*------------------------------------------------------------------------------
Function name   : kona_hs_module_init
Description     : Initialize the driver
Return type     : int
------------------------------------------------------------------------------*/
int __init kona_aci_hs_module_init(void)
{
#ifdef DEBUG
	hs_sysfs_init();
#endif
	return platform_driver_register(&headset_driver);
}

/*------------------------------------------------------------------------------
Function name   : kona_hs_module_exit
Description     : clean up
Return type     : int
------------------------------------------------------------------------------*/
void __exit kona_aci_hs_module_exit(void)
{
#ifdef DEBUG
	hs_sysfs_exit();
#endif
	return platform_driver_unregister(&headset_driver);
}

module_init(kona_aci_hs_module_init);
module_exit(kona_aci_hs_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("Headset plug and button detection");
