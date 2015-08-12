
/***********************************************************************
* Driver for ISX012 (5MP Camera) from SAMSUNG SYSTEM LSI
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
***********************************************************************/

  

#include <uapi/linux/i2c.h>
#include <linux/delay.h>
#include <media/v4l2-ctrls.h>

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <uapi/linux/videodev2.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <linux/videodev2_brcm.h>
#include <mach/clock.h>
#include "camdrv_ss.h"
#include "camdrv_ss_isx012.h"           
#include <linux/module.h>
extern const struct v4l2_ctrl_ops camdrv_ss_ctrl_ops;
extern int flagforAE;
#define ISX012_NAME	"isx012"
#define SENSOR_ID 2
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x4b0000  // 4915200  = 2560*1920     //0x33F000     //3403776 //2216 * 1536
#define SENSOR_PREVIEW_WIDTH      1280
#define SENSOR_PREVIEW_HEIGHT     960
#define AF_OUTER_WINDOW_WIDTH   512 //320
#define AF_OUTER_WINDOW_HEIGHT  426 //266
#define AF_INNER_WINDOW_WIDTH   230 //143
#define AF_INNER_WINDOW_HEIGHT  230 //143
#define MAX_BUFFER			(4096)


#define ISX012_DEFAULT_PIX_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define ISX012_DEFAULT_MBUS_PIX_FMT    V4L2_MBUS_FMT_UYVY8_2X8 
#define FORMAT_FLAGS_COMPRESSED 0x3
#define DEFUALT_MCLK		26000000
#define ISX012_REGISTER_SIZE 0xFF
static bool camdrv_ss_isx012_check_flash_needed(struct v4l2_subdev *sd);

#ifdef CONFIG_FLASH_MIC2871
static int camdrv_ss_isx012_MIC2871_flash_control(struct v4l2_subdev *sd, int control_mode);
#else
#ifdef CONFIG_FLASH_KTD2692
static int camdrv_ss_isx012_KTD2692_flash_control(struct v4l2_subdev *sd, int control_mode);
#else
#ifdef CONFIG_FLASH_DS8515
static int camdrv_ss_isx012_DS8515_flash_control(struct v4l2_subdev *sd, int control_mode);
#else
static int camdrv_ss_isx012_AAT_flash_control(struct v4l2_subdev *sd, int control_mode);
#endif
#endif
#endif


//static int camdrv_ss_isx012_set_ae_stable_status(struct v4l2_subdev *sd, unsigned short value);//Nikhil
//static int camdrv_ss_isx012_get_ae_stable_status_value(struct v4l2_subdev *sd);//Nikihl
int camdrv_ss_isx012_set_preflash_check(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl);
int camdrv_ss_isx012_set_preflash_sequence(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl);
#define ISX012_DELAY_DURATION 0xFFFF
static bool first_af_status = false;
static bool set_preflash = false;
unsigned short ae_auto;
unsigned short ae_now;
short ersc_auto;
short ersc_now;
short aescl;
bool  touch_af = false;
bool  capture_flash = false;



unsigned int iso_value = ISO_AUTO;
unsigned int ev_value = EV_DEFAULT;


#ifdef CONFIG_FLASH_MIC2871
#define camdrv_ss_isx012_flash_control(a,b)  camdrv_ss_isx012_MIC2871_flash_control(a,b)
#else
#ifdef CONFIG_FLASH_DS8515
#define camdrv_ss_isx012_flash_control(a,b)  camdrv_ss_isx012_DS8515_flash_control(a,b)
#else
#ifdef CONFIG_FLASH_KTD2692
#define camdrv_ss_isx012_flash_control(a,b)  camdrv_ss_isx012_KTD2692_flash_control(a,b)
#else
#define camdrv_ss_isx012_flash_control(a,b)  camdrv_ss_isx012_AAT_flash_control(a,b)
#endif
#endif
#endif
static int camdrv_ss_isx012_check_wait_for_mode_transition(struct v4l2_subdev *sd, int imode);

/***********************************************************/
/* H/W configuration - Start                               */
/***********************************************************/

#define EXIF_SOFTWARE		""
#define EXIF_MAKE		"SAMSUNG"

#define SENSOR_0_CLK			"dig_ch0_clk"
#define SENSOR_0_CLK_FREQ		(26000000)

#define CSI0_LP_FREQ			(100000000)
#define CSI1_LP_FREQ			(100000000)
static struct regulator *VCAM_A_2_8_V;
static struct regulator *VCAM1_CORE_1_8_V;
static struct regulator *VCAM_IO_1_8_V;
static struct regulator *VCAM_CORE_1_2_V;
static struct regulator *VCAM_AF_2_8V;

/* individual configuration */
#if defined(CONFIG_MACH_HAWAII_SS_VIVALTONFC3G_REV00)\
	|| defined(CONFIG_MACH_HAWAII_SS_VIVALTODS5M_REV00)
#define VCAM_IO_1_8V_REGULATOR		"lvldo1"
#define VCAM1_CORE_1_8V_REGULATOR	"lvldo2"
#define VCAM_CORE_1_2V_REGULATOR	"vsrldo"
#define VCAM_A_2_8V_REGULATOR		"mmcldo1"
#define VCAM_AF_2_8V_REGULATOR		"mmcldo2"
#define VCAM1_CORE_1_8V_REGULATOR_NEEDED
#define VCAM_IO_1_8V_REGULATOR_uV	1786000
#define VCAM1_CORE_1_8V_REGULATOR_uV	1786000
#define VCAM_CORE_1_2V_REGULATOR_uV	1200000
#define VCAM_A_2_8V_REGULATOR_uV	2800000
#define VCAM_AF_2_8V_REGULATOR_uV	2800000
#define CAM0_RESET			111
#define CAM0_STNBY			2
#define CAM1_RESET			5
#define CAM1_STNBY			4
#define GPIO_CAM_FLASH_SET		12
#define GPIO_CAM_FLASH_EN		26
#define EXIF_MODEL			"GT-B7272"

#elif defined(CONFIG_MACH_HAWAII_SS_LOGAN_REV02) \
	|| defined(CONFIG_MACH_HAWAII_SS_LOGANDS_REV00) \
	|| defined(CONFIG_MACH_HAWAII_SS_LOGANDS_REV01) \
	|| defined(CONFIG_MACH_HAWAII_SS_CS02_REV00) \
	|| defined(CONFIG_MACH_JAVA_SS_EVAL) \
	|| defined(CONFIG_MACH_JAVA_SS_VICTOR3G) \
	|| defined(CONFIG_MACH_HAWAII_SS_HEAT3G_REV00) \
	|| defined(CONFIG_MACH_HAWAII_SS_HEAT3G_REV01) 
#define VCAM_A_2_8V_REGULATOR		"mmcldo1"
#define VCAM_IO_1_8V_REGULATOR		"lvldo1"
#define VCAM_CORE_1_2V_REGULATOR	"vsrldo"
#define VCAM_AF_2_8V_REGULATOR		"mmcldo2"
#define VCAM_A_2_8V_REGULATOR_uV	2800000
#define VCAM_IO_1_8V_REGULATOR_uV	1786000
#define VCAM_CORE_1_2V_REGULATOR_uV	1200000
#define VCAM_AF_2_8V_REGULATOR_uV	2800000
#define CAM0_RESET			111
#define CAM0_STNBY			2
#define GPIO_CAM_FLASH_SET		12
#define GPIO_CAM_FLASH_EN		26
#define EXIF_MODEL			"GT-B7272"

#elif defined(CONFIG_MACH_HAWAII_SS_LOGAN_REV01) \
	|| defined(CONFIG_MACH_HAWAII_SS_GOLDENVEN_REV01)\
	|| defined(CONFIG_MACH_HAWAII_SS_CODINAN)

#define VCAM_A_2_8V_REGULATOR		"mmcldo1"
#define VCAM_IO_1_8V_REGULATOR		"lvldo1"
#define VCAM_AF_2_8V_REGULATOR		"mmcldo2"
#define VCAM_CORE_1_2V_REGULATOR	"vsrldo"
#define VCAM_A_2_8V_REGULATOR_uV	2800000
#define VCAM_IO_1_8V_REGULATOR_uV	1800000
#define VCAM_CORE_1_2V_REGULATOR_uV	1200000
#define VCAM_AF_2_8V_REGULATOR_uV	2800000
#define CAM0_RESET			111
#define CAM0_STNBY			2
#define GPIO_CAM_FLASH_SET		34
#define GPIO_CAM_FLASH_EN		11
#define EXIF_MODEL			"GT-B7272"

#elif defined(CONFIG_MACH_HAWAII_SS_LOGAN_REV00)

#define VCAM_A_2_8V_REGULATOR		"mmcldo1"
#define VCAM_IO_1_8V_REGULATOR		"tcxldo1"
#define VCAM_AF_2_8V_REGULATOR		"mmcldo2"
#define VCAM_CORE_1_2V_REGULATOR	"vsrldo"
#define VCAM_A_2_8V_REGULATOR_uV	2800000
#define VCAM_IO_1_8V_REGULATOR_uV	1800000
#define VCAM_CORE_1_2V_REGULATOR_uV	1200000
#define VCAM_AF_2_8V_REGULATOR_uV	2800000
#define CAM0_RESET			111
#define CAM0_STNBY			2
#define GPIO_CAM_FLASH_SET		34
#define GPIO_CAM_FLASH_EN		11
#define EXIF_MODEL			"GT-B7272"

#else /* NEED TO REDEFINE FOR NEW VARIANT */

#define VCAM_A_2_8V_REGULATOR		"mmcldo1"
#define VCAM_IO_1_8V_REGULATOR		"lvldo1"
#define VCAM_AF_2_8V_REGULATOR		"mmcldo2"
#define VCAM_CORE_1_2V_REGULATOR	"vsrldo"
#define VCAM_A_2_8V_REGULATOR_uV	2800000
#define VCAM_IO_1_8V_REGULATOR_uV	1800000
#define VCAM_CORE_1_2V_REGULATOR_uV	1200000
#define VCAM_AF_2_8V_REGULATOR_uV	2800000
#define CAM0_RESET			111
#define CAM0_STNBY			2
#define GPIO_CAM_FLASH_SET		34
#define GPIO_CAM_FLASH_EN		11
#define EXIF_MODEL			"GT-B7272"

#endif

/***********************************************************/
/* H/W configuration - End                                 */
/***********************************************************/


#define AE_STABLE          1 //nikhil
#define AE_UNSTABLE    0  //nikhil
extern int flash_check; 
extern int torch_check;
static DEFINE_MUTEX(af_cancel_op);
//extern inline struct camdrv_ss_state *to_state(struct v4l2_subdev *sd);


extern  int camdrv_ss_i2c_set_config_register(struct i2c_client *client, 
                                         regs_t reg_buffer[], 
          				                 int num_of_regs, 
          				                 char *name);

extern int camdrv_ss_set_preview_size(struct v4l2_subdev *sd);
extern int camdrv_ss_set_capture_size(struct v4l2_subdev *sd);
extern int camdrv_ss_set_dataline_onoff(struct v4l2_subdev *sd, int onoff);
extern struct camdrv_ss_state *to_state(struct v4l2_subdev *sd);
int wb_auto=1,iso_auto=1;
//#define __JPEG_CAPTURE__ 1        

static const struct camdrv_ss_framesize isx012_supported_preview_framesize_list[] = {
/*	{ PREVIEW_SIZE_QCIF,	176,  144 },
	{ PREVIEW_SIZE_QVGA,	320,  240 },
	{ PREVIEW_SIZE_CIF,	352,  288 },*/
	{ PREVIEW_SIZE_VGA,	640,  480 },
	//{ PREVIEW_SIZE_D1,	720,  480 },
	{ PREVIEW_SIZE_XGA,	1024, 768 },
	{ PREVIEW_SIZE_1MP,	1280, 960 },
#if 0
	{ PREVIEW_SIZE_1MP,	1280, 1024 },
	{ PREVIEW_SIZE_W1MP,	1600,  960 },
	{ PREVIEW_SIZE_2MP,	1600, 1200 },
	{ PREVIEW_SIZE_3MP,	2048, 1536 },
#endif
	{ PREVIEW_SIZE_5MP,	2560, 1920 },
};


static const struct camdrv_ss_framesize  isx012_supported_capture_framesize_list[] = {
	{ CAPTURE_SIZE_VGA,		640,  480 },
//	{ CAPTURE_SIZE_1MP,		1280, 960 },
//	{ CAPTURE_SIZE_2MP,		1600, 1200 },
	{ CAPTURE_SIZE_3MP,		2048, 1536 },
	{ CAPTURE_SIZE_5MP,		2560, 1920 },
};
const static struct v4l2_fmtdesc isx012_fmts[] = 
{
    {
        .index          = 0,
        .type           = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .flags          = 0,
        .description    = "UYVY",
        .pixelformat    = V4L2_MBUS_FMT_UYVY8_2X8,
    },

#ifdef __JPEG_CAPTURE__
	{
		.index		= 1,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_COMPRESSED,
		.description	= "JPEG + Postview",
		.pixelformat	= V4L2_MBUS_FMT_JPEG_1X8,
	},
#endif	
};

static const struct v4l2_ctrl_config isx012_controls[] = {
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_SENSOR_MOUNT_ORIENTATION,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Sensor mount info",
		.min	= 0,
		.max	= 1,
		.step	= 1,
		.def	= 0,
		.flags 	= V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_INITIALIZE,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Initialize",
		.min	= 0,
		.max	= 2,
		.step		= 1,
		.def	= 2,
		.flags 	= 0,

	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAM_SET_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Mode",
		.min	= INIT_MODE,
		.max	= PICTURE_MODE,
		.step		= 1,
		.def	= INIT_MODE,
		.flags 	= 0,

	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_GET_ESD_SHOCK_STATUS,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Get ESD status",
		.min	= 0,
		.max	= 1,
		.step	= 1,
		.def	= 0,
		.flags 	= V4L2_CTRL_FLAG_VOLATILE,
	},
				
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_ESD_DETECTED_RESTART_CAMERA,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "ESD Restart",
		.min	= 0,
		.max	= 1,
		.step	= 1,
		.def	= 0,
		.flags 	= 0,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_EXIF_SENSOR_INFO,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Exif sensor info",
		.min	= 0,
		.max	= 0x7FFFFFFF,
		.step	= 1,
		.def	= 0,
		.flags 	= 0,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_READ_MODE_CHANGE_REG,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Read mode change",
		.min	= 0,
		.max	= 1,
		.step	= 1,
		.def	= 0,
		.flags 	= V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_AUTO_FOCUS_RESULT,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "AF result",
		.min	= 0,
		.max	= 1,
		.step	= 1,
		.def	= 0,
		.flags 	= V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_AE_STABLE_RESULT,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "AE result",
		.min	= AE_UNSTABLE,
		.max	= AE_STABLE,
		.step	= 1,
		.def	= AE_UNSTABLE,
		.flags 	= V4L2_CTRL_FLAG_VOLATILE,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_FLASH_CONTROL,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Flash control",
		.min	= FLASH_CONTROL_BASE,
		.max	= FLASH_CONTROL_MAX_LEVEL,
		.step	= 1,
		.def	= FLASH_CONTROL_OFF,
		.flags 	= 0,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_DEFAULT_FOCUS_POSITION,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Default Focus Position",
		.min	= FOCUS_MODE_AUTO,
		.max	= (1 << FOCUS_MODE_AUTO | 1 << FOCUS_MODE_MACRO | 1 << FOCUS_MODE_INFINITY),
		.step	= 1,
		.def	= FOCUS_MODE_AUTO,
		.flags 	= 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_SET_AF_PREFLASH,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "AF Preflash",
		.min	= PREFLASH_OFF,
		.max	= PREFLASH_ON,
		.step	= 1,
		.def	= PREFLASH_OFF,
		.flags 	= 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_FLASH_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Flash",
		.min	= FLASH_MODE_OFF,
		.max	= (1 << FLASH_MODE_OFF | 1 << FLASH_MODE_AUTO | 1 << FLASH_MODE_ON
					| 1 << FLASH_MODE_TORCH_ON),
		.step		= 1,
		.def	= FLASH_MODE_OFF,
		.flags 	= 0,

	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_BRIGHTNESS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Brightness",
		.min	= EV_MINUS_4,
		.max	= EV_PLUS_4,
		.step		= 1,
		.def	= EV_DEFAULT,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_WHITE_BALANCE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Whilte Balance",
		.min	= WHITE_BALANCE_AUTO ,
		.max	= (1 << WHITE_BALANCE_AUTO | 1 << WHITE_BALANCE_DAYLIGHT/*WHITE_BALANCE_SUNNY*/ | 1 << WHITE_BALANCE_CLOUDY
			           | 1 << WHITE_BALANCE_INCANDESCENT/*WHITE_BALANCE_TUNGSTEN*/ | 1 << WHITE_BALANCE_FLUORESCENT ),
		.step		= 1,
		.def	= WHITE_BALANCE_AUTO,
		.flags = 0,
	},

/* remove zoom setting here to use AP zoom feture
	{
		.id			= V4L2_CID_CAMERA_ZOOM,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Zoom",
		.min	= ZOOM_LEVEL_0,
		.max	= ZOOM_LEVEL_8,
		.step		= 1,
		.def	= ZOOM_LEVEL_0,
	},
*/

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_EFFECT,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Color Effects",
		.min	= IMAGE_EFFECT_NONE,
		.max	= (1 << IMAGE_EFFECT_NONE | 1 << IMAGE_EFFECT_MONO/*IMAGE_EFFECT_BNW*/
						| 1 << IMAGE_EFFECT_SEPIA | 1 << IMAGE_EFFECT_NEGATIVE), /* this should be replace by querymenu */
		.step		= 1,
		.def	= IMAGE_EFFECT_NONE,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_ISO,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "ISO",
		.min	= ISO_AUTO ,
		.max	= (1 << ISO_AUTO | 1 << ISO_50 | 1 << ISO_100 | 1 << ISO_200 | 1 << ISO_400 | 1 << ISO_800 | 1 << ISO_1200 
						| 1 << ISO_1600 | 1 << ISO_2400 | 1 << ISO_3200 | 1 << ISO_SPORTS | 1 << ISO_NIGHT | 1 << ISO_MOVIE),
		.step		= 1,
		.def	= ISO_AUTO,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_METERING,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Metering",
		.min	= METERING_MATRIX,
		.max	= (1 << METERING_MATRIX | 1 << METERING_CENTER | 1 << METERING_SPOT),
		.step		= 1,
		.def	= METERING_CENTER,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_CONTRAST,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Contrast",
		.min	= CONTRAST_MINUS_2,
		.max	= CONTRAST_PLUS_2,
		.step		= 1,
		.def	= CONTRAST_DEFAULT,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_SATURATION,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Saturation",
		.min	= SATURATION_MINUS_2,
		.max	= SATURATION_PLUS_2,
		.step		= 1,
		.def	= SATURATION_DEFAULT,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_SHARPNESS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Sharpness",
		.min	= SHARPNESS_MINUS_2,
		.max	= SHARPNESS_PLUS_2,
		.step		= 1,
		.def	= SHARPNESS_DEFAULT,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_WDR,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "WDR",
		.min	= WDR_OFF,
		.max	= WDR_ON,
		.step		= 1,
		.def	= WDR_OFF,
		.flags = 0,
	},
/*
	{
		.id			= V4L2_CID_CAMERA_FACE_DETECTION,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Face Detection",
		.min	= FACE_DETECTION_OFF,
		.max	= FACE_DETECTION_ON,
		.step		= 1,
		.def	= FACE_DETECTION_OFF,
	},
*/
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_FOCUS_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Focus",
		.min	= FOCUS_MODE_AUTO,
		.max	= (1 << FOCUS_MODE_AUTO | 1 << FOCUS_MODE_MACRO
						| 1 << FOCUS_MODE_INFINITY), /* querymenu ?*/
		.step		= 1,
		.def	= FOCUS_MODE_AUTO,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAM_JPEG_QUALITY,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "JPEG Quality",
		.min	= 0,
		.max	= 100,
		.step		= 1,
		.def	= 100,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_SCENE_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Scene Mode",
		.min	= SCENE_MODE_NONE,
		.max	= (1 << SCENE_MODE_NONE | 1 << SCENE_MODE_PORTRAIT |
						1 << SCENE_MODE_NIGHTSHOT | 1 << SCENE_MODE_LANDSCAPE
						| 1 << SCENE_MODE_SPORTS | 1 << SCENE_MODE_PARTY_INDOOR |
						1 << SCENE_MODE_BEACH_SNOW | 1 << SCENE_MODE_SUNSET |
						1 << SCENE_MODE_FIREWORKS | 1 << SCENE_MODE_CANDLE_LIGHT | /*querymenu?*/
						1 << SCENE_MODE_BACK_LIGHT | 1<< SCENE_MODE_DUSK_DAWN |
						1 << SCENE_MODE_FALL_COLOR | 1<< SCENE_MODE_TEXT),
		.step		= 1,
		.def	= SCENE_MODE_NONE,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_SET_AUTO_FOCUS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Set AutoFocus",
		.min	= AUTO_FOCUS_OFF,
		.max	= AUTO_FOCUS_2ND_CANCEL,
		.step		= 1,
		.def	= AUTO_FOCUS_OFF,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id		= V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "AEAWUNLOCK",
		.min	= AE_UNLOCK_AWB_UNLOCK,
		.max	= (1 << AE_UNLOCK_AWB_UNLOCK | 1 <<AE_LOCK_AWB_LOCK ),
		.step		= 1,
		.def	= AE_UNLOCK_AWB_UNLOCK,
		.flags = 0,

	},
		
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id 		= V4L2_CID_CAMERA_TOUCH_AF_AREA,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Touchfocus areas",
		.min	= 0,
		.max	= 0x7FFFFFFF,
		.step		= 1,
		.def	= 1,
		.flags = 0,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_FRAME_RATE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Framerate control",
		.min	= FRAME_RATE_AUTO,
		.max	= (1 << FRAME_RATE_AUTO /*| 1 << FRAME_RATE_5 | 1 << FRAME_RATE_7 */|1 << FRAME_RATE_10  | 1 << FRAME_RATE_15
						/*| 1 << FRAME_RATE_20*/ | 1 << FRAME_RATE_25 | 1 << FRAME_RATE_30),		
		.step		= 1,
		.def	= FRAME_RATE_AUTO,
		.flags = 0,
	},


	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAM_PREVIEW_ONOFF,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Preview control",
		.min	= 0,
		.max	= 1,
		.step		= 1,
		.def	= 0,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_CHECK_DATALINE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Check Dataline",
		.min	= CHK_DATALINE_OFF,
		.max	= CHK_DATALINE_ON,
		.step		= 1,
		.def	= CHK_DATALINE_OFF,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_CHECK_DATALINE_STOP,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Check Dataline Stop",
		.min	= 0,
		.max	= 1,
		.step		= 1,
		.def	= 0,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_RETURN_FOCUS,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Return Focus",
		.min	= 0,
		.max	= 1,
		.step		= 1,
		.def	= 0,
		.flags = 0,
	},

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_ANTI_BANDING,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Anti Banding",
		.min	      = ANTI_BANDING_AUTO,
		.max	=  (1 << ANTI_BANDING_AUTO | 1 << ANTI_BANDING_50HZ | 1 << ANTI_BANDING_60HZ
                                    | 1 << ANTI_BANDING_OFF),
		.step		= 1,
		.def	= ANTI_BANDING_AUTO,
		.flags = 0,
	},
	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_VT_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Vtmode",
		.min	      = CAM_VT_MODE_3G,
		.max	= CAM_VT_MODE_VOIP,
		.step		= 1,
		.def	= CAM_VT_MODE_3G,
		.flags = 0,
	},	

	{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_SENSOR_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Cam mode",
		.min	      = SENSOR_CAMERA,
		.max	= SENSOR_MOVIE,
		.step		= 1,
		.def	= SENSOR_CAMERA,
		.flags = 0,
	},	
			{
		.ops    = &camdrv_ss_ctrl_ops,
		.id			= V4L2_CID_CAMERA_SAMSUNG,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Samsung Camera",
		.min	= 0,
		.max	= 5,
		.step		= 1,
		.def	= 1,
		.flags = 0,
	},
		
};


static int camdrv_ss_isx012_calibrateSensor(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	int err = 0;

	unsigned char	rgucOTPDatas[15] = {0, };
	unsigned short  readValue = 0, readAddr = 0, writeAddr = 0, writeValue = 0;
	unsigned short  startReadAddr, endReadAddr;
	unsigned short  useValue = 0, usTemp1, usTemp2;
	unsigned char	checkValue = 0;
	int  iIndex = 0;

	//camdrv_ss_isx012_check_wait_for_mode_transition(sd, 1);
	
	// Read OTP1(0x004F)
	CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);
	readAddr = 0x004F;
	err = camdrv_ss_i2c_read_2_bytes_1(client, readAddr, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr);
		return err;
	}
	checkValue = readValue & 0xFF;
	err = 0;
	CAM_ERROR_PRINTK(" : Read OTP1(0x%x) = 0x%x\n", readAddr, checkValue );
	CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

	// take OTP_V[bit4]
	useValue = ((checkValue & 0x10) >> 4);
	CAM_ERROR_PRINTK(" : useValue = 0x%x\n", useValue);

	if( useValue == 1 )
	{
		startReadAddr = 0x004F;
		endReadAddr = 0x005D;
	}
	else
	{
		// Read OTP0(0x0040)
		readAddr = 0x0040;
		readValue=0;
		err = camdrv_ss_i2c_read_2_bytes_1(client, readAddr, &readValue);
		if( err < 0 )
		{
			CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr);
			return err;
		}
		checkValue = readValue & 0xFF;
		err = 0;
		CAM_ERROR_PRINTK(" : Read OTP0(0x%x) = 0x%x\n", readAddr, checkValue );

		// take OTP_V[bit4]
		useValue = ((checkValue & 0x10) >> 4);
		CAM_ERROR_PRINTK(" : useValue = 0x%x\n", useValue);
		if( useValue == 1 )
		{
			startReadAddr = 0x0040;
			endReadAddr = 0x004E;
		}
		else
		{
			startReadAddr = 0x0;
		}
	}
	CAM_ERROR_PRINTK( "%s : line=%d startReadAddr=0x%x\n",__func__,__LINE__,startReadAddr);

	// READ OTP MAP
	if( startReadAddr == 0 )
	{
		err = camdrv_ss_i2c_set_config_register(
					client, (regs_t *) isx012_reg_main_callibration_shading_table_nocal,
					ARRAY_SIZE(isx012_reg_main_callibration_shading_table_nocal),
					"reg_main_callibration_shading_table_nocal");
		if (err < 0){
			CAM_ERROR_PRINTK("[%s : %d] ERROR! Couldn't Set isx012_Pll_Setting_4\n", __func__, __LINE__);
		return -EIO;
		};
	}
	else
	{
		iIndex = 0;
		for( readAddr = startReadAddr; readAddr <= endReadAddr; readAddr++ )
		{
			err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr, (unsigned short*)&rgucOTPDatas[iIndex]);
			if( err < 0 )
			{
				CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr);
				return err;
			}
			CAM_ERROR_PRINTK(" : Read rgucOTPDatas[%d] = 0x%x\n", iIndex, rgucOTPDatas[iIndex] );
			iIndex++;
	}
		CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

		// Read Shading Table [113:110]
		usTemp1 = 0; usTemp2 = 0; useValue = 0;
		usTemp1 = (rgucOTPDatas[14] & 0x03); // [113:112]
		usTemp2 = ((rgucOTPDatas[13] & 0xC0) >> 6); // [111:110]
		useValue = ((usTemp1 << 2) | usTemp2);
		CAM_ERROR_PRINTK(" :Read Shading Table[113:110], usValue=0x%x, usTemp1=0x%x, usTemp2=0x%x\n",
											useValue, usTemp1, usTemp2);

		// Write ShadingTable
		if( useValue == 0 )
		{
			err = camdrv_ss_i2c_set_config_register(
					client,(regs_t *) isx012_reg_main_callibration_shading_table_0,
					ARRAY_SIZE(isx012_reg_main_callibration_shading_table_0),
					"reg_main_callibration_shading_table_0");
			if (err < 0){
				CAM_ERROR_PRINTK("[%s : %d] ERROR! Couldn't Set reg_main_callibration_shading_table_0\n", __func__, __LINE__);
				return -EIO;	
			}
		}
		else if( useValue == 1 )
		{
			err = camdrv_ss_i2c_set_config_register(
					client,(regs_t *) isx012_reg_main_callibration_shading_table_1,
					ARRAY_SIZE(isx012_reg_main_callibration_shading_table_1),
					"reg_main_callibration_shading_table_1");
			if (err < 0){
				CAM_ERROR_PRINTK("[%s : %d] ERROR! Couldn't Set reg_main_callibration_shading_table_1\n", __func__, __LINE__);
				return -EIO;	
			}
		}
		else if( useValue == 2 )
    {
			err = camdrv_ss_i2c_set_config_register(
					client,(regs_t *) isx012_reg_main_callibration_shading_table_2,
					ARRAY_SIZE(isx012_reg_main_callibration_shading_table_2),
					"reg_main_callibration_shading_table_2");
			if (err < 0){
				CAM_ERROR_PRINTK("[%s : %d] ERROR! Couldn't Set reg_main_callibration_shading_table_2\n", __func__, __LINE__);
				return -EIO;	
			}
    }
    else
    {
			CAM_ERROR_PRINTK(" invaild useValue = %d", useValue );
			return -1;
		}
	CAM_ERROR_PRINTK( "\n%s : line=%d\n",__func__,__LINE__);

		// Write NorR(0x6804) = OTP Data[53:40]
		usTemp1 = 0; usTemp2 = 0; useValue = 0;
		usTemp1 = (rgucOTPDatas[6] & 0x3F); // [53:48]
		usTemp2 = (rgucOTPDatas[5] & 0xFF); // [47:40]
		useValue = ((usTemp1 << 8) | usTemp2);
		CAM_ERROR_PRINTK(" :OTP Data[53:40], usValue=0x%x, usTemp1=0x%x, usTemp2=0x%x", useValue, usTemp1, usTemp2);

		writeAddr = 0x6804;
		writeValue = (useValue & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}
		CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

		writeAddr = 0x6805;
		writeValue = ((useValue >> 8) & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}

		// Write NorB(0x6806) = OTP Data[69:56]
		usTemp1 = 0; usTemp2 = 0; useValue = 0;
		usTemp1 = (rgucOTPDatas[8] & 0x3F); // [69:64]
		usTemp2 = (rgucOTPDatas[7] & 0xFF); // [63:56]
		useValue = ((usTemp1 << 8) | usTemp2);
		CAM_ERROR_PRINTK(" :OTP Data[69:56], useValue=0x%x, usTemp1=0x%x, usTemp2=0x%x", useValue, usTemp1, usTemp2);

		writeAddr = 0x6806;
		writeValue = (useValue & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}
		CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

		writeAddr = 0x6807;
		writeValue = ((useValue >> 8) & 0xFF);
	    err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}

		// Write PreR(0x6808) = OTP Data[99:90]
		usTemp1 = 0; usTemp2 = 0; useValue = 0;
		usTemp1 = (rgucOTPDatas[12] & 0x0F); // [99:96]
		usTemp2 = ((rgucOTPDatas[11] & 0xFC) >> 2); // [95:90]
		useValue = ((usTemp1 << 6) | usTemp2);
		CAM_ERROR_PRINTK(" :OTP Data[99:90], useValue=0x%x, usTemp1=0x%x, usTemp2=0x%x", useValue, usTemp1, usTemp2);
		CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

		writeAddr = 0x6808;
		writeValue = (useValue & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}

		writeAddr = 0x6809;
		writeValue = ((useValue >> 8) & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}
		CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

		// Write PreB(0x680A) = OTP Data[109:100]
		usTemp1 = 0; usTemp2 = 0; useValue = 0;
		usTemp1 = (rgucOTPDatas[13] & 0x3F); // [109:104]
		usTemp2 = ((rgucOTPDatas[12] & 0xF0) >> 4); // [103:100]
		useValue = ((usTemp1 << 4) | usTemp2);
		CAM_ERROR_PRINTK(" :OTP Data[53:40], useValue=0x%x, usTemp1=0x%x, usTemp2=0x%x", useValue, usTemp1, usTemp2);

		writeAddr = 0x680A;
		writeValue = (useValue & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}
		CAM_ERROR_PRINTK( "%s : line=%d\n",__func__,__LINE__);

		writeAddr = 0x680B;
		writeValue = ((useValue >> 8) & 0xFF);
		err = camdrv_ss_i2c_write_3_bytes(client, writeAddr, writeValue); 
		if(err < 0)
		{
			CAM_ERROR_PRINTK("[%s : %d] i2c write fail, writeAddr=0x%x", __func__, __LINE__, writeAddr);
			return -EIO;
		}
    }
	CAM_ERROR_PRINTK( "%s : line=%d err=%d\n",__func__,__LINE__,err);

	return err;
}

int camdrv_ss_isx012_calculate_aegain_offset(struct i2c_client *client, unsigned short ae_auto, unsigned short ae_now, short ersc_auto, short ersc_now)
{
	int aediff, aeoffset;
//	int err = 0;

	aediff = (ae_now + ersc_now) - (ae_auto + ersc_auto);

	if (aediff < 0)	{
		aediff = 0;
	}

	if ( ersc_now < 0 ) {
		if ( aediff >= AE_MAXDIFF )	{
			aeoffset = -AE_OFSETVAL - ersc_now;
		}
		else	{

			CAM_INFO_PRINTK("(%s) ISX012 aeoffset_table position = %d\n", __func__,aediff/10);
		    CAM_INFO_PRINTK("(%s) ISX012 aeoffset_table table value  = %d\n", __func__,-aeoffset_table[aediff/10]);
			aeoffset = -aeoffset_table[aediff/10] - ersc_now;
		}
	}
	else	{
		if ( aediff >= AE_MAXDIFF )	{
			aeoffset = -AE_OFSETVAL;
		}
		else	{
			aeoffset = -aeoffset_table[aediff/10];
		}
	}

	// Set AE Gain offset

	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x0186, aeoffset);
	
	CAM_INFO_PRINTK("(%s) ISX012 ae_auto  = %d\n", __func__,ae_auto);
	CAM_INFO_PRINTK("(%s) ISX012 ae_now  = %d\n", __func__,ae_now);
	CAM_INFO_PRINTK("(%s) ISX012 ersc_auto  = %d\n", __func__,ersc_auto);
	CAM_INFO_PRINTK("(%s) ISX012 ersc_now  = %d\n", __func__,ersc_now);
	CAM_INFO_PRINTK("(%s) ISX012 aediff  = %d\n", __func__,aediff);
	CAM_INFO_PRINTK("(%s) ISX012 aeoffset  = %d\n", __func__,aeoffset);

	return 0;
}

int camdrv_ss_isx012_get_manoutgain(struct i2c_client *client, short aescl)
{
	short value;
	//int err = 0;
//	struct i2c_reg_value ucWriteReg;

	value = aescl - AE_SCL_SUBRACT_VALUE;

	// Set AE SCL
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x5E02, value);

	CAM_INFO_PRINTK("(%s) ISX012 aescl = %d\n", __func__,aescl);
	CAM_INFO_PRINTK("(%s) ISX012 value = aescl - AE_SCL_SUBRACT_VALUE = %d\n", __func__,aescl - AE_SCL_SUBRACT_VALUE);
	CAM_INFO_PRINTK("(%s) ISX012 value = %d\n", __func__,aescl);

	return 0;
}

int camdrv_ss_isx012_get_previewaescale(struct i2c_client *client, unsigned short *ae_auto, short *ersc_auto)
{
    unsigned short  readAddr1 = 0x01CE; // USER_AESCL_AUTO
    unsigned short  readAddr2 = 0x01CA; // ERRSCL_AUTO
    unsigned short  readValue = 0;
    int err = 0;
    
    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr1, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr1);
		return err;
	}

	CAM_INFO_PRINTK("(%s) ISX012 AE scale auto = %d\n", __func__,(short)readValue);
    *ae_auto = (unsigned short)readValue;

    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr2, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr2);
		return err;
	}

	CAM_INFO_PRINTK("(%s) Err AE scale auto  = %d\n", __func__,(short)readValue);
    *ersc_auto = (short)readValue;

    return 0;
}

int camdrv_ss_isx012_get_prestrobeaescale(struct i2c_client *client, unsigned short *ae_now, short *ersc_now, short *aescl)
{
    unsigned short  readAddr1 = 0x01D0; // USER_AEACL_NOW
    unsigned short  readAddr2 = 0x01CC; // ERRSCL_NOWAUTO
    unsigned short  readAddr3 = 0x8BC0; // AESCL
    unsigned short  readValue = 0;
    int err = 0;
    
    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr1, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr1);
		return err;
	}

	CAM_INFO_PRINTK("(%s) ISX012 AE scale now = %d\n", __func__,(short)readValue);
    *ae_now = (unsigned short)readValue;

    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr2, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr2);
		return err;
	}

	CAM_INFO_PRINTK("[(%s) ISX012 Err AE scale now = %d\n", __func__,(short)readValue);
    *ersc_now = (short)readValue;

    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr3, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr3);
		return err;
	}

	CAM_INFO_PRINTK("(%s) ISX012 aescl = %d\n", __func__,(short)readValue);
    *aescl = (short)readValue;

    return 0;
}


int camdrv_ss_isx012_get_usergain_level(struct v4l2_subdev *sd,struct i2c_client *client)
{
    unsigned short  readAddr = 0x01A5; // USER_GAIN_LEVEL_NOW
    unsigned short readValue = 0;
    struct camdrv_ss_state *state = to_state(sd);
    int err = 0;
     if (state->current_flash_mode == FLASH_MODE_AUTO)
     {
	msleep(150);
     }
	err = camdrv_ss_i2c_read_2_bytes_1(client, readAddr, &readValue);
	readValue &= 0xFF;

	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr);
		return err;
	}
	if (readValue < 0) {
        CAM_ERROR_PRINTK("ERR(%s):i2c read error\n", __func__);
        return -1;
    }

    CAM_ERROR_PRINTK("(%s) ISX012 User Gain = %d\n", __func__,readValue);

    return readValue;
}

int camdrv_ss_isx012_calculate_ae_gain_offset(struct i2c_client *client, short ae_auto, short ae_now, short ersc_auto, short ersc_now)
{
	short aediff, aeoffset;
//	int ret=0;

	aediff = (ae_now + ersc_now) - (ae_auto + ersc_auto);

	if (aediff < 0) {
		aediff = 0;
	}

	if ( ersc_now < 0 ) {
		if ( aediff >= AE_MAXDIFF ){
			aeoffset = -AE_OFSETVAL - ersc_now;
		}else{
			CAM_INFO_PRINTK("(%s) ISX012 aeoffset_table position = %d\n", __func__,aediff/10);
			CAM_INFO_PRINTK("(%s) ISX012 aeoffset_table table value  = %d\n", __func__,-aeoffset_table[aediff/10]);
			aeoffset = -aeoffset_table[aediff/10] - ersc_now;
		}
	}
	else	{
		if ( aediff >= AE_MAXDIFF )	{
			aeoffset = -AE_OFSETVAL;
		}
		else	{
			aeoffset = -aeoffset_table[aediff/10];
		}
	}

	// Set AE Gain offset
	camdrv_ss_i2c_write_4_bytes_reverse_data(client,0x0186,aeoffset);

	CAM_INFO_PRINTK("(%s) ISX012 ae_auto  = %d\n", __func__,ae_auto);
	CAM_INFO_PRINTK("(%s) ISX012 ae_now  = %d\n", __func__,ae_now);
	CAM_INFO_PRINTK("(%s) ISX012 ersc_auto  = %d\n", __func__,ersc_auto);
	CAM_INFO_PRINTK("(%s) ISX012 ersc_now  = %d\n", __func__,ersc_now);
	CAM_INFO_PRINTK("(%s) ISX012 aediff  = %d\n", __func__,aediff);
	CAM_INFO_PRINTK("(%s) ISX012 aeoffset  = %d\n", __func__,aeoffset);

	return 0;
}

static int camdrv_ss_isx012_check_wait_for_mode_transition(struct v4l2_subdev *sd, int imode)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);	
    unsigned short read_value = 0;
    unsigned char check_value = 0;
    unsigned char roof_out_count = 0;
    int ret = 0;
/*
	imode value is 0 : Wait for Mode Transition (OM)
	imode value is 1 : Wait for Mode Transition (CM)
	imode value is 2 : Wait for JPEG_UPDATE
	imode value is 3 : Wait for VINT
*/	
	CAM_ERROR_PRINTK( "%s : imode=%d\n",__func__,imode);
	if(imode == 0) // Wait for Mode Transition (OM)
	{
		roof_out_count = 50;

		do {
			msleep(10); // 10ms
			CAM_ERROR_PRINTK( "%s : read_value=%d\n",__func__,read_value);
			//ret = camdrv_ss_i2c_read_2_bytes(client, 0x000E, &read_value);   // INTSTS
			
			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);   // INTSTS
			CAM_ERROR_PRINTK( "%s :read_value=%d ret= %d roof_out_count=%d\n",__func__,read_value,ret,roof_out_count);
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}
			check_value = (read_value & 0x01);
		}while((check_value != 1) && roof_out_count-- );	

		roof_out_count = 50;
		
		do {
			printk("%s %d\n",__func__,__LINE__);
			ret = camdrv_ss_i2c_write_3_bytes(client, 0x0012, 0x01); // Clear Interrupt Status (OM)
			printk("%s %d ret =%d\n",__func__,__LINE__,ret);
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : Clear Interrupt Status  IS FAILED\n",__func__);
				return -EIO;
			}
			msleep(1); // 1ms
			
			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);   // INTSTS
			CAM_ERROR_PRINTK( "%s :read_value=%d ret= %d roof_out_count=%d\n",__func__,read_value,ret,roof_out_count);
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}

			check_value = (read_value & 0x01);
		}while((check_value != 0) && roof_out_count-- );
	}
	else if(imode == 1) // Wait for Mode Transition (CM)
	{
		roof_out_count = 50;
		do {
			msleep(1); // 10ms
			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);  // INTSTS
			CAM_ERROR_PRINTK( "%s : 1 read_value=%x\n",__func__,read_value);
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}
			CAM_ERROR_PRINTK( "%s : 1 check_value=%x roof_out_count=%d\n",__func__,check_value,roof_out_count);

			check_value = ((read_value & 0x02) >> 1);
		}while((check_value != 1) && roof_out_count-- );	
		
		roof_out_count = 50;
		
		do {
			ret = camdrv_ss_i2c_write_3_bytes(client, 0x0012, 0x02); // Clear Interrupt Status (OM)
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : Clear Interrupt Status  IS FAILED\n",__func__);
				return -EIO;
			}
			msleep(1); // 1ms

			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);   // INTSTS
			CAM_ERROR_PRINTK( "%s : 2 read_value=%x\n",__func__,read_value);
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}

			check_value = ((read_value & 0x02) >> 1);
			
			CAM_ERROR_PRINTK( "%s : 2 check_value=%x roof_out_count=%d\n",__func__,check_value,roof_out_count);

		}while((check_value != 0) && roof_out_count-- );
	}
	else if(imode == 2) // Wait for JPEG_UPDATE
	{
		roof_out_count = 50;
		do {
			msleep(10); // 10ms
			ret = camdrv_ss_i2c_read_2_bytes_2(client, 0x000E, &read_value);   // INTSTS

			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}

			check_value = ((read_value & 0x04) >> 2);
		}while((check_value != 1) && roof_out_count-- );	

		roof_out_count = 50;
		
		do {
			ret = camdrv_ss_i2c_write_3_bytes(client, 0x0012, 0x04); // Clear Interrupt Status (OM)
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : Clear Interrupt Status  IS FAILED\n",__func__);
				return -EIO;
			}
			
			msleep(1); // 1ms
			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);   // INTSTS

			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}

			check_value = ((read_value & 0x04) >> 2);
		}while((check_value != 0) && roof_out_count-- );
	}
	else if(imode == 3) // Wait for VINT
	{
		roof_out_count = 50;
		do {
			msleep(1); // 10ms
			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);   // INTSTS

			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}

			check_value = ((read_value & 0x20) >> 5);
		}while((check_value != 1) && roof_out_count-- );	

		roof_out_count = 50;
		
		do {
			ret = camdrv_ss_i2c_write_3_bytes(client, 0x0012, 0x20); // Clear Interrupt Status (OM)
			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : Clear Interrupt Status  IS FAILED\n",__func__);
				return -EIO;
			}
			
			msleep(1); // 1ms
			ret = camdrv_ss_i2c_read_2_bytes_1(client, 0x000E, &read_value);   // INTSTS

			if(ret < 0)
			{
				CAM_ERROR_PRINTK( "%s : INTSTS IS FAILED\n",__func__);
				return -EIO;
			}

			check_value = ((read_value & 0x20) >> 5);
		}while((check_value != 0) && roof_out_count-- );
	}

	return 0;
}

static int camdrv_ss_isx012_pll_setting(struct v4l2_subdev *sd)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);	
    int err = 0;

	// PLL Setting
	
	err = camdrv_ss_i2c_set_config_register(
					client,(regs_t *) isx012_Pll_Setting_4,
					ARRAY_SIZE(isx012_Pll_Setting_4),
					"Pll_Setting_4");
	if (err < 0){
		CAM_ERROR_PRINTK("[%s : %d] ERROR! Couldn't Set isx012_Pll_Setting_4\n", __FILE__, __LINE__);
		return -EIO;	
	}

	return 0;
}

static int camdrv_ss_isx012_sensor_off_vcm(struct v4l2_subdev *sd)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);	
    int err = 0;

	// Sensor Off VCM
	
	err = camdrv_ss_i2c_set_config_register(
					client, (regs_t *)isx012_sensor_off_vcm,
					ARRAY_SIZE(isx012_sensor_off_vcm),
					"sensor_off_vcm");
	if (err < 0){
		CAM_ERROR_PRINTK("[%s : %d] ERROR! Couldn't Set isx012_sensor_off_vcm\n", __FILE__, __LINE__);
		return -EIO;	
	}
	
	return 0;
}

static int camdrv_ss_isx012_enum_frameintervals(struct v4l2_subdev *sd,struct v4l2_frmivalenum *fival)
{
	int err = 0;
	int size,i;

	if (fival->index >= 1)
	{
		CAM_ERROR_PRINTK("%s: returning ERROR because index =%d !!!  \n",__func__,fival->index);
		return -EINVAL;
	}
	

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;

	for(i = 0; i < ARRAY_SIZE(isx012_supported_preview_framesize_list); i++) {
		if((isx012_supported_preview_framesize_list[i].width == fival->width) &&
		    (isx012_supported_preview_framesize_list[i].height == fival->height))
		{
			size = isx012_supported_preview_framesize_list[i].index;
			break;
		}
	}

	if(i == ARRAY_SIZE(isx012_supported_preview_framesize_list))
	{
		CAM_ERROR_PRINTK("%s unsupported width = %d and height = %d\n",
			__func__, fival->width, fival->height);
		return -EINVAL;
	}

	switch (size) {
	case PREVIEW_SIZE_5MP:
	case PREVIEW_SIZE_3MP:
	case PREVIEW_SIZE_2MP:
	case PREVIEW_SIZE_W1MP:
		fival->discrete.numerator = 2;
		fival->discrete.denominator = 15;
		CAM_INFO_PRINTK("%s: w = %d, h = %d, fps = 7.5\n",
			__func__, fival->width, fival->height);
		break;
	default:
		fival->discrete.numerator = 1;
		fival->discrete.denominator = 30;
		CAM_INFO_PRINTK("%s: w = %d, h = %d, fps = 30\n",
			__func__, fival->width, fival->height);
		break;
	}

	return err;
}


static long camdrv_ss_isx012_ss_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
//	struct camdrv_ss_state *state =
//		container_of(sd, struct camdrv_ss_states, sd);
	int ret = 0;

	switch(cmd) {

		case VIDIOC_THUMB_SUPPORTED:
		{
			int *p = arg;
#ifdef JPEG_THUMB_ACTIVE
			*p =1;
#else
			*p = 0; /* NO */
#endif
			break;
		}

		case VIDIOC_THUMB_G_FMT:
		{
			struct v4l2_format *p = arg;
			struct v4l2_pix_format *pix = &p->fmt.pix;
			p->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			/* fixed thumbnail resolution and format */
			pix->width = 640;
			pix->height = 480;
			pix->bytesperline = 640 * 2;
			pix->sizeimage = 640 * 480 * 2;
			pix->field = V4L2_FIELD_ANY;
			pix->colorspace = V4L2_COLORSPACE_JPEG,
			pix->pixelformat = V4L2_PIX_FMT_UYVY;
			break;
		}

		case VIDIOC_THUMB_S_FMT:
		{
		//	struct v4l2_format *p = arg;
			/* for now we don't support setting thumbnail fmt and res */
			ret = -EINVAL;
			break;
		}
		case VIDIOC_JPEG_G_PACKET_INFO:
		{
			struct v4l2_jpeg_packet_info *p = arg;
#ifdef JPEG_THUMB_ACTIVE
			 p->padded = 4;
			 p->packet_size= 0x27c;
#else
			 p->padded = 0;
			 p->packet_size = 0x810;
#endif
			 break;
		}
		case VIDIOC_SENSOR_G_OPTICAL_INFO:
		{
			 struct v4l2_sensor_optical_info *p= arg;
       p->hor_angle.numerator = 512;
       p->hor_angle.denominator = 10;
       p->ver_angle.numerator = 394;
       p->ver_angle.denominator = 10;
       p->focus_distance[0] = 10;
       p->focus_distance[1] = 120;
       p->focus_distance[2] = -1;
       p->focal_length.numerator = 331;
       p->focal_length.denominator = 100;
			break;	
		}
		default:
			ret = -ENOIOCTLCMD;
			break;
	}

	return ret;
}


int camdrv_ss_isx012_set_preview_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;

	CAM_INFO_PRINTK( "%s %d : mode \n", __func__,state->mode_switch);

	if (!state->pix.width || !state->pix.height) {
		CAM_ERROR_PRINTK( "%s : width or height is NULL!!!\n",__func__);
		return -EINVAL;
	}

	if (state->mode_switch == INIT_DONE_TO_CAMERA_PREVIEW) {

		err = camdrv_ss_set_preview_size(sd);
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s : camdrv_ss_set_preview_size is FAILED !!\n", __func__);
			return -EIO;
		}
			
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_preview_camera_regs, ARRAY_SIZE(isx012_preview_camera_regs), "preview_camera_regs");
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s :isx012_preview_camera_regs IS FAILED\n",__func__);
			return -EIO;
		}

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){
				
				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;

				}
			}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;
				}
			}

		err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_af_restart, ARRAY_SIZE(isx012_af_restart), "af_restart");
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s :isx012_af_restart IS FAILED\n",__func__);
			return -EIO;
		}

	}

	if (state->mode_switch == PICTURE_CAPTURE_TO_CAMERA_PREVIEW_RETURN) {

		if(capture_flash)
	    	{
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_Main_Flash_End_EVT1, ARRAY_SIZE(isx012_Main_Flash_End_EVT1), "Main_Flash_End_EVT1");
		if (err < 0) {
				CAM_ERROR_PRINTK( "%s :isx012_preview_camera_regs IS FAILED\n",__func__);
			return -EIO;
		}

			if (state->currentWB== WHITE_BALANCE_AUTO){			
				//Flash AWB mode 
				err = camdrv_ss_i2c_write_3_bytes(client, 0x0282, 0x20);
				if (err != 0) {
		            CAM_ERROR_PRINTK("ERR(%s):i2c write error\n", __func__);
		            return err;
		   		}
	    	}

			// Vlatch On 
			err = camdrv_ss_i2c_write_3_bytes(client, 0x8800, 0x01);
			if (err != 0) {
	            CAM_ERROR_PRINTK("ERR(%s):i2c write error\n", __func__);
	            return err;
	   		}			
			capture_flash = false;
		}

		err = camdrv_ss_set_preview_size(sd);
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s : camdrv_ss_set_preview_size is FAILED !!\n", __func__);
			return -EIO;
		}
		
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_preview_camera_regs, ARRAY_SIZE(isx012_preview_camera_regs), "preview_camera_regs");
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s :isx012_preview_camera_regs IS FAILED\n",__func__);
			return -EIO;
		}

		camdrv_ss_isx012_check_wait_for_mode_transition(sd, 1);
		
		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){
				
				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;

				}
			}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;
				}
			}


		
	}

	if(state->mode_switch == CAMERA_PREVIEW_TO_CAMCORDER_PREVIEW)
	{
		/* CSP614861: temporary added fixed FPS for 720p performance test */
		if (state->mode_switch == CAMERA_PREVIEW_TO_CAMCORDER_PREVIEW || state->mode_switch == INIT_DONE_TO_CAMCORDER_PREVIEW) {
						
			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_HD_Camcorder_regs, ARRAY_SIZE(isx012_HD_Camcorder_regs), "HD_Camcorder_regs");
			if (err < 0) {
				CAM_ERROR_PRINTK("%s : I2C HD_Camcorder_regs IS FAILED\n", __func__);
				return -EIO;
			}
						
			err = camdrv_ss_set_preview_size(sd);
			if (err < 0) {
				CAM_ERROR_PRINTK( "%s : camdrv_ss_set_preview_size is FAILED !!\n", __func__);
				return -EIO;
			}

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_preview_camera_regs, ARRAY_SIZE(isx012_preview_camera_regs), "preview_camera_regs");
			if (err < 0) {
				CAM_ERROR_PRINTK( "%s :isx012_preview_camera_regs IS FAILED\n",__func__);
				return -EIO;
			}

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){
				
				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;

				}
			}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;
				}
			}

			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_af_restart, ARRAY_SIZE(isx012_af_restart), "af_restart");
			if (err < 0) {
				CAM_ERROR_PRINTK( "%s :isx012_af_restart IS FAILED\n",__func__);
				return -EIO;
			}
		}

	}
	else if(state->mode_switch == INIT_DONE_TO_CAMCORDER_PREVIEW)
	{
		{
			CAM_ERROR_PRINTK("%s : Raju state->mode_switch =%d\n", __func__,state->mode_switch );

			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_HD_Camcorder_regs, ARRAY_SIZE(isx012_HD_Camcorder_regs), "HD_Camcorder_regs");
			if (err < 0) {
				CAM_ERROR_PRINTK("%s : I2C HD_Camcorder_regs IS FAILED\n", __func__);
				return -EIO;
			}
	
			err = camdrv_ss_set_preview_size(sd);
			if (err < 0) {
				CAM_ERROR_PRINTK( "%s : camdrv_ss_set_preview_size is FAILED !!\n", __func__);
				return -EIO;
			}

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_preview_camera_regs, ARRAY_SIZE(isx012_preview_camera_regs), "preview_camera_regs");
			if (err < 0) {
				CAM_ERROR_PRINTK( "%s :isx012_preview_camera_regs IS FAILED\n",__func__);
				return -EIO;
			}

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){
				
				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;

				}
			}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;
				}
			}

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_af_restart, ARRAY_SIZE(isx012_af_restart), "af_restart");
			if (err < 0) {
				CAM_ERROR_PRINTK( "%s :isx012_af_restart IS FAILED\n",__func__);
				return -EIO;
			}
		}
	}
	else if(state->mode_switch == CAMCORDER_PREVIEW_TO_CAMERA_PREVIEW)
	{
		
		err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_HD_Camcorder_Disable_regs, ARRAY_SIZE(isx012_HD_Camcorder_Disable_regs), "HD_Camcorder_Disable_regs");
		if (err < 0) {
			CAM_ERROR_PRINTK("%s : I2C HD_Camcorder_Disable_regs IS FAILED\n", __func__);
			return -EIO;
		}
		
		err = camdrv_ss_set_preview_size(sd);
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s : camdrv_ss_set_preview_size is FAILED !!\n", __func__);
			return -EIO;
		}
		
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_preview_camera_regs, ARRAY_SIZE(isx012_preview_camera_regs), "preview_camera_regs");
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s :isx012_preview_camera_regs IS FAILED\n",__func__);
			return -EIO;
		}

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){
				
				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;

				}
			}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				return -EIO;
				}
			}
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_af_restart, ARRAY_SIZE(isx012_af_restart), "af_restart");
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s :isx012_af_restart IS FAILED\n",__func__);
			return -EIO;
		}
	}

	state->camera_flash_fire = 0;
	state->camera_af_flash_checked = 0;

	if (state->check_dataline) { /* Output Test Pattern */
		err = camdrv_ss_set_dataline_onoff(sd, 1);
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s : check_dataline is FAILED !!\n", __func__);
			return -EIO;
		}
	}

	return 0;
}


static int camdrv_ss_isx012_set_capture_start(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;
	unsigned short usrGain=0;
	//int light_state = CAM_NORMAL_LIGHT;

	CAM_INFO_PRINTK( "%s Entered\n", __func__);

	if((touch_af == true) && (state->camera_flash_fire))
	{
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_ae_maual_mode_regs, ARRAY_SIZE(isx012_ae_maual_mode_regs), "ae_maual_mode_regs");
		if (err < 0) {
			CAM_ERROR_PRINTK( "%s :iisx012_ae_maual_mode_regs IS FAILED\n",__func__);
			return -EIO;
		}

		mdelay(66); // 66ms

	}

	/* Set image size */
	err = camdrv_ss_set_capture_size(sd);
	if (err < 0) {
		CAM_ERROR_PRINTK( "%s: camdrv_ss_set_capture_size not supported !!!\n", __func__);
		return -EIO;
	}

	if (state->camera_af_flash_checked == 0) {
		if (state->current_flash_mode == FLASH_MODE_ON) {
			state->camera_flash_fire = 1;
			CAM_INFO_PRINTK("%s : Flash mode is ON\n", __func__);
		} else if (state->current_flash_mode == FLASH_MODE_AUTO) {
			if (state->camera_flash_fire == 1) {
				CAM_INFO_PRINTK(
					"%s: Auto Flash : flash needed\n",
					__func__);
			} else
				CAM_INFO_PRINTK(
					"%s: Auto Flash : flash not needed\n",
					__func__);
		}
   else if (state->current_flash_mode == FLASH_MODE_OFF) {
    state->camera_flash_fire = 0;
			CAM_INFO_PRINTK("%s : Flash mode is OFF\n", __func__);
   }
	}

	/* Set Flash registers */
	if (state->camera_flash_fire) {
		err = camdrv_ss_i2c_set_config_register(
				client,(regs_t *) isx012_Main_Flash_Start_EVT1,
				ARRAY_SIZE(isx012_Main_Flash_Start_EVT1),
				"Main_Flash_Start_EVT1");
		if (err < 0)
			CAM_ERROR_PRINTK(
				"[%s : %d] ERROR! Couldn't Set isx012_Main_Flash_Start_EVT1\n",
				__FILE__, __LINE__);

     camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_MAX_LEVEL);
  mdelay(150); // 100ms     
	}


	/* Set Snapshot registers */

	if (state->currentScene == SCENE_MODE_NIGHTSHOT){
			usrGain = camdrv_ss_isx012_get_usergain_level(sd, client);
		if (usrGain < 0) {
		CAM_ERROR_PRINTK("ERR(%s):get user gain level error\n", __func__);
		return 0;
		}

		if((usrGain & 0xFF) >= 0x3D){
			err = camdrv_ss_i2c_set_config_register(client,
				(regs_t *)isx012_snapshot_nightmode_regs,
				ARRAY_SIZE(isx012_snapshot_nightmode_regs),
				"snapshot_nightmode_regs");
		}
		else{
			err = camdrv_ss_i2c_set_config_register(client,
				(regs_t *)isx012_snapshot_normal_regs,
				ARRAY_SIZE(isx012_snapshot_normal_regs),
				"snapshot_normal_regs");
		}
	}
	else{
		err = camdrv_ss_i2c_set_config_register(client,
				(regs_t *)isx012_snapshot_normal_regs,
				ARRAY_SIZE(isx012_snapshot_normal_regs),
				"snapshot_normal_regs");

	//	mdelay(30); // 100ms     
		if (err < 0) {
			CAM_ERROR_PRINTK(
					"%s : isx012_snapshot_normal_regs i2c failed !\n",
					__func__);
			return -EIO;
		}
	}

//		camdrv_ss_isx012_check_wait_for_mode_transition(sd, 1);
//		camdrv_ss_isx012_check_wait_for_mode_transition(sd, 3);

	if (state->camera_flash_fire) {


		mdelay(210); // 210ms

		{					
			unsigned short  readValue = 0;
			unsigned short  checkValue = 0;
			unsigned int  iRoofOutCnt = 100;

	        // Read AWBSTS
			do {
				mdelay(10);

				err = camdrv_ss_i2c_read_2_bytes_2(client, 0x8A24, &readValue);
				if( err < 0  )
				{
					CAM_ERROR_PRINTK("ERR(%s) error ret =%d", __func__, err);
					return err ;
	    		}

				checkValue = (readValue & 0x06);
				iRoofOutCnt--;
			}while( ((checkValue != 2) && (checkValue != 4) && (checkValue != 6)) && iRoofOutCnt );

			CAM_INFO_PRINTK( "%s() ISX012 Read AWBSTS iRoofOutCnt=%d\n", __func__, iRoofOutCnt);
		}

        capture_flash = state->camera_flash_fire;		
	}
	return 0;
}


int camdrv_ss_isx012_set_iso(struct v4l2_subdev *sd, int mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;


	CAM_INFO_PRINTK( "%s :  value =%d wb_auto=%d, iso_auto=%d\n" , __func__, mode,wb_auto,iso_auto);

	switch (mode) {
	case ISO_AUTO:
	{
#if 0	
		iso_auto=1;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_auto_regs, ARRAY_SIZE(isx012_iso_auto_regs), "iso_auto_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_auto_regs, ARRAY_SIZE(isx012_iso_auto_regs), "iso_auto_regs");
#endif
		
		break;
	}
	case ISO_50:
	{
#if 0	
		iso_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_50_regs, ARRAY_SIZE(isx012_iso_50_regs), "iso_50_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_50_regs, ARRAY_SIZE(isx012_iso_50_regs), "iso_50_regs");
#endif
		
		break;
	}

	case ISO_100:
	{
#if 0	
		iso_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_100_regs, ARRAY_SIZE(isx012_iso_100_regs), "iso_100_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_100_regs, ARRAY_SIZE(isx012_iso_100_regs), "iso_100_regs");
#endif
		
		break;
	}
	case ISO_200:
	{
#if 0	
		iso_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_200_regs, ARRAY_SIZE(isx012_iso_200_regs), "iso_200_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_200_regs, ARRAY_SIZE(isx012_iso_200_regs), "iso_200_regs");
#endif
		
		break;
	}
	case ISO_400:
	{
#if 0	
		iso_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_400_regs, ARRAY_SIZE(isx012_iso_400_regs), "iso_400_regs");
#else
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_iso_400_regs, ARRAY_SIZE(isx012_iso_400_regs), "iso_400_regs");
#endif
		
		break;
	}


	default:
	{
		CAM_ERROR_PRINTK( "%s  : default case supported !!!\n" , __func__);
			break;
        }			
	} /* end of switch */

	iso_value = mode;
	return err;
}

int camdrv_ss_isx012_set_ev(struct v4l2_subdev *sd, int mode)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	CAM_INFO_PRINTK("%s :  value =%d\n", __func__, mode);

	switch (mode) {
	case EV_MINUS_4:
		err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_ev_minus_4_regs, ARRAY_SIZE(isx012_ev_minus_4_regs), "ev_minus_4_regs");
		break;

	case EV_MINUS_3:
		err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_ev_minus_3_regs, ARRAY_SIZE(isx012_ev_minus_3_regs), "ev_minus_3_regs");
		break;

	case EV_MINUS_2:
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_ev_minus_2_regs, ARRAY_SIZE(isx012_ev_minus_2_regs), "ev_minus_2_regs");
		break;

	case EV_MINUS_1:
		err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_ev_minus_1_regs, ARRAY_SIZE(isx012_ev_minus_1_regs), "ev_minus_1_regs");
		break;

	case EV_DEFAULT:
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_ev_default_regs, ARRAY_SIZE(isx012_ev_default_regs), "ev_default_regs");
		break;

	case EV_PLUS_1:
		err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_ev_plus_1_regs, ARRAY_SIZE(isx012_ev_plus_1_regs), "ev_plus_1_regs");
		break;
	
	case EV_PLUS_2:
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_ev_plus_2_regs, ARRAY_SIZE(isx012_ev_plus_2_regs), "ev_plus_2_regs");
 		break;
	
	case EV_PLUS_3:
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_ev_plus_3_regs, ARRAY_SIZE(isx012_ev_plus_3_regs), "ev_plus_3_regs");
		break;

	case EV_PLUS_4:
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_ev_plus_4_regs, ARRAY_SIZE(isx012_ev_plus_4_regs), "ev_plus_4_regs");
		break;

	default:
		CAM_ERROR_PRINTK("%s : default case supported !!!\n", __func__);
		break;
	}
	ev_value = mode;
	return err;
}

int camdrv_ss_isx012_set_white_balance(struct v4l2_subdev *sd, int mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;

	CAM_INFO_PRINTK( "%s :  value =%d wb_auto=%d iso_auto=%d\n" , __func__, mode,wb_auto,iso_auto);

	switch (mode) {
	case WHITE_BALANCE_AUTO:
	{
#if 0	
		wb_auto=1;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_regs, ARRAY_SIZE(isx012_wb_auto_regs), "wb_auto_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_regs, ARRAY_SIZE(isx012_wb_auto_regs), "wb_auto_regs");
#endif
		
		break;
	}

	case WHITE_BALANCE_CLOUDY:
	{
#if 0	
		wb_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_cloudy_regs, ARRAY_SIZE(isx012_wb_cloudy_regs), "wb_cloudy_regs");
#else
	 	err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_cloudy_regs, ARRAY_SIZE(isx012_wb_cloudy_regs), "wb_cloudy_regs");
#endif
		
		break;
	}


	case WHITE_BALANCE_FLUORESCENT:
	{
#if 0	
		wb_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_fluorescent_regs, ARRAY_SIZE(isx012_wb_fluorescent_regs), "wb_fluorescent_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_fluorescent_regs, ARRAY_SIZE(isx012_wb_fluorescent_regs), "wb_fluorescent_regs");
#endif
		
		break;
	}
	case WHITE_BALANCE_DAYLIGHT:
	{
#if 0	
		wb_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_daylight_regs, ARRAY_SIZE(isx012_wb_daylight_regs), "wb_daylight_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_daylight_regs, ARRAY_SIZE(isx012_wb_daylight_regs), "wb_daylight_regs");
#endif
		
		break;
	}
	case WHITE_BALANCE_INCANDESCENT:
	{
#if 0	
		wb_auto=0;
		if(wb_auto==1 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_iso_auto_regs, ARRAY_SIZE(isx012_wb_iso_auto_regs), "wb_iso_auto_regs");
		}else if(wb_auto==0 && iso_auto==1){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_auto_regs, ARRAY_SIZE(isx012_wb_manual_iso_auto_regs), "wb_manual_iso_auto_regs");
		}else if(wb_auto==1 &&iso_auto==0){
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_wb_auto_iso_manual_regs, ARRAY_SIZE(isx012_wb_auto_iso_manual_regs), "wb_auto_iso_manual_regs");
		}else{
			
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_manual_iso_manual_regs, ARRAY_SIZE(isx012_wb_manual_iso_manual_regs), "wb_manual_iso_manual_regs");
		}
		
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_incandescent_regs, ARRAY_SIZE(isx012_wb_incandescent_regs), "wb_incandescent_regs");
#else
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_wb_incandescent_regs, ARRAY_SIZE(isx012_wb_incandescent_regs), "wb_incandescent_regs");
#endif
		break;
	}

	default:
	{
		CAM_ERROR_PRINTK( " %s : default not supported !!!\n" , __func__);
		break;
	}
	}

	state->currentWB = mode;

	return err;
}

//denis
static int camdrv_ss_isx012_set_scene_mode(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;

	CAM_INFO_PRINTK( "%s : value =%d\n", __func__, ctrl->val);
#if 1
	if (state->current_mode == PICTURE_MODE){
		return 0;
	}
#endif	

    	err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_none_regs, ARRAY_SIZE(isx012_scene_none_regs), "scene_none_regs");
	switch (ctrl->val) {
        /*
    	case SCENE_MODE_NONE:
    	{
    		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_none_regs, ARRAY_SIZE(isx012_scene_none_regs), "scene_none_regs");
    		break;
    	}
        */
    	case SCENE_MODE_PORTRAIT:
    	{
    		/* Metering-Center, EV0, WB-Auto, Sharp-1, Sat0, AF-Auto will be set in HAL layer */

   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_portrait_regs, ARRAY_SIZE(isx012_scene_portrait_regs), "scene_portrait_regs");
    		break;
    	}

    	case SCENE_MODE_NIGHTSHOT:
    	{
			CAM_ERROR_PRINTK("night mode normal\n");
    		err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_nightshot_regs, ARRAY_SIZE(isx012_scene_nightshot_regs), "scene_nightshot_regs");
    		break;
    	}

    	case SCENE_MODE_BACK_LIGHT:
    	{
    		/* Metering-Spot, EV0, WB-Auto, Sharp0, Sat0, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_backlight_regs, ARRAY_SIZE(isx012_scene_backlight_regs), "scene_backlight_regs");
    		break;
    	}

    	case SCENE_MODE_LANDSCAPE:
    	{
    		/* Metering-Matrix, EV0, WB-Auto, Sharp+1, Sat+1, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_scene_landscape_regs, ARRAY_SIZE(isx012_scene_landscape_regs), "scene_landscape_regs");
    		break;
    	}

    	case SCENE_MODE_SPORTS:
    	{
   			err = camdrv_ss_i2c_set_config_register(client, (regs_t *)isx012_scene_sports_regs, ARRAY_SIZE(isx012_scene_sports_regs), "scene_sports_regs");
    		break;
    	}

    	case SCENE_MODE_PARTY_INDOOR:
    	{
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_party_indoor_regs, ARRAY_SIZE(isx012_scene_party_indoor_regs), "scene_party_indoor_regs");
    		break;
    	}

    	case SCENE_MODE_BEACH_SNOW:
    	{
    		/* Metering-Center, EV+1, WB-Auto, Sharp0, Sat+1, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_beach_snow_regs, ARRAY_SIZE(isx012_scene_beach_snow_regs), "scene_beach_snow_regs");
    		break;
    	}

    	case SCENE_MODE_SUNSET:
    	{
    		/* Metering-Center, EV0, WB-daylight, Sharp0, Sat0, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_sunset_regs, ARRAY_SIZE(isx012_scene_sunset_regs), "scene_sunset_regs");
    		break;
    	}

    	case SCENE_MODE_DUSK_DAWN:
    	{
    		/* Metering-Center, EV0, WB-fluorescent, Sharp0, Sat0, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_duskdawn_regs, ARRAY_SIZE(isx012_scene_duskdawn_regs), "scene_duskdawn_regs");
    		break;
    	}

    	case SCENE_MODE_FALL_COLOR:
    	{
    		/* Metering-Center, EV0, WB-Auto, Sharp0, Sat+2, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_fall_color_regs, ARRAY_SIZE(isx012_scene_fall_color_regs), "scene_fall_color_regs");
    		break;
    	}

    	case SCENE_MODE_FIREWORKS:
    	{
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_fireworks_regs, ARRAY_SIZE(isx012_scene_fireworks_regs), "scene_fireworks_regs");
    		break;
    	}

    	case SCENE_MODE_TEXT:
    	{
    		/* Metering-Center, EV0, WB-Auto, Sharp+2, Sat0, AF-Macro will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_text_regs, ARRAY_SIZE(isx012_scene_text_regs), "scene_text_regs");
    		break;
    	}

    	case SCENE_MODE_CANDLE_LIGHT:
    	{
    		/* Metering-Center, EV0, WB-Daylight, Sharp0, Sat0, AF-Auto will be set in HAL layer */
   			err = camdrv_ss_i2c_set_config_register(client,(regs_t *) isx012_scene_candle_light_regs, ARRAY_SIZE(isx012_scene_candle_light_regs), "scene_candle_light_regs");
    		break;
    	}

    	default:
    	{
    		CAM_ERROR_PRINTK( "%s default not supported !!!\n", __func__);
    		err = -EINVAL;
    		break;
    	}
	}

	state->currentScene = ctrl->val;

	return err;
}


//static unsigned char pBurstData[MAX_BUFFER];
#if 0
static int camdrv_ss_isx012_i2c_set_data_burst(struct i2c_client *client, 
                                         regs_t reg_buffer[],int num_of_regs,char *name)
{

	struct i2c_msg msg = {client->addr, 0, 0, 0};	
    unsigned short subaddr=0, data_value=0;
	
	int next_subaddr;
    int i;
	int index = 0;
	int err = 0;

    memset(pBurstData, 0, sizeof(pBurstData));
    for(i = 0; i < num_of_regs; i++)
    {

        subaddr = reg_buffer[i] >> 16;
        data_value = reg_buffer[i];

        switch(subaddr)
        {
             case START_BURST_MODE:
            {
                // Start Burst datas
                if(index == 0)
                {
                    pBurstData[index++] = subaddr >> 8;
                    pBurstData[index++] = subaddr & 0xFF;	
                }	

                pBurstData[index++] = data_value >> 8;
                pBurstData[index++] = data_value & 0xFF;	

                // Get Next Address
                if((i+1) == num_of_regs)  // The last code
                {
                    next_subaddr = 0xFFFF;   // Dummy
                }
                else
                {
                    next_subaddr = reg_buffer[i+1]>>16;
                }

                // If next subaddr is different from the current subaddr
                // In other words, if burst mode ends, write the all of the burst datas which were gathered until now
                if(next_subaddr != subaddr) 
                {
                    msg.buf = pBurstData;
                    msg.len = index;

                    err = i2c_transfer(client->adapter, &msg, 1);
                	if(err < 0)
                	{
                		CAM_ERROR_PRINTK("[%s: %d] i2c burst write fail\n", __FILE__, __LINE__);	
                		return -EIO;
                	}

                    // Intialize and gather busrt datas again.
                    index = 0;
                    memset(pBurstData, 0, sizeof(pBurstData));
                }
                break;
            }
            case DELAY_SEQ:
            {
				if(data_value == DELAY_SEQ)
		   			break;				

//				CAM_ERROR_PRINTK("%s : added sleep for  = %d msec in %s !PLEASE CHECK !!! \n", __func__,data_value,name);
                msleep(data_value);
                break;
            }

            case 0xFCFC:
            case 0x0028:
            case 0x002A:
            default:
            {
				      err = camdrv_ss_i2c_write_4_bytes_reverse_data(client, subaddr, data_value);
            	if(err < 0)
            	{
            		CAM_ERROR_PRINTK("%s :i2c transfer failed ! \n", __func__);
            		return -EIO;
            	}
            	break;
            }            
        }
    }


    return 0;
}
#endif
#define DS_PULS_HI_TIME    2
#define DS_PULS_LO_TIME    2
#define DS_LATCH_TIME      500
#ifdef CONFIG_FLASH_DS8515
static void camdrv_ss_DS8515_flash_write_data(unsigned char count)
{
    unsigned long flags;
	CAM_INFO_PRINTK( "%s %d :camdrv_ss_DS8515_flash_write_data  E\n", __func__, count);

    local_irq_save(flags);

    {
        if(count)
        {
            do 
            {
           
				 gpio_set_value(GPIO_CAM_FLASH_SET, 0); //low till T_off
                udelay(DS_PULS_LO_TIME);

				 gpio_set_value(GPIO_CAM_FLASH_SET, 1); //go high
				 udelay(DS_PULS_HI_TIME);
            } while (--count);

            udelay(DS_LATCH_TIME);
        }
    }
    
    local_irq_restore(flags);
}


#endif
#define MIC_PULS_HI_TIME	1
#define MIC_PULS_LO_TIME	1
#define MIC_LATCH_TIME		150
#define MIC_END_TIME		500

/* MIC2871 flash control driver.*/
#ifdef CONFIG_FLASH_MIC2871
static void camdrv_ss_MIC2871_flash_write_data(char addr, char data)
{
	int i;
	CAM_INFO_PRINTK("%s %d %d:  E\n", __func__, addr, data);
	/* send address */
	printk(KERN_ALERT "%s addr(%d) data(%d)\n", __func__, addr, data);
	for (i = 0; i < (addr + 1); i++) {
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		udelay(1);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
		udelay(1);
	}
	/* wait T lat */
    udelay(97);
	/* send data */
	for (i = 0; i < (data + 1); i++) {
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		udelay(1);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
		udelay(1);
	}
	/* wait T end */
    udelay(405);
}
#endif

#define AAT_PULS_HI_TIME    1
#define AAT_PULS_LO_TIME    1
#define AAT_LATCH_TIME      500

// AAT1271 flash control driver.
#ifdef CONFIG_FLASH_SKY81279
static void camdrv_ss_AAT_flash_write_data(unsigned char count)
{
    unsigned long flags;
	CAM_INFO_PRINTK( "%s %d :camdrv_ss_isx012_AAT_flash_write_data  E\n", __func__, count);

    local_irq_save(flags);
/*
    if(HWREV >= 0x03)
    {
        if(count)
        {
            do 
            {
                gpio_set_value(GPIO_CAM_FLASH_SET_NEW, 0);
                udelay(AAT_PULS_LO_TIME);

                gpio_set_value(GPIO_CAM_FLASH_SET_NEW, 1);
                udelay(AAT_PULS_HI_TIME);
            } while (--count);

            udelay(AAT_LATCH_TIME);
        }
    }
    else*/
    {
        if(count)
        {
            do 
            {
				/*
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(AAT_PULS_LO_TIME);

				gpio_set_value(GPIO_CAM_FLASH_SET, 1);
				udelay(AAT_PULS_HI_TIME);
				*/
				gpio_set_value(GPIO_CAM_FLASH_SET, 0); /*low till T_off */
				udelay(AAT_PULS_LO_TIME);

				gpio_set_value(GPIO_CAM_FLASH_SET, 1); /* go high */
				udelay(AAT_PULS_LO_TIME);
			} while (--count);

			udelay(AAT_LATCH_TIME);
		}
    }
    
    local_irq_restore(flags);
}
#endif
//static int ktd2692_flash_enset;
//static int ktd2692_flash_flen;

#define T_DS		30	//	12
#define T_EOD_H		600 //	350
#define T_EOD_L		30
#define T_H_LB		10
#define T_L_LB		2*T_H_LB
#define T_L_HB		10
#define T_H_HB		2*T_L_HB
#define T_RESET		800	//	700
#define T_RESET2	1000
/* KTD2692 : command address(A2:A0) */
#define LVP_SETTING		0x0 << 5
#define FLASH_TIMEOUT	0x1 << 5
#define MIN_CURRENT		0x2 << 5
#define MOVIE_CURRENT	0x3 << 5
#define FLASH_CURRENT	0x4 << 5
#define MODE_CONTROL	0x5 << 5
void KTD2692_set_flash(unsigned int ctl_cmd)
{
	int i=0;
	int j = 0;
	int k = 0;
	unsigned long flags;
	unsigned int ctl_cmd_buf;
	
	spinlock_t lock;
	spin_lock_init(&lock);
	printk(KERN_ALERT "%s ctl_cmd(%d)\n", __func__, ctl_cmd);
	if ( MODE_CONTROL == (MODE_CONTROL & ctl_cmd) )
		k = 8;
	else
		k = 1;
	spin_lock_irqsave(&lock, flags);

	for(j = 0; j < k; j++) {

		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
		udelay(T_DS);
		ctl_cmd_buf = ctl_cmd;
		for(i = 0; i < 8; i++) {
			if(ctl_cmd_buf & 0x80) { /* set bit to 1 */
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(T_L_HB);
				gpio_set_value(GPIO_CAM_FLASH_SET, 1);
				udelay(T_H_HB);
				//printk(KERN_ALERT "bit 1\n");
			} else { /* set bit to 0 */
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(T_L_LB);
				gpio_set_value(GPIO_CAM_FLASH_SET, 1);
				udelay(T_H_LB);
				//printk(KERN_ALERT "bit 0\n");
			}
			ctl_cmd_buf = ctl_cmd_buf << 1;
		}

		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		udelay(T_EOD_L);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
		udelay(T_EOD_H);
	}
	spin_unlock_irqrestore(&lock, flags);
}

/*
static int ktd2692_led(int light, int mode)
{

	printk(KERN_ALERT "%s add_primary_cam_flash_ktd2692)\n", __func__, light);
	int reg;
	unsigned char reg_val;

	gpio_request(ktd2692_flash_enset, "camacq");
	gpio_request(ktd2692_flash_flen, "camacq");

	switch (light) {
	case SH_RCU_LED_ON:

		if (mode == SH_RCU_LED_MODE_PRE) {
                printk(KERN_ALERT " inside preflash step 1\n");
		gpio_set_value(ktd2692_flash_enset, 0);
			udelay(T_RESET2);
			KTD2692_set_flash(LVP_SETTING | 0x00);
			KTD2692_set_flash(MOVIE_CURRENT | 0x07);
			KTD2692_set_flash(MODE_CONTROL | 0x01);
			} else {
                printk(KERN_ALERT " inside main flash step 1\n");
		gpio_set_value(ktd2692_flash_enset, 0);
			udelay(T_RESET2);
			KTD2692_set_flash(LVP_SETTING | 0x00);
			KTD2692_set_flash(FLASH_CURRENT | 0x0F);
			KTD2692_set_flash(MODE_CONTROL | 0x02);
			}
		break;
	case SH_RCU_LED_OFF:
		KTD2692_set_flash(MODE_CONTROL | 0x00);
		gpio_set_value(ktd2692_flash_enset, 0);
		break;
	default:
		gpio_set_value(ktd2692_flash_flen, 0);
		gpio_set_value(ktd2692_flash_enset, 0);
		udelay(500);
		break;
	}
	gpio_free(ktd2692_flash_flen);
	gpio_free(ktd2692_flash_enset);

	return 0;
}
*/
static int camdrv_ss_isx012_KTD2692_flash_control(struct v4l2_subdev *sd, int control_mode)
{
	
gpio_request(GPIO_CAM_FLASH_SET, "flash_set");
gpio_request(GPIO_CAM_FLASH_EN, "flash_en");
gpio_direction_output(GPIO_CAM_FLASH_SET,0);
gpio_direction_output(GPIO_CAM_FLASH_EN,0);
	CAM_INFO_PRINTK( "%s %d :  E\n",  __func__, control_mode);
	
    switch(control_mode)
	{
        // USE FLASH MODE
		case FLASH_CONTROL_MAX_LEVEL:
		{	
			if(flash_check==0)
			{
				//spin_lock_irqsave(&lock, flags);
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(T_RESET2);
				KTD2692_set_flash(LVP_SETTING | 0x00);
				KTD2692_set_flash(FLASH_CURRENT | 0x0F);
				KTD2692_set_flash(MODE_CONTROL | 0x02);
				//spin_unlock_irqrestore(&lock, flags);
			}
			break;
		}
        // USE FLASH MODE
		case FLASH_CONTROL_HIGH_LEVEL:
		{	
			if(flash_check==0)
			{
				//spin_lock_irqsave(&lock, flags);
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(T_RESET2);
				KTD2692_set_flash(LVP_SETTING | 0x00);
				KTD2692_set_flash(MOVIE_CURRENT | 0x04);
				KTD2692_set_flash(MODE_CONTROL | 0x01);
				//spin_unlock_irqrestore(&lock, flags);
			}
			break;
		}
        // USE MOVIE MODE : AF Pre-Flash Mode(Torch Mode)
		case FLASH_CONTROL_MIDDLE_LEVEL:
		{	
			if(flash_check==0)
			{
				//spin_lock_irqsave(&lock, flags);
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(T_RESET2);
				KTD2692_set_flash(LVP_SETTING | 0x00);
				KTD2692_set_flash(MOVIE_CURRENT | 0x04);
				KTD2692_set_flash(MODE_CONTROL | 0x01);
				//spin_unlock_irqrestore(&lock, flags);
			}
			break;
		}

			// USE MOVIE MODE : Movie Mode(Torch Mode)
		case FLASH_CONTROL_LOW_LEVEL:
		{
			if(flash_check==0)
			{
				//spin_lock_irqsave(&lock, flags);
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				udelay(T_RESET2);
				KTD2692_set_flash(LVP_SETTING | 0x00);
				KTD2692_set_flash(MOVIE_CURRENT | 0x04);
				KTD2692_set_flash(MODE_CONTROL | 0x01);
				//spin_unlock_irqrestore(&lock, flags);
			}
			break;
		}

		case FLASH_CONTROL_OFF:
		default:
		{
			if(flash_check==0)
			{
				KTD2692_set_flash(MODE_CONTROL | 0x00);
				gpio_set_value(GPIO_CAM_FLASH_EN, 0);
				gpio_set_value(GPIO_CAM_FLASH_SET, 0);
			}
			break;
		}
	}
	return 0;
}

static float camdrv_ss_isx012_get_exposureTime(struct v4l2_subdev *sd)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);	
    unsigned short readAddr1 = 0x019C; // SHT_TIME_OUT_L
    unsigned short readAddr2 = 0x019E; // SHT_TIME_OUT_H
    unsigned short readValue = 0;
    int exposureTime = 0;
    int err = 0;

    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr1, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr1);
		return err;
	}

    CAM_INFO_PRINTK( "ISX012 Shutter  lower_readValue = %d\n", readValue );
    exposureTime = readValue;

    err = camdrv_ss_i2c_read_2_bytes_2(client, readAddr2, &readValue);
	if( err < 0 )
	{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr2);
		return err;
}

	CAM_INFO_PRINTK( "ISX012 Shutter  upper_readValue = %d\n", readValue );

    exposureTime |= (readValue << 16);

    CAM_INFO_PRINTK("ISX012 exposureTime = %dus\n", exposureTime);

    return exposureTime;
}

//aska add
static int camdrv_ss_isx012_get_nightmode(struct v4l2_subdev *sd)
{
	return 0;
};

static int camdrv_ss_isx012_get_iso_speed_rate(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const short ISOSENS_OUT[] =
		{25, 32, 40, 50, 64, 80, 100, 125, 160, 200,
		 250, 320, 400, 500, 640, 800, 1000, 1250, 1600};
	int len = sizeof(ISOSENS_OUT) / sizeof(short);
	unsigned short	readAddr = 0x019A; // ISOSENS_OUT
	unsigned char readValue = 0;
	unsigned short readshortValue=0;
	int isospeedrating = 100;
	int err = 0;

	//err = camdrv_ss_i2c_read_2_bytes_1(client, readAddr, &readValue);
	err = camdrv_ss_i2c_read_2_bytes_1(client, readAddr, &readshortValue);
	readValue = (unsigned char) readshortValue;
	if( err < 0 )
		{
		CAM_ERROR_PRINTK("i2c read fail, readAddr=0x%x\n", readAddr);
		return err;
		}

	if ((0 < readValue) && (readValue <= len)) {
		isospeedrating = ISOSENS_OUT[readValue - 1];
	} else {
		CAM_ERROR_PRINTK("ERR(%s):Invalid ISOSENS_OUT(%d)\n", __func__, readValue);
			isospeedrating = 400;
		}

	CAM_INFO_PRINTK("%s: ISOSENS_OUT=%d iso=%d\n", __func__, readAddr, isospeedrating);

	return isospeedrating;
}
	
/*NIKHIL*/
#if 0
static int camdrv_ss_isx012_set_ae_stable_status(struct v4l2_subdev *sd, unsigned short value)
{ 


 struct i2c_client *client = v4l2_get_subdevdata(sd);
    int err = 0;
	CAM_INFO_PRINTK("camdrv_ss_isx012_get_ae_stable_status E \n");

    //Check AE stable
    err = camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x0028, 0x7000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002A, 0x0588);
    err += camdrv_ss_i2c_write_2_bytes(client, 0x0F12, (unsigned char)value);
    
    if(err < 0)
    {
        CAM_ERROR_PRINTK("[%s: %d] ERROR! AE stable check\n", __FILE__, __LINE__);
    }
	return err;
}
#endif
/*NIKHIL*/
#if 0
static int camdrv_ss_isx012_get_ae_stable_status_value(struct v4l2_subdev *sd)
{
 struct i2c_client *client = v4l2_get_subdevdata(sd);
    int err = 0;
    unsigned short AE_stable = 0x0000;
    int rows_num_=0;

    //Check AE stable
    err = camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x0028, 0x7000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002A, 0x2c74);
    err += camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &AE_stable);

    if(err < 0)
    {
        CAM_ERROR_PRINTK("[%s: %d] ERROR! AE stable check\n", __FILE__, __LINE__);
    }

   return AE_stable;  
}
#endif
#if 0
static int camdrv_ss_isx012_get_ae_stable_status(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{   

    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int err = 0;
    unsigned short AE_stable = 0x0000;
	int rows_num_=0;
    
    //Check AE stable
    err = camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002C, 0x7000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002E, 0x1E3C);
    err += camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &AE_stable);

    if(err < 0)
    {
        CAM_ERROR_PRINTK("[%s: %d] ERROR! AE stable check\n", __FILE__, __LINE__);
    }

    if(AE_stable == 0x0001)
    {
        ctrl->val = AE_STABLE;
    }
    else
    {
        ctrl->val = AE_UNSTABLE;
    }

   struct i2c_client *client = v4l2_get_subdevdata(sd);
    int err = 0;
    unsigned short AE_stable = 0x0000;
    //int rows_num_=0;
  CAM_INFO_PRINTK("camdrv_ss_isx012_get_ae_stable_status E \n");
    //Check AE stable
    err = camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x0028, 0x7000);
    err += camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002A, 0x2c74);
    err += camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &AE_stable);

     if(err < 0)
    {
        CAM_ERROR_PRINTK("[%s: %d] ERROR! AE stable check\n", __FILE__, __LINE__);
    }

    if(AE_stable == 0x0001)
    {
        ctrl->val = AE_STABLE;
    }
    else
    {
        ctrl->val = AE_UNSTABLE;
    }

    return 0;
}        
// end 
#endif

static int camdrv_ss_isx012_get_auto_focus_status(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{ 
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct camdrv_ss_state *state = to_state(sd);
    int err = 0;
    unsigned short usReadData =0 ;
    CAM_INFO_PRINTK("camdrv_ss_isx012_get_auto_focus_status E \n");
	
	if(!first_af_status){
		err = camdrv_ss_i2c_read_2_bytes_1(client, 0x8B8A,&usReadData);
		if(err < 0){
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			CAM_ERROR_PRINTK("%s: I2C failed during AF \n", __func__);
			ctrl->val = CAMERA_AF_STATUS_FAILED;
			return -EFAULT;
			}

		CAM_INFO_PRINTK(" 1st check status, usReadData : 0x%x\n", usReadData );

		switch( usReadData & 0xFF )
			{
			case 8:
				CAM_INFO_PRINTK("1st CAM_AF_SUCCESS\n " );
				ctrl->val = CAMERA_AF_STATUS_SEARCHING;
				first_af_status = true;
				err = camdrv_ss_i2c_set_config_register(client,	(regs_t *)isx012_af_write,	ARRAY_SIZE(isx012_af_write),"af_write");
				if (err < 0) {
					camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
					CAM_ERROR_PRINTK("%s: I2C failed during isx012_get_2nd_af_search_status\n", __func__);
					ctrl->val = CAMERA_AF_STATUS_FAILED;
					return -EFAULT;
					}
				break;
				default:
					CAM_ERROR_PRINTK("1st CAM_AF_FAIL.\n ");
					ctrl->val = CAMERA_AF_STATUS_SEARCHING;
					}
		}
	
	if (first_af_status){
		err = camdrv_ss_i2c_read_2_bytes_1(client, 0x8B8B, &usReadData);
		if(err < 0){
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			CAM_ERROR_PRINTK("%s: I2C failed during AF \n", __func__);
			ctrl->val = CAMERA_AF_STATUS_FAILED;
			first_af_status = false;
			return -EFAULT;
			}
		
		CAM_INFO_PRINTK(" 2nd check status, usReadData : 0x%x\n", usReadData );
		
		if ((usReadData & 0xff)==1){
			CAM_INFO_PRINTK("2nd CAM_AF_SUCCESS, \n ");
			ctrl->val = CAMERA_AF_STATUS_FOCUSED;
			first_af_status = false;

			if (state->camera_flash_fire == 1) {
				err = camdrv_ss_isx012_get_prestrobeaescale(client, &ae_now, &ersc_now, &aescl);
	            if (err < 0) {
					CAM_ERROR_PRINTK("ERR(%s):get AE scale error\n", __func__);
					return err;
				}
				}
					if(flagforAE==1){
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_af_saf_off,	ARRAY_SIZE(isx012_af_saf_off),"af_saf_off");
						}
					else{
	err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_af_touchsaf_off,	ARRAY_SIZE(isx012_af_touchsaf_off),"af_touchsaf_off");
						}
			if (err < 0) {
				CAM_ERROR_PRINTK("%s: I2C failed \n", __func__);
				return -EFAULT;
				}
			
			if (state->camera_flash_fire == 1) {
				mdelay(66); //66 ms 

				// AE SCL
                camdrv_ss_isx012_get_manoutgain(client, aescl);

				// Set AE Gain Offset
				camdrv_ss_isx012_calculate_aegain_offset(client, ae_auto, ae_now, ersc_auto, ersc_now);

				}
			
			if(flash_check==0&&torch_check==0)
			{
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			}
			if (state->currentScene == SCENE_MODE_NIGHTSHOT) {
				msleep(200);
				}
			touch_af = false;
			return 0;
			}
		else
		{
			CAM_ERROR_PRINTK("2nd CAMERA_AF_STATUS_FAILED.... \n ");
			ctrl->val = CAMERA_AF_STATUS_FAILED;
						first_af_status = false;

			if (state->camera_flash_fire == 1) {
				err = camdrv_ss_isx012_get_prestrobeaescale(client, &ae_now, &ersc_now, &aescl);
				if(err < 0){
					CAM_ERROR_PRINTK("ERR(%s):get AE scale error\n", __func__);
					return err;
				}
			}
			
					if(flagforAE==1){
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_af_saf_off,	ARRAY_SIZE(isx012_af_saf_off),"af_saf_off");
						}
					else{
	err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_af_touchsaf_off,	ARRAY_SIZE(isx012_af_touchsaf_off),"af_touchsaf_off");
						}
				if(err < 0){				
				CAM_ERROR_PRINTK("%s: I2C failed \n", __func__);
				return -EFAULT;
				}
			
			if (state->camera_flash_fire == 1) {
				mdelay(66); //66 ms 

				// AE SCL
                camdrv_ss_isx012_get_manoutgain(client, aescl);

				// Set AE Gain Offset
				camdrv_ss_isx012_calculate_aegain_offset(client, ae_auto, ae_now, ersc_auto, ersc_now);

			}
						
			if(flash_check==0&&torch_check==0)
			{
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			}
			if (state->currentScene == SCENE_MODE_NIGHTSHOT) {
				msleep(200);
				}
			touch_af = false;

			}
		}
	return 0;
}


static int camdrv_ss_isx012_get_touch_focus_status(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{ 
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct camdrv_ss_state *state = to_state(sd);
    int err = 0;
    unsigned short usReadData =0 ;
    CAM_INFO_PRINTK("camdrv_ss_isx012_get_touch_focus_status E \n");
	
	if(!first_af_status){
		err = camdrv_ss_i2c_read_2_bytes_1(client, 0x8B8A,&usReadData);
		if(err < 0){
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			CAM_ERROR_PRINTK("%s: I2C failed during AF \n", __func__);
			ctrl->val = CAMERA_AF_STATUS_FAILED;
			return -EFAULT;
			}

		CAM_INFO_PRINTK(" 1st check status, usReadData : 0x%x\n", usReadData );

		switch( usReadData & 0xFF )
			{
			case 8:
				CAM_INFO_PRINTK("1st CAM_AF_SUCCESS\n " );
				ctrl->val = CAMERA_AF_STATUS_SEARCHING;
				first_af_status = true;
				err = camdrv_ss_i2c_set_config_register(client,	(regs_t *)isx012_af_write,	ARRAY_SIZE(isx012_af_write),"af_write");
				if (err < 0) {
					camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
					CAM_ERROR_PRINTK("%s: I2C failed \n", __func__);
					ctrl->val = CAMERA_AF_STATUS_FAILED;
					return -EFAULT;
					}
				break;
				default:
					CAM_ERROR_PRINTK("1st CAM_AF_FAIL.\n ");
					ctrl->val = CAMERA_AF_STATUS_SEARCHING;
					}
		}
	
	if (first_af_status){
		err = camdrv_ss_i2c_read_2_bytes_1(client, 0x8B8B, &usReadData);
		if(err < 0){
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			CAM_ERROR_PRINTK("%s: I2C failed during AF \n", __func__);
			ctrl->val = CAMERA_AF_STATUS_FAILED;
			first_af_status = false;
			return -EFAULT;
			}
		
		CAM_INFO_PRINTK(" 2nd check status, usReadData : 0x%x\n", usReadData );
		if ((usReadData & 0xff)==1){
			CAM_INFO_PRINTK("2nd CAM_AF_SUCCESS, \n ");
			ctrl->val = CAMERA_AF_STATUS_FOCUSED;
			first_af_status = false;

			if (state->camera_flash_fire == 1) {
				err = camdrv_ss_isx012_get_prestrobeaescale(client, &ae_now, &ersc_now, &aescl);
	            if (err < 0) {
					CAM_ERROR_PRINTK("ERR(%s):get AE scale error\n", __func__);
					return err;
				}
			}
			
			err = camdrv_ss_i2c_set_config_register(client,	(regs_t *)isx012_af_touchsaf_off,	ARRAY_SIZE(isx012_af_touchsaf_off),"af_touchsaf_off");
			if (err < 0) {
				CAM_ERROR_PRINTK("%s: I2C failed \n", __func__);
				return -EFAULT;
			}
			
			if (state->camera_flash_fire == 1) {
				mdelay(66); //66 ms 

				// AE SCL
                camdrv_ss_isx012_get_manoutgain(client, aescl);

				// Set AE Gain Offset
				camdrv_ss_isx012_calculate_aegain_offset(client, ae_auto, ae_now, ersc_auto, ersc_now);


				}

			if(flash_check==0&&(torch_check==0||state->current_mode!=CAMCORDER_PREVIEW_MODE))
			{
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			}
			if (state->currentScene == SCENE_MODE_NIGHTSHOT) {
				msleep(200);
				}

			touch_af = true;
			return 0;

			}
		else	
		{
			CAM_ERROR_PRINTK("2nd CAMERA_AF_STATUS_FAILED.... \n ");
			ctrl->val = CAMERA_AF_STATUS_FAILED;
			first_af_status = false;

			if (state->camera_flash_fire == 1) {
				err = camdrv_ss_isx012_get_prestrobeaescale(client, &ae_now, &ersc_now, &aescl);
	            if (err < 0) {
					CAM_ERROR_PRINTK("ERR(%s):get AE scale error\n", __func__);
					return err;
				}
			}
			
			err = camdrv_ss_i2c_set_config_register(client,	(regs_t *)isx012_af_touchsaf_off,	ARRAY_SIZE(isx012_af_touchsaf_off),"af_touchsaf_off");
			if (err < 0) {
				CAM_ERROR_PRINTK("%s: I2C failed \n", __func__);
				return -EFAULT;
			}
			
			if (state->camera_flash_fire == 1) {
				mdelay(66); //66 ms 

				// AE SCL
                camdrv_ss_isx012_get_manoutgain(client, aescl);

				// Set AE Gain Offset
				camdrv_ss_isx012_calculate_aegain_offset(client, ae_auto, ae_now, ersc_auto, ersc_now);


				}

			if(flash_check==0&&(torch_check==0||state->current_mode!=CAMCORDER_PREVIEW_MODE))
			{
			camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
			}
			if (state->currentScene == SCENE_MODE_NIGHTSHOT) {
				msleep(200);
				}

			touch_af = true;

			}
		}
	return 0;
}


static int camdrv_ss_isx012_set_auto_focus(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;
	unsigned short usrGain=0;

	mutex_lock(&af_cancel_op);
	CAM_INFO_PRINTK("camdrv_ss_isx012_set_auto_focus E \n");

	first_af_status = false;


	state->bStartFineSearch = false;

	if(ctrl->val == AUTO_FOCUS_ON){
		CAM_INFO_PRINTK("camdrv_ss_isx012_set_auto_focus E  %d\n", state->camera_af_flash_checked);
		if (state->current_flash_mode == FLASH_MODE_ON) {
			state->camera_flash_fire = 1;
		}
		else if (state->current_flash_mode == FLASH_MODE_AUTO) {
			bool bflash_needed = false;
			if (camdrv_ss_isx012_check_flash_needed(sd))
				bflash_needed = true;
			else
				CAM_ERROR_PRINTK( "%s : check_flash_needed is NULL !!!\n", __func__);
			if (bflash_needed) {
				state->camera_flash_fire = 1;
			}
		}

		CAM_INFO_PRINTK("%s, AUTFOCUS ON\n", __func__ );
		CAM_INFO_PRINTK("%s , AF start\n ",__func__);

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_af_window_reset,ARRAY_SIZE(isx012_af_window_reset),"af_window_reset");
		if(err < 0){

			CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;

			}
		}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}

		if(set_preflash==true)
			camdrv_ss_isx012_set_preflash_sequence(sd, ctrl);


		if(state->camera_flash_fire!=1){
			if (state->currentScene == SCENE_MODE_NIGHTSHOT){
						usrGain = camdrv_ss_isx012_get_usergain_level(sd, client);
				if (usrGain < 0) {
				CAM_ERROR_PRINTK("ERR(%s):get user gain level error\n", __func__);
				return 0;
				}

				if((usrGain & 0xFF) >= 0x3D){
					err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_lowlux_night_halfrelease_mode, 
					ARRAY_SIZE(isx012_lowlux_night_halfrelease_mode),"lowlux_night_halfrelease_mode");
				}
				else{
					err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_halfrelease_mode, ARRAY_SIZE(isx012_halfrelease_mode),"halfrelease_mode");
				}
									if(err < 0){

					CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}
				msleep(40);
			}
		else if(state->mode_switch == CAMERA_PREVIEW_TO_CAMCORDER_PREVIEW || state->mode_switch == INIT_DONE_TO_CAMCORDER_PREVIEW)
		{
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_Camcorder_SAF_mode, ARRAY_SIZE(isx012_Camcorder_SAF_mode),"isx012_Camcorder_SAF_mode");
			if(err < 0){
				CAM_ERROR_PRINTK("%s : i2c failed !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}

			msleep(40);

		}

		else{
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_halfrelease_mode, ARRAY_SIZE(isx012_halfrelease_mode),"halfrelease_mode");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
			msleep(40);
		}
	}
		else if(state->mode_switch == CAMERA_PREVIEW_TO_CAMCORDER_PREVIEW || state->mode_switch == INIT_DONE_TO_CAMCORDER_PREVIEW)
			{
				err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_Camcorder_SAF_mode, ARRAY_SIZE(isx012_Camcorder_SAF_mode),"isx012_Camcorder_SAF_mode");
				if(err < 0){
					CAM_ERROR_PRINTK("%s : i2c failed !!! \n", __func__);
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}

				msleep(40);

			}
}
else if(ctrl->val == AUTO_FOCUS_OFF) {

	CAM_INFO_PRINTK("%s :  AUTFOCUS OFF , af_mode : %d\n", __func__, state->af_mode);

	if (state->af_mode == FOCUS_MODE_AUTO) {

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs_cancel1,ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1),"focus_mode_auto_regs_cancel1");
		if(err < 0){

			CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}
	}
	else if (state->af_mode == FOCUS_MODE_MACRO) {

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs_cancel1,ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1),"focus_mode_macro_regs_cancel1");
		if(err < 0){

			CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}
	}

	if (state->camera_flash_fire) {
		camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
		state->camera_flash_fire = 0;
	}
}

else if(ctrl->val == AUTO_FOCUS_1ST_CANCEL){

	if (state->af_mode == FOCUS_MODE_AUTO) {

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs_cancel1, ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1),"focus_mode_auto_regs_cancel1");
		if(err < 0){

			CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}
	}
	else if (state->af_mode == FOCUS_MODE_MACRO) {

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs_cancel1,ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1),"focus_mode_macro_regs_cancel1");
		if(err < 0){

			CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}
	}
}
else if(ctrl->val == AUTO_FOCUS_2ND_CANCEL ){
	if (state->af_mode == FOCUS_MODE_AUTO) {

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs_cancel1, ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1),"focus_mode_auto_regs_cancel1");
		if(err < 0){
			CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}
	}
	else if (state->af_mode == FOCUS_MODE_MACRO) {

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs_cancel1, ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1),"focus_mode_macro_regs_cancel1");
		if(err < 0){

			CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
			mutex_unlock(&af_cancel_op);
			return -EIO;
		}
	}
}
mutex_unlock(&af_cancel_op);
return 0;

}


static int camdrv_ss_isx012_set_touch_focus_area(struct v4l2_subdev *sd, enum v4l2_touch_af touch_af, v4l2_touch_area *touch_area)
{
	struct camdrv_ss_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
 	int err = 0;
	int preview_width, preview_height;
	const int OUTPUT_IMAGE_AREA_W = 2592;
	const int OUTPUT_IMAGE_AREA_H = 1944;
	first_af_status = false;
	
	preview_width = state->pix.width;
	preview_height = state->pix.height;
	
	
	
	 
	
//	int ret;

CAM_INFO_PRINTK("[%s:%d] preview_width %d, preview_height %d\n",__func__, __LINE__, preview_width, preview_height);	 
	if(touch_af == TOUCH_AF_START){
//		char *end;
		// (-1000,-1000) is the upper left point. (1000, 1000) is the lower right
		// point.
		int left   = touch_area->leftTopX;
		int top    = touch_area->leftTopY;
		int right  = touch_area->rightBottomX;
		int bottom = touch_area->rightBottomY;
		int width  = right - left;
		int height = bottom - top;

		CAM_ERROR_PRINTK("%s:AF-1 (%d,%d,%d,%d)", __func__, left, top, right, bottom);
		CAM_ERROR_PRINTK("%s rcu_output_size=%dx%d", __func__,preview_width, preview_height);
		
		if ((0 != left) || (top != 0) || (0 != right) || (0 != bottom)) /*(m_zoom_ratio != 0)*/{// 1xZoom
			//      mTouchAF = true;

			/*
			left   = ((left   + 1000) * preview_width)  / 2000;
			top    = ((top    + 1000) * preview_width) / 2000;
			right  = ((right  + 1000) * preview_width)  / 2000;
			bottom = ((bottom + 1000) * preview_height) / 2000;

			CAM_ERROR_PRINTK("%s:AF-2 (%d,%d,%d,%d)", __func__, left, top, right, bottom );

			int width  = right - left;
			int height = bottom - top;

			CAM_ERROR_PRINTK("%s:AF-3(%d,%d,%d,%d,%dx%d)", __func__, left, top, right, bottom, width, height);

			int zoom = 255 - (192 * m_zoom_ratio ) + 1;
			int zoom_w = preview_width * zoom / 256;
			int zoom_h = preview_height * zoom / 256;

			CAM_ERROR_PRINTK("%s:AF-4 zoom=%d zoom_size=%dx%d", __func__, zoom, zoom_w, zoom_h);

			int zoom_left   = (preview_width / 2) - (zoom_w / 2);
			int zoom_top    = (preview_height / 2) - (zoom_h / 2);

			CAM_ERROR_PRINTK("%s:AF-5 B(%d,%d)", __func__, zoom_left, zoom_top);

			left   = zoom_left + (left * zoom / 256);
			top    = zoom_top + (top * zoom / 256);
			right  = left + (width * zoom / 256);
			bottom = top + (height * zoom / 256);
			width  = right - left;
			height = bottom - top;

			CAM_ERROR_PRINTK("%s:AF-6 (%d,%d,%d,%d,%dx%d)", __func__, left, top, right, bottom, width, height);*/

			int xposition = left   * OUTPUT_IMAGE_AREA_W / preview_width;
			int yposition = top    * OUTPUT_IMAGE_AREA_H / preview_height;
			int hsize     = width  * OUTPUT_IMAGE_AREA_W / preview_width;
			int vsize     = height * OUTPUT_IMAGE_AREA_H / preview_height;
			
			CAM_ERROR_PRINTK("%s:AF-7 (%dx%d)", __func__, preview_width, preview_height);
			CAM_ERROR_PRINTK("%s:AF-8 (%d,%d,%dx%d)", __func__, xposition, yposition, hsize, vsize);

			isx012_af_opd4[0].value = xposition + 8 + 41;  // HDELAY
			isx012_af_opd4[1].value = yposition + 5;       // VDELAY
			isx012_af_opd4[2].value = hsize;               // HSIZE
			isx012_af_opd4[3].value = vsize;               // VSIZE

			CAM_ERROR_PRINTK("%s:AF-9 (%d,%d,%d,%d)", __func__, isx012_af_opd4[0].value,isx012_af_opd4[1].value, isx012_af_opd4[2].value, isx012_af_opd4[3].value);

			if ((isx012_af_opd4[0].value + isx012_af_opd4[2].value <= 2641) &&(isx012_af_opd4[1].value + isx012_af_opd4[3].value <= 1949)) {

				err = camdrv_ss_i2c_set_config_register(client,(regs_t *)	isx012_af_opd4,	ARRAY_SIZE(isx012_af_opd4),"af_opd4");
				if (err < 0) {
					
					CAM_ERROR_PRINTK("%s: I2C failed during isx012_af_opd4\n", __func__);
					return -EFAULT;
					}
				err = camdrv_ss_i2c_set_config_register(client,(regs_t *)	isx012_af_window_set,	ARRAY_SIZE(isx012_af_window_set),"af_window_set");
				if (err < 0) {

					CAM_ERROR_PRINTK("%s: I2C failed during isx012_af_window_set\n", __func__);
					return -EFAULT;
					}
				} else {

				CAM_ERROR_PRINTK("ERR(%s):overflow AF Window(w=%d,h=%d)\n", __func__,isx012_af_opd4[0].value + isx012_af_opd4[2].value, isx012_af_opd4[1].value + isx012_af_opd4[3].value);

				return -1;
				}
				}
		}
	else if(touch_af == TOUCH_AF_STOP){
		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_af_window_reset,ARRAY_SIZE(isx012_af_window_reset),"af_window_reset");

		if (err < 0) {

			CAM_ERROR_PRINTK("%s: I2C failed during isx012_af_window_set\n", __func__);
			return -EFAULT;

			}
		}
	return 0;

		
}

static int camdrv_ss_isx012_set_touch_focus(struct v4l2_subdev *sd, enum v4l2_touch_af touch_af, v4l2_touch_area *touch_area)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;
	struct v4l2_ctrl *ctrl =NULL;
	unsigned short usrGain=0;

	mutex_lock(&af_cancel_op);

	CAM_INFO_PRINTK("camdrv_ss_isx012_set_touch_focus E \n");

	first_af_status = false;

	// Initialize fine search value.

	state->bStartFineSearch = false;

	if(touch_af == TOUCH_AF_START) {

		CAM_INFO_PRINTK("camdrv_ss_isx012_set_touch_focus E  %d\n", state->camera_af_flash_checked);

		if (state->current_flash_mode == FLASH_MODE_ON) {
			state->camera_flash_fire = 1;
		}
		else if (state->current_flash_mode == FLASH_MODE_AUTO) {
			bool bflash_needed = false;
			if (camdrv_ss_isx012_check_flash_needed(sd))
				bflash_needed = true;
			else
				CAM_ERROR_PRINTK( "%s : check_flash_needed is NULL !!!\n", __func__);
			if (bflash_needed) {
				state->camera_flash_fire = 1;
			}
		}

		CAM_INFO_PRINTK("%s , Touch AF start\n ",__func__);

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs,ARRAY_SIZE(isx012_focus_mode_auto_regs),"focus_mode_auto_regs");
			if(err < 0){
				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;

			}
		}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs,ARRAY_SIZE(isx012_focus_mode_macro_regs),"focus_mode_macro_regs");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}

		if(set_preflash==true)
			camdrv_ss_isx012_set_preflash_sequence(sd, ctrl);

		
		if(state->camera_flash_fire !=1){
			if (state->currentScene == SCENE_MODE_NIGHTSHOT){
				usrGain = camdrv_ss_isx012_get_usergain_level(sd, client);
				if (usrGain < 0) {
					CAM_ERROR_PRINTK("ERR(%s):get user gain level error\n", __func__);
					return 0;
				}

				if((usrGain & 0xFF) >= 0x3D){
					err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_lowlux_night_halfrelease_mode, 
					ARRAY_SIZE(isx012_lowlux_night_halfrelease_mode),"lowlux_night_halfrelease_mode");
				}
				else{
					err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_halfrelease_mode, ARRAY_SIZE(isx012_halfrelease_mode),"halfrelease_mode");
				}
										if(err < 0){
							CAM_ERROR_PRINTK( "%s : i2c failed !!! \n", __func__);
							mutex_unlock(&af_cancel_op);
							return -EIO;
						}
				msleep(40);
			}
			else if(state->mode_switch == CAMERA_PREVIEW_TO_CAMCORDER_PREVIEW || state->mode_switch == INIT_DONE_TO_CAMCORDER_PREVIEW)
			{
				err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_Camcorder_SAF_mode, ARRAY_SIZE(isx012_Camcorder_SAF_mode),"isx012_Camcorder_SAF_mode");
				if(err < 0){
					CAM_ERROR_PRINTK("%s : i2c failed !!! \n", __func__);
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}

				msleep(40);

			}
			else{
				err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_halfrelease_mode, ARRAY_SIZE(isx012_halfrelease_mode),"halfrelease_mode");
				if(err < 0){
					CAM_ERROR_PRINTK("%s : i2c failed !!! \n", __func__);
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}

				msleep(40);
			}

		}
		else if(state->mode_switch == CAMERA_PREVIEW_TO_CAMCORDER_PREVIEW || state->mode_switch == INIT_DONE_TO_CAMCORDER_PREVIEW)
			{
				err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_Camcorder_SAF_mode, ARRAY_SIZE(isx012_Camcorder_SAF_mode),"isx012_Camcorder_SAF_mode");
				if(err < 0){
					CAM_ERROR_PRINTK("%s : i2c failed !!! \n", __func__);
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}

				msleep(40);

			}
	}
	else if(touch_af == TOUCH_AF_STOP) {

		CAM_INFO_PRINTK("%s :  AUTFOCUS OFF , af_mode : %d\n", __func__, state->af_mode);

		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs_cancel1,ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1),"focus_mode_auto_regs_cancel1");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs_cancel1,ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1),"focus_mode_macro_regs_cancel1");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;

			}
		}
		CAM_INFO_PRINTK("%s , ae unlock\n ",__func__);

		err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_ae_unlock_regs, ARRAY_SIZE(isx012_ae_unlock_regs), "ae_unlock_regs");
		if (state->currentWB== WHITE_BALANCE_AUTO){

			CAM_INFO_PRINTK("%s , awb unlock\n ",__func__);
			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_awb_unlock_regs, ARRAY_SIZE(isx012_awb_unlock_regs), "awb_unlock_regs");
		}
	}

	else if(touch_af == TOUCH_FOCUS_1ST_CANCEL){

		if (state->af_mode == FOCUS_MODE_AUTO){

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs_cancel1, ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1),"focus_mode_auto_regs_cancel1");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}
		else if (state->af_mode == FOCUS_MODE_MACRO){

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs_cancel1,ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1),"focus_mode_macro_regs_cancel1");
			if(err < 0){
				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}
	}
	else if(touch_af == TOUCH_FOCUS_2ND_CANCEL ){
		if (state->af_mode == FOCUS_MODE_AUTO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_auto_regs_cancel1, ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1),"focus_mode_auto_regs_cancel1");
			if(err < 0){

				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}
		else if (state->af_mode == FOCUS_MODE_MACRO) {

			err = camdrv_ss_i2c_set_config_register(client,(regs_t *)isx012_focus_mode_macro_regs_cancel1, ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1),"focus_mode_macro_regs_cancel1");

			if(err < 0){
				CAM_ERROR_PRINTK( "%s : i2c failed in OFF !!! \n", __func__);
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		}
	}
	mutex_unlock(&af_cancel_op);	
	return 0;
}



#if 0
static int camdrv_ss_isx012_get_light_condition(struct v4l2_subdev *sd, int *Result)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);	
    unsigned short read_value1, read_value2;
    int NB_value = 0;

    camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
    camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002C, 0x7000);
    camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002E, 0x2A3C);
    camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &read_value1);   // LSB (0x2A3C)
    camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &read_value2);   // MSB (0x2A3E)

    NB_value = (int)read_value2;
    NB_value = ((NB_value << 16) | (read_value1 & 0xFFFF));
    
    if(NB_value > 0xFFFE)
    {
        *Result = CAM_HIGH_LIGHT;
	    CAM_INFO_PRINTK("%s : Highlight Read(0x%X) \n", __func__, NB_value);
    }
    else if(NB_value > 0x0020)
    {
        *Result = CAM_NORMAL_LIGHT;
	    CAM_INFO_PRINTK("%s : Normallight Read(0x%X) \n", __func__, NB_value);
    }
    else
    {
        *Result = CAM_LOW_LIGHT;
	    CAM_INFO_PRINTK("%s : Lowlight Read(0x%X) \n", __func__, NB_value);
    }

	return 0;
}
#endif


static bool camdrv_ss_isx012_check_flash_needed(struct v4l2_subdev *sd)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);	
	bool retGain, retExptime;
	int ret = 0;
	unsigned short readData=0;

    retGain = false;
	retExptime = false;
    
    readData = camdrv_ss_isx012_get_usergain_level(sd, client);
    if (readData < 0) {
        CAM_ERROR_PRINTK("ERR(%s):get user gain level error\n", __func__);
        return 0;
    }

	switch(ev_value)
    {
    case EV_MINUS_4:
        if((readData & 0xFF) >= GLOWLIGHT_EV_M4) retGain = true;
        break;
    case EV_MINUS_3:
        if((readData & 0xFF) >= GLOWLIGHT_EV_M3) retGain = true;
        break;
    case EV_MINUS_2:
        if((readData & 0xFF) >= GLOWLIGHT_EV_M2) retGain = true;
        break;
    case EV_MINUS_1:
        if((readData & 0xFF) >= GLOWLIGHT_EV_M1) retGain = true;
        break;
    case EV_DEFAULT:
        if((readData & 0xFF) >= GLOWLIGHT_EV_DEFAULT) retGain = true;
        break;
    case EV_PLUS_1:
        if((readData & 0xFF) >= GLOWLIGHT_EV_P1) retGain = true;
        break;
    case EV_PLUS_2:
        if((readData & 0xFF) >= GLOWLIGHT_EV_P2) retGain = true;
        break;
    case EV_PLUS_3:
        if((readData & 0xFF) >= GLOWLIGHT_EV_P3) retGain = true;
        break;
    case EV_PLUS_4:
        if((readData & 0xFF) >= GLOWLIGHT_EV_P4) retGain = true;
        break;
    default:
        CAM_ERROR_PRINTK("ERR(%s):Invalid brightness(%d) ret(%d)", __func__, ev_value, 0);
        return 0;
    };
    
	readData = camdrv_ss_isx012_get_exposureTime(sd);
    if (ret < 0) {
        CAM_ERROR_PRINTK("ERR(%s):get exposure time error\n", __func__);
        return 0;
    }

	switch(iso_value)
    {
    case ISO_AUTO:
         break;
    case ISO_50:
        if((readData & 0xFFFF) >= GLOWLIGHT_ISO50) retExptime = true;
        break;
    case ISO_100:
        if((readData & 0xFFFF) >= GLOWLIGHT_ISO100) retExptime = true;
        break;
    case ISO_200:
        if((readData & 0xFFFF) >= GLOWLIGHT_ISO200) retExptime = true;
        break;
    case ISO_400:
        if((readData & 0xFFFF) >= GLOWLIGHT_ISO400) retExptime = true;
        break;
    case ISO_800:
        break;
    default:
        CAM_ERROR_PRINTK("ERR(%s):Invalid ISO(%d) ret(%d)", __func__, iso_value, 0);
        return 0;
    };

    return (retGain | retExptime);
}
#if 0
static int camdrv_ss_isx012_DS8515_flash_control(struct v4l2_subdev *sd, int control_mode)
{
	CAM_INFO_PRINTK("%s %d :  E\n", __func__, control_mode);
     switch(control_mode)
    {
        // USE FLASH MODE
        case FLASH_CONTROL_MAX_LEVEL:
        {
            gpio_set_value(GPIO_CAM_FLASH_SET, 0);
            gpio_set_value(GPIO_CAM_FLASH_EN, 0);
            udelay(1);
            gpio_set_value(GPIO_CAM_FLASH_EN, 1);
            udelay(2);
            break;
        }
    
        // USE FLASH MODE
        case FLASH_CONTROL_HIGH_LEVEL:
        {
            gpio_set_value(GPIO_CAM_FLASH_SET, 0);
            gpio_set_value(GPIO_CAM_FLASH_EN, 0);
            udelay(1);


            msleep(1);   // Flash Mode Set time

            camdrv_ss_DS8515_flash_write_data(2);
            gpio_set_value(GPIO_CAM_FLASH_SET, 1);
            break;
        }

        // USE MOVIE MODE : AF Pre-Flash Mode(Torch Mode)
        case FLASH_CONTROL_MIDDLE_LEVEL:
	{
                gpio_set_value(GPIO_CAM_FLASH_SET, 0);
            gpio_set_value(GPIO_CAM_FLASH_EN, 0);
            udelay(1);

            camdrv_ss_DS8515_flash_write_data(5);
            gpio_set_value(GPIO_CAM_FLASH_SET, 1);
            break;
        }

        // USE MOVIE MODE : Movie Mode(Torch Mode)
        case FLASH_CONTROL_LOW_LEVEL:
        {
                gpio_set_value(GPIO_CAM_FLASH_SET, 0);
            gpio_set_value(GPIO_CAM_FLASH_EN, 0); 
            udelay(1);

            camdrv_ss_DS8515_flash_write_data(7);   // 69mA
            gpio_set_value(GPIO_CAM_FLASH_SET, 1);
            break;
        }

        case FLASH_CONTROL_OFF:
        default:
        {
                gpio_set_value(GPIO_CAM_FLASH_SET, 0);
            gpio_set_value(GPIO_CAM_FLASH_EN, 0);
            break;
        }        
    }

    return 0;
}

#endif
#if 0
static int camdrv_ss_isx012_MIC2871_flash_control(struct v4l2_subdev *sd, int control_mode)
{
	unsigned long flags;
	spinlock_t lock;
	spin_lock_init(&lock);

	CAM_INFO_PRINTK( "%s %d :  E\n",  __func__, control_mode);
    switch(control_mode)
	{
        // USE FLASH MODE
	case FLASH_CONTROL_MAX_LEVEL:
		{	spin_lock_irqsave(&lock, flags);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
           		camdrv_ss_MIC2871_flash_write_data(5, 1);     /*Setting the safety timer threshold ST_TH(5) to 1(100mA)*/
		//	camdrv_ss_MIC2871_flash_write_data(3, 7); 	/* Setting the safety timer duration register STDUR(3) to 7 (1250msec)*/
           	//	camdrv_ss_MIC2871_flash_write_data(1, 0);     /* write 100%(0) to FEN/FCUR(1) */
			camdrv_ss_MIC2871_flash_write_data(4, 0); 	/* write disable(0) to low battery threshold LB_TH(4) */
		/* enable */
        	//	gpio_set_value(GPIO_CAM_FLASH_EN, 1);
        		camdrv_ss_MIC2871_flash_write_data(1, 16); 
		spin_unlock_irqrestore(&lock, flags);
		break;
	}
        // USE FLASH MODE
	case FLASH_CONTROL_HIGH_LEVEL:
		{	spin_lock_irqsave(&lock, flags);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
			camdrv_ss_MIC2871_flash_write_data(4, 0); 	/* write disable(0) to low battery threshold LB_TH(4) */
		camdrv_ss_MIC2871_flash_write_data(2, 21);
		spin_unlock_irqrestore(&lock, flags);
		break;
	}
        // USE MOVIE MODE : AF Pre-Flash Mode(Torch Mode)
	case FLASH_CONTROL_MIDDLE_LEVEL:
        	{	spin_lock_irqsave(&lock, flags);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
			camdrv_ss_MIC2871_flash_write_data(4, 0);	/* write disable(0) to low battery threshold LB_TH(4) */
		camdrv_ss_MIC2871_flash_write_data(2, 21);
		spin_unlock_irqrestore(&lock, flags);
		break;
	}

        // USE MOVIE MODE : Movie Mode(Torch Mode)
	case FLASH_CONTROL_LOW_LEVEL:
        	{	spin_lock_irqsave(&lock, flags);
		gpio_set_value(GPIO_CAM_FLASH_SET, 1);
			camdrv_ss_MIC2871_flash_write_data(4, 0); 	/* write disable(0) to low battery threshold LB_TH(4) */
		camdrv_ss_MIC2871_flash_write_data(2, 27);
		spin_unlock_irqrestore(&lock, flags);
		break;
	}

	case FLASH_CONTROL_OFF:
	default:
		{	gpio_set_value(GPIO_CAM_FLASH_EN, 0);
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
			udelay(500);
		break;
	}
	}
	return 0;
}

#endif
#if 0
static int camdrv_ss_isx012_AAT_flash_control(struct v4l2_subdev *sd, int control_mode)
{
	CAM_INFO_PRINTK("%s %d :  E\n", __func__, control_mode);
     switch(control_mode)
    {
        // USE FLASH MODE
	case FLASH_CONTROL_MAX_LEVEL:
	{
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		gpio_set_value(GPIO_CAM_FLASH_EN, 0);
		udelay(1);
		gpio_set_value(GPIO_CAM_FLASH_EN, 1);
		break;
	}

        // USE FLASH MODE
	case FLASH_CONTROL_HIGH_LEVEL:
	{
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		gpio_set_value(GPIO_CAM_FLASH_EN, 0);
		udelay(1);

		gpio_set_value(GPIO_CAM_FLASH_EN, 1);
            udelay(10);    // Flash Mode Set time

		camdrv_ss_AAT_flash_write_data(3);
		break;
	}

        // USE MOVIE MODE : AF Pre-Flash Mode(Torch Mode)
	case FLASH_CONTROL_MIDDLE_LEVEL:
	{
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		gpio_set_value(GPIO_CAM_FLASH_EN, 0);
		udelay(1);

		camdrv_ss_AAT_flash_write_data(1);
		break;
	}

        // USE MOVIE MODE : Movie Mode(Torch Mode)
	case FLASH_CONTROL_LOW_LEVEL:
	{
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		gpio_set_value(GPIO_CAM_FLASH_EN, 0);
		udelay(1);

            camdrv_ss_AAT_flash_write_data(7);   // 69mA
		break;
	}

	case FLASH_CONTROL_OFF:
	default:
	{
		gpio_set_value(GPIO_CAM_FLASH_SET, 0);
		gpio_set_value(GPIO_CAM_FLASH_EN, 0);
		break;
	}
	}

	return 0;
}

#endif
int raj=1;
static int camdrv_ss_isx012_sensor_power(struct v4l2_subdev *sd, int on)
{
	unsigned int value;
	int ret = -1;
	struct clk *clock;
	struct clk *lp_clock;
	struct clk *axi_clk;

	CAM_INFO_PRINTK("%s:camera power %s\n", __func__, (on ? "on" : "off"));
if(raj==1)
{
 dump_stack();
 raj=0;
}
	ret = -1;
	lp_clock = clk_get(NULL, CSI0_LP_PERI_CLK_NAME_STR);
	if (IS_ERR_OR_NULL(lp_clock)) {
		CAM_ERROR_PRINTK("%s: Unable to get %s clock\n",__func__,
		CSI0_LP_PERI_CLK_NAME_STR);
		goto e_clk_get;
	}

	clock = clk_get(NULL, SENSOR_0_CLK);
	if (IS_ERR_OR_NULL(clock)) {
		CAM_ERROR_PRINTK("%s: unable to get clock %s\n", __func__, SENSOR_0_CLK);
		goto e_clk_get;
	}

	axi_clk = clk_get(NULL, "csi0_axi_clk");
	if (IS_ERR_OR_NULL(axi_clk)) {
		CAM_ERROR_PRINTK("%s:unable to get clock csi0_axi_clk\n", __func__);
		goto e_clk_get;
	}

	VCAM_A_2_8_V= regulator_get(NULL, VCAM_A_2_8V_REGULATOR);
	if(IS_ERR(VCAM_A_2_8_V))
	{
		CAM_ERROR_PRINTK("%s: can not get VCAM_A_2_8_V.8V\n",__func__);
		return -1;
	}
#ifdef VCAM1_CORE_1_8V_REGULATOR_NEEDED	
	VCAM1_CORE_1_8_V= regulator_get(NULL, VCAM1_CORE_1_8V_REGULATOR);
	if(IS_ERR(VCAM1_CORE_1_8_V))
	{
		CAM_ERROR_PRINTK("%s: can not get VCAM1_CORE_1_8_V\n",__func__);
		return -1;
	}
#endif

        VCAM_IO_1_8_V = regulator_get(NULL, VCAM_IO_1_8V_REGULATOR);
	if(IS_ERR(VCAM_IO_1_8_V))
	{
		CAM_ERROR_PRINTK("%s: can not get VCAM_IO_1.8V\n",__func__);
		return -1;
	}	
	
	VCAM_AF_2_8V = regulator_get(NULL, VCAM_AF_2_8V_REGULATOR);
	if(IS_ERR(VCAM_AF_2_8V))
	{
		CAM_ERROR_PRINTK("%s: can not get VCAM_AF_2_8V\n",__func__);
		return -1;
	}

	VCAM_CORE_1_2_V = regulator_get(NULL, VCAM_CORE_1_2V_REGULATOR);
	if(IS_ERR(VCAM_CORE_1_2_V))
	{
		CAM_ERROR_PRINTK("%s: can not get VCAM_CORE_1_2_V\n",__func__);
		return -1;
	}

//	gpio_request(CAM_AF_EN, "cam_af_2_8v");
//	gpio_direction_output(CAM_AF_EN,0); 
	
	/* CAM_INFO_PRINTK("set cam_rst cam_stnby  to low\n"); */
	//gpio_request(CAM1_RESET, "cam1_rst");
	//gpio_direction_output(CAM1_RESET,0);

	//gpio_request(CAM1_STNBY, "cam1_stnby");
	//gpio_direction_output(CAM1_STNBY,0);

#if 0   // Moved to "if(on) block"
	gpio_request(CAM0_RESET, "cam0_rst");
	gpio_direction_output(CAM0_RESET,0);
	
	gpio_request(CAM0_STNBY, "cam0_stnby");
	gpio_direction_output(CAM0_STNBY,0);
		
	gpio_request(CAM1_RESET, "cam1_rst");
	gpio_direction_output(CAM1_RESET,0);

	gpio_request(CAM1_STNBY, "cam1_stnby");
	gpio_direction_output(CAM1_STNBY,0);
#endif

//	value = ioread32(padctl_base + PADCTRLREG_DCLK1_OFFSET) & (~PADCTRLREG_DCLK1_PINSEL_DCLK1_MASK);
//		iowrite32(value, padctl_base + PADCTRLREG_DCLK1_OFFSET);


	if(on)
	{
		CAM_INFO_PRINTK("%s: power on the sensor and setting GPIO as output \n",__func__); //@HW
		//gpio_direction_output(CAM0_RESET,0);
		//gpio_direction_output(CAM0_STNBY,0);
		
		gpio_request(CAM0_RESET, "cam0_rst");
		gpio_direction_output(CAM0_RESET,0);
	
		gpio_request(CAM0_STNBY, "cam0_stnby");
		gpio_direction_output(CAM0_STNBY,0);
		
		gpio_request(CAM1_RESET, "cam1_rst");
		gpio_direction_output(CAM1_RESET,0);

		gpio_request(CAM1_STNBY, "cam1_stnby");
		gpio_direction_output(CAM1_STNBY,0);
		
		value = regulator_set_voltage(VCAM_CORE_1_2_V, VCAM_CORE_1_2V_REGULATOR_uV, VCAM_CORE_1_2V_REGULATOR_uV);
		if (value)
			CAM_ERROR_PRINTK("%s:regulator_set_voltage VCAM_CORE_1_2_V failed \n", __func__);

		value = regulator_set_voltage(VCAM_A_2_8_V, VCAM_A_2_8V_REGULATOR_uV, VCAM_A_2_8V_REGULATOR_uV);
		if (value)
			CAM_ERROR_PRINTK("%s:regulator_set_voltage VCAM_A_2_8_V failed \n", __func__);
#ifdef VCAM1_CORE_1_8V_REGULATOR_NEEDED
		value = regulator_set_voltage(VCAM1_CORE_1_8_V, VCAM1_CORE_1_8V_REGULATOR_uV, VCAM1_CORE_1_8V_REGULATOR_uV);
		if (value)
			CAM_ERROR_PRINTK("%s:regulator_set_voltage VCAM1_CORE_1_8_V failed \n", __func__);
#endif
		value = regulator_set_voltage(VCAM_IO_1_8_V, VCAM_IO_1_8V_REGULATOR_uV, VCAM_IO_1_8V_REGULATOR_uV);
		if (value)
			CAM_ERROR_PRINTK("%s:regulator_set_voltage VCAM_IO_1_8_V failed \n", __func__);

		value = regulator_set_voltage(VCAM_AF_2_8V, VCAM_AF_2_8V_REGULATOR_uV, VCAM_AF_2_8V_REGULATOR_uV);
		if (value)
			CAM_ERROR_PRINTK("%s:regulator_set_voltage VCAM_AF_2_8V failed \n", __func__);




		if (mm_ccu_set_pll_select(CSI0_BYTE1_PLL, 8)) {
			CAM_ERROR_PRINTK("%s: failed to set BYTE1\n",__func__);
			goto e_clk_pll;
		}
		if (mm_ccu_set_pll_select(CSI0_BYTE0_PLL, 8)) {
			CAM_ERROR_PRINTK("%s: failed to set BYTE0\n",__func__);
			goto e_clk_pll;
		}
		if (mm_ccu_set_pll_select(CSI0_CAMPIX_PLL, 8)) {
			CAM_ERROR_PRINTK("%s: failed to set PIXPLL\n",__func__);
			goto e_clk_pll;
		}

		value = clk_enable(axi_clk);
		if (value) {
			CAM_ERROR_PRINTK("%s: Failed to enable axi clock\n",__func__);
			goto e_clk_axi;
		}

		value = clk_enable(lp_clock);
		if (value) {
			CAM_ERROR_PRINTK("%s: Failed to enable lp clock\n",__func__);
			goto e_clk_lp;
		}

		value = clk_set_rate(lp_clock, CSI0_LP_FREQ);
		if (value) {
			CAM_ERROR_PRINTK("%s: Failed to set lp clock\n",__func__);
			goto e_clk_set_lp;
		}

		value = regulator_enable(VCAM_CORE_1_2_V);
		if (value) {
			CAM_ERROR_PRINTK("%s:failed to enable VCAM_CORE_1_2_V \n", __func__);
			return -1;
		} else
			CAM_ERROR_PRINTK("%s: enabled VCAM_CORE_1_2V \n", __func__);
		msleep(1);

		value = regulator_enable(VCAM_IO_1_8_V);
		if (value) {
			CAM_ERROR_PRINTK("%s:failed to enable VCAM_IO_1_8_V\n", __func__);
			return -1;
		}
		msleep(1);
		
		value = regulator_enable(VCAM_A_2_8_V);
		if (value) {
			CAM_ERROR_PRINTK("%s:failed to enable VCAM_A_2_8_V\n", __func__);
			return -1;
		}
		msleep(1);
#ifdef VCAM1_CORE_1_8V_REGULATOR_NEEDED	
		value = regulator_enable(VCAM1_CORE_1_8_V);
		if (value) {
			CAM_ERROR_PRINTK("%s:failed to enable VCAM1_CORE_1_8_V\n", __func__);
			return -1;
		}
		msleep(1);
#endif
		value = regulator_enable(VCAM_AF_2_8V);
		if (value) {
			CAM_ERROR_PRINTK("%s:failed to enable VCAM_AF_2_8V \n", __func__);
			return -1;
		}
//		gpio_set_value(CAM_AF_EN,1);

		msleep(1);

        	gpio_set_value(CAM1_STNBY,1);
		msleep(2);

		value = clk_enable(clock);
		if (value) {
			CAM_ERROR_PRINTK("%s: Failed to enable sensor 0 clock\n",__func__);
			goto e_clk_sensor;
		}
		
		value = clk_set_rate(clock, SENSOR_0_CLK_FREQ);
		if (value) {
			CAM_ERROR_PRINTK("%s: Failed to set sensor0 clock\n",__func__);
			goto e_clk_set_sensor;
		}
	
		msleep(3);

		gpio_set_value(CAM1_RESET,1);
		msleep(2);
		
		gpio_set_value(CAM1_STNBY,0);
		msleep(2);
		

		gpio_set_value(CAM0_RESET,1);
		msleep(2);

		printk(KERN_ALERT "%s First OM check\n", __func__);
		camdrv_ss_isx012_check_wait_for_mode_transition(sd, 0);

		printk(KERN_ALERT "%s ISX012 PLL Setting\n", __func__);
		camdrv_ss_isx012_pll_setting(sd);

		printk(KERN_ALERT "%s Second OM check\n", __func__);
		camdrv_ss_isx012_check_wait_for_mode_transition(sd, 0);		

		gpio_set_value(CAM0_STNBY,1);
		msleep(1);

		printk(KERN_ALERT "%s Third OM check\n", __func__);
		camdrv_ss_isx012_check_wait_for_mode_transition(sd, 0);	

// Flag is enabled and will be used if camera has flash enabled.		
//#ifdef CONFIG_FLASH_ENABLE
#if 0
				CAM_INFO_PRINTK( "5MP camera ISX012 loaded. HWREV is 0x%x\n", HWREV);

		/*	// FLASH
		if(HWREV >= 0x03)
		{
			ret = gpio_request(GPIO_CAM_FLASH_SET_NEW, "GPJ0[5]");
			if(ret)
			{
				CAM_ERROR_PRINTK( "gpio_request(GPJ0[5]) failed!! \n");
				return 0;
			}
		}
		else*/
		{
			ret = gpio_request(GPIO_CAM_FLASH_SET, "GPJ0[2]");
			if(ret)
			{
				CAM_ERROR_PRINTK( "gpio_request(GPJ0[2]) failed!! \n");
						return 0;
			}
		}

		ret = gpio_request(GPIO_CAM_FLASH_EN, "GPJ0[7]");
		if(ret)
		{
			CAM_ERROR_PRINTK( "gpio_request(GPJ0[7]) failed!! \n");
					return 0;
		}

		/*if(HWREV >= 0x03)
		{
			gpio_direction_output(GPIO_CAM_FLASH_SET_NEW, 0);
		}
		else*/
		{
			gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
		}
		gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
#endif // CONFIG_FLASH_ENABLE
	}
	else
	{
	flagforAE=0;
		CAM_INFO_PRINTK("%s: power off the sensor and setting GPIO as input \n",__func__); //@HW

		gpio_direction_output(CAM0_STNBY,1);
		gpio_set_value(CAM0_STNBY,0);
		msleep(100);

		//camdrv_ss_isx012_check_wait_for_mode_transition(cam_sd, 0);		
		gpio_direction_output(CAM0_RESET,1);
		gpio_set_value(CAM0_RESET, 0);
		msleep(1);
		
		clk_disable(clock);
		clk_disable(lp_clock);
		clk_disable(axi_clk);	
		msleep(1);

		gpio_set_value(CAM1_RESET,0);
		msleep(1);
		
		/* enable power down gpio */

//		gpio_set_value(CAM_AF_EN,0); 
#ifdef VCAM1_CORE_1_8V_REGULATOR_NEEDED
		regulator_disable(VCAM1_CORE_1_8_V);
#endif
		regulator_disable(VCAM_AF_2_8V);
		msleep(1);
		regulator_disable(VCAM_A_2_8_V);
		regulator_disable(VCAM_IO_1_8_V);
		regulator_disable(VCAM_CORE_1_2_V);


//#ifdef CONFIG_FLASH_ENABLE
// Flag is enabled and will be used if camera has flash enabled.		
#if 0
			// FLASH
			// ?? need to do below?
			//camdrv_ss_AAT_flash_control(sd, FLASH_CONTROL_OFF);
			/*
			if(HWREV >= 0x03)
			{					 
				gpio_free(GPIO_CAM_FLASH_SET_NEW);
			}
			else*/
			{
				gpio_free(GPIO_CAM_FLASH_SET);
			}
			gpio_free(GPIO_CAM_FLASH_EN);
#endif // CONFIG_FLASH_ENABLE
/* Backporting Rhea to Hawaii: end */
/*
#ifdef CONFIG_FLASH_ENABLE
camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
		
#endif
*/
		if (!flash_check)
			   {
					   gpio_set_value(GPIO_CAM_FLASH_SET, 0);
				   
					   gpio_set_value(GPIO_CAM_FLASH_EN, 0);
			   
			   }


		CAM_INFO_PRINTK("%s: off success \n",__func__);
	}

	return 0;

e_clk_set_sensor:
	clk_disable(clock);
e_clk_sensor:
e_clk_set_lp:
	clk_disable(lp_clock);
e_clk_lp:
	clk_disable(axi_clk);
e_clk_axi:
e_clk_pll:
e_clk_get:
	return ret;
}


int camdrv_ss_isx012_get_sensor_param_for_exif(
	struct v4l2_subdev *sd,
	struct v4l2_exif_sensor_info *exif_param)
{	char str[20],str1[20];
	int num = -1;
	int ret = -1;
	float exposureTime = 0.0f;
	struct camdrv_ss_state *state = to_state(sd);
//to turn off MAIN flash.
	if((flash_check==0)&&(torch_check==0))
	{
	camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_OFF);
	}
	switch(state->current_flash_mode)
	{
		case FLASH_MODE_ON:
		snprintf(str1, 19, "%d", 9);
		break;
		
		case FLASH_MODE_OFF:
		snprintf(str1, 19, "%d", 0);//strcpy(str1,"OFF");
		break;

		case FLASH_MODE_AUTO:
		if(state->camera_flash_fire)
			
		snprintf(str1, 19, "%d", 25);//strcpy(str1,"AUTO");
		else 
		snprintf(str1, 19, "%d", 24);	
		break;
		
		default:
		strcpy(str1,"");
	}

	strcpy(exif_param->strSoftware,		EXIF_SOFTWARE);
	strcpy(exif_param->strMake,		EXIF_MAKE);
	strcpy(exif_param->strModel,		EXIF_MODEL);

	exposureTime = camdrv_ss_isx012_get_exposureTime(sd);
	num = (int)((1/(exposureTime/1000))*1000);
	//num = (int)exposureTime;
	if (num > 0) 
	{
		snprintf(str, 19, "1/%d", num);
		strcpy(exif_param->exposureTime, str);
	} 
	else 
	{
		strcpy(exif_param->exposureTime, "");
	}
	
	CAM_INFO_PRINTK("%s : exposure time =  %s \n",__func__,exif_param->exposureTime);

	num = camdrv_ss_isx012_get_iso_speed_rate(sd);
	if (num > 0) {
		sprintf(str, "%d,", num);
		strcpy(exif_param->isoSpeedRating, str);
	} else {
		strcpy(exif_param->isoSpeedRating, "");
	}
	CAM_INFO_PRINTK("%s :num=%d and isoSpeedRating =  %s \n",__func__,num, exif_param->isoSpeedRating);

	/* sRGB mandatory field! */
	strcpy(exif_param->colorSpaceInfo,	"1");

	strcpy(exif_param->contrast,		"0");
	strcpy(exif_param->saturation,		"0");
	strcpy(exif_param->sharpness,		"0");

	strcpy(exif_param->FNumber,		(char *)"26/10");
	strcpy(exif_param->exposureProgram,	"2");
	strcpy(exif_param->shutterSpeed,	"");
	strcpy(exif_param->aperture,		"");
	strcpy(exif_param->brightness,		"");
	strcpy(exif_param->exposureBias,	"");
	strcpy(exif_param->maxLensAperture,	(char *)"2757/1000");
	strcpy(exif_param->flash,		str1);
	strcpy(exif_param->lensFocalLength,	(char *)"331/100");
	strcpy(exif_param->userComments,	"User comments");
	ret = 0;

	return ret;

}

bool camdrv_ss_isx012_get_esd_status(struct v4l2_subdev *sd)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);	

	bool bEsdStatus = false;
#if 0
	unsigned short read_value = 0;

//	CAM_INFO_PRINTK("%s = %d \n",__func__,bEsdStatus);

	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002C, 0x7000);
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002E, 0x01A8);
	camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &read_value);

	if (read_value != 0xAAAA) {
		bEsdStatus = true;		//ESD detected
		CAM_ERROR_PRINTK("%s :: ESD detected. 1st read value is 0x%x \n",__func__,read_value);
	}

//	CAM_INFO_PRINTK("%s first detection is ok. readed value is 0x%x \n",__func__,read_value);

	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002C, 0xD000);
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002E, 0x1002);
	camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &read_value);

	if (read_value != 0x0000) {
		bEsdStatus = true;		//ESD detected
		CAM_ERROR_PRINTK("%s :: ESD detected. 2nd read value is 0x%x \n",__func__,read_value);
	}

//	CAM_INFO_PRINTK("%s 2nd detection is ok. readed value is 0x%x \n",__func__,read_value);
#endif
	return bEsdStatus;
}

enum camdrv_ss_capture_mode_state
	camdrv_ss_isx012_get_mode_change(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned short read_value = 0xFFFF;
	int ret = -1;
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0xFCFC, 0xD000);
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002C, 0x7000);
	camdrv_ss_i2c_write_4_bytes_reverse_data(client, 0x002E, 0x215F);
	ret = camdrv_ss_i2c_read_2_bytes(client, 0x0F12, &read_value);
	if (ret < 0) {
		CAM_ERROR_PRINTK("%s: CAMDRV_SS_CAPTURE_MODE_READ_FAILED\n", __func__);
		return CAMDRV_SS_CAPTURE_MODE_READ_FAILED; /* read fail */
	}

	/* CAM_INFO_PRINTK("read mode change value <7000.215Fh>=0x%X\n", read_value); */
	if (read_value == 0x0100) { /* capture mode */
		CAM_INFO_PRINTK(
		"%s: CAPTURE_MODE_READY read_value = 0x%x\n"
		, __func__, read_value);

		return CAMDRV_SS_CAPTURE_MODE_READY;
	} else if (read_value == 0x0000) { /* preview mode */
		CAM_INFO_PRINTK(
		"%s: CAPTURE_MODE_READ_PROCESSING..read_value = 0x%x\n"
		, __func__, read_value);

		return CAMDRV_SS_CAPTURE_MODE_READ_PROCESSING;
	}
	CAM_ERROR_PRINTK(
	"%s: CAPTURE_MODE_READ_FAILED! read_value = 0x%x\n"
	, __func__, read_value);

	return CAMDRV_SS_CAPTURE_MODE_READ_FAILED; /* unknown mode */

}
//New code


int camdrv_ss_isx012_set_preflash_check(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
set_preflash=true;
return 0;
}




int camdrv_ss_isx012_set_preflash_sequence(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{

struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camdrv_ss_state *state = to_state(sd);
	int err = 0;

//         int n=0;//Nikhil
         int ret =0;
        CAM_INFO_PRINTK("camdrv_ss_isx012_set_preflash_sequence E \n");

		#if 0 //temp_lucky
         camdrv_ss_isx012_set_ae_stable_status(sd,0x0000); 	

     //if (state->camera_af_flash_checked == 0) 
    //{

	 #endif
       state->camera_flash_fire = 0;
       if (state->current_flash_mode == FLASH_MODE_ON) {
            state->camera_flash_fire = 1;
       }
       else if (state->current_flash_mode == FLASH_MODE_AUTO) 
       {
          bool bflash_needed = false;
          if (camdrv_ss_isx012_check_flash_needed(sd)){
            	bflash_needed = true;

	} else {
            CAM_ERROR_PRINTK( "%s : check_flash_needed is NULL !!!\n", __func__);
          	}
		  
          if (bflash_needed)
          {
            state->camera_flash_fire = 1;
          }
	}
    //}

	if (state->camera_flash_fire) {

		        // Preview AE scale Read
		ret = camdrv_ss_isx012_get_previewaescale(client, &ae_auto, &ersc_auto);
		if (ret < 0) {
			CAM_ERROR_PRINTK("ERR(%s):get AE scale error\n", __func__);
			return ret;
		}

		err = camdrv_ss_i2c_set_config_register(client,
			(regs_t *)isx012_Pre_Flash_ae_line,
			ARRAY_SIZE(isx012_Pre_Flash_ae_line),
			"Pre_Flash_ae_line");
		if (err < 0) {
			CAM_ERROR_PRINTK("ERR(%s)Couldn't isx012_Pre_Flash_ae_line \n", __FILE__);
		}

		msleep(60); // 60ms


		
		if(state->currentWB == WHITE_BALANCE_AUTO)
		{
		    // AWB_SN1
			ret = camdrv_ss_i2c_write_3_bytes(client, 0x0282, 0x00);
			if (ret != 0) {
	            CAM_ERROR_PRINTK("ERR(%s):i2c write error\n", __func__);
	            return ret;
       		}
		}

        // Flash on Set
		err = camdrv_ss_i2c_set_config_register(client,
			(regs_t *)isx012_Pre_Flash_Start_EVT1,
			ARRAY_SIZE(isx012_Pre_Flash_Start_EVT1),
			"Pre_Flash_Start_EVT1");
		if (err < 0) {
			CAM_ERROR_PRINTK("ERR(%s) Couldn't Set isx012_Pre_Flash_Start_EVT1 \n", __FILE__);
		}

		// Vlatch On 
		ret = camdrv_ss_i2c_write_3_bytes(client, 0x8800, 0x01);
		if (ret != 0) {
            CAM_ERROR_PRINTK("ERR(%s):i2c write error\n", __func__);
            return ret;
		}

		msleep(40); // 40ms
		
		CAM_INFO_PRINTK("(%s)Pre-flash on\n",__func__);
		 camdrv_ss_isx012_flash_control(sd, FLASH_CONTROL_MIDDLE_LEVEL);

		{
			unsigned short  readValue = 0;
			unsigned short  checkValue = 0;
			unsigned int  iRoofOutCnt = 100;

	        // Read MODSEL_FIX
			do {
				mdelay(10);

				ret = camdrv_ss_i2c_read_2_bytes_2(client, 0x0080, &readValue);
				if( ret < 0  )
				{
					CAM_ERROR_PRINTK("ERR(%s) error ret =%d\n", __func__, ret);
					return ret ;
	    		}

				checkValue = (readValue & 0x01); // bit 0
				iRoofOutCnt--;
			}while( (checkValue != 1) && iRoofOutCnt );

			CAM_INFO_PRINTK( "%s() ISX012 Read MODSEL_FIX iRoofOutCnt=%d\n", __func__, iRoofOutCnt);

/*
			// Read HALF_MOVE_STS
		    readValue = 0;
			iRoofOutCnt = 100;
			checkValue = 0;
			do {
				mdelay(10);

				ret = camdrv_ss_i2c_read_2_bytes_2(client, 0x01B0, &readValue);
				if( ret < 0  )
				{
					CAM_ERROR_PRINTK("ERR(%s) error ret =%d\n", __func__, ret);
					return ret;
				}

				checkValue = (readValue & 0x01); // bit 0
				iRoofOutCnt--;
			}while( (checkValue != 0) && iRoofOutCnt );

			CAM_INFO_PRINTK( "%s() ISX012 Read HALF_MOVE_STS iRoofOutCnt=%d\n", __func__, iRoofOutCnt); */
		}
	}
 //state->camera_flash_fire = 0;
 set_preflash=false;
 return 0;
}
void  camdrv_ss_isx012_set_camera_vendorid (char *rear_camera_vendorid)
{
    strcpy(rear_camera_vendorid,"0x0505");
}

//END code
bool camdrv_ss_sensor_functions_isx012(struct camdrv_ss_sensor_cap *sensor)
{

	strcpy(sensor->name,ISX012_NAME);
	sensor->supported_preview_framesize_list  = isx012_supported_preview_framesize_list;
	sensor->supported_number_of_preview_sizes = ARRAY_SIZE(isx012_supported_preview_framesize_list);
	
	sensor->supported_capture_framesize_list  =  isx012_supported_capture_framesize_list;
	sensor->supported_number_of_capture_sizes = ARRAY_SIZE(isx012_supported_capture_framesize_list);
	
	sensor->fmts 				   = isx012_fmts;
	sensor->rows_num_fmts		   =ARRAY_SIZE(isx012_fmts);


	sensor->controls				   =isx012_controls;
	sensor->rows_num_controls	      =ARRAY_SIZE(isx012_controls);
	
	sensor->default_pix_fmt 				   = ISX012_DEFAULT_PIX_FMT;
	sensor->default_mbus_pix_fmt			   = ISX012_DEFAULT_MBUS_PIX_FMT;
	sensor->register_size 		  			 = ISX012_REGISTER_SIZE;
	sensor->skip_frames						 = 2;

	sensor->delay_duration				= ISX012_DELAY_DURATION;

	/* sensor dependent functions */
	
/* mandatory*/
	sensor->thumbnail_ioctl			       = camdrv_ss_isx012_ss_ioctl;
	sensor->enum_frameintervals			   = camdrv_ss_isx012_enum_frameintervals;
	
/*optional*/
	sensor->checkwait_for_mode_transition = camdrv_ss_isx012_check_wait_for_mode_transition;
	sensor->calibrateSensor = camdrv_ss_isx012_calibrateSensor;
	sensor->get_nightmode		   		 = camdrv_ss_isx012_get_nightmode; //aska add
	sensor->set_preview_start      = camdrv_ss_isx012_set_preview_start;//aska add
	sensor->set_capture_start      = camdrv_ss_isx012_set_capture_start;//aska add
	sensor->set_vcm_off            = camdrv_ss_isx012_sensor_off_vcm;   //Ray add

	sensor->set_iso      					 = camdrv_ss_isx012_set_iso;//aska add
	sensor->set_white_balance      = camdrv_ss_isx012_set_white_balance;//aska add
	//sensor->get_ae_stable_status      =  camdrv_ss_isx012_get_ae_stable_status;
	sensor->set_ev      		   = camdrv_ss_isx012_set_ev;
	sensor->set_auto_focus		 	  =  camdrv_ss_isx012_set_auto_focus;
	sensor->get_auto_focus_status     = camdrv_ss_isx012_get_auto_focus_status;

	sensor->set_touch_focus_area 	  =  camdrv_ss_isx012_set_touch_focus_area;
	sensor->set_touch_focus		 	  =  camdrv_ss_isx012_set_touch_focus;
	sensor->get_touch_focus_status     = camdrv_ss_isx012_get_touch_focus_status;
	#ifdef CONFIG_FLASH_MIC2871
	sensor->flash_control			= camdrv_ss_isx012_MIC2871_flash_control;
	#else
	#ifdef CONFIG_FLASH_DS8515
	sensor->flash_control    	   =	camdrv_ss_isx012_DS8515_flash_control;
	#else
	#ifdef CONFIG_FLASH_KTD2692
	sensor->flash_control    	   = camdrv_ss_isx012_KTD2692_flash_control;
	#else
	sensor->flash_control			= camdrv_ss_isx012_AAT_flash_control;
	#endif
	#endif
	#endif
//	sensor->i2c_set_data_burst   	   = camdrv_ss_isx012_i2c_set_data_burst;
	sensor->check_flash_needed   	   = camdrv_ss_isx012_check_flash_needed;
//	sensor->get_light_condition   = camdrv_ss_isx012_get_light_condition;
	
	sensor->sensor_power 			   = camdrv_ss_isx012_sensor_power;

	sensor->get_exif_sensor_info       = camdrv_ss_isx012_get_sensor_param_for_exif;
	sensor->getEsdStatus 			   = camdrv_ss_isx012_get_esd_status;
	/* use 150ms delay or frame skip instead of register read */
	/* sensor->get_mode_change_reg	= camdrv_ss_isx012_get_mode_change; */

	sensor->set_scene_mode 			   = camdrv_ss_isx012_set_scene_mode;   //denis
        sensor->get_prefalsh_on                      =  camdrv_ss_isx012_set_preflash_check; //Nikhil

       sensor->rear_camera_vendorid = camdrv_ss_isx012_set_camera_vendorid; // test
	/*REGS and their sizes*/
	/* List all the capabilities of sensor . List all the supported register setting tables */
	
	sensor->init_regs						  = (regs_t *)isx012_init_regs;
	sensor->rows_num_init_regs				  = ARRAY_SIZE(isx012_init_regs);
	
	sensor->preview_camera_regs 			  = (regs_t *)isx012_preview_camera_regs;
	sensor->rows_num_preview_camera_regs 	  = ARRAY_SIZE(isx012_preview_camera_regs);
			
	/*snapshot mode*/
	sensor->snapshot_normal_regs			  =	(regs_t *)isx012_snapshot_normal_regs;
	sensor->rows_num_snapshot_normal_regs	  = ARRAY_SIZE(isx012_snapshot_normal_regs);

	sensor->snapshot_lowlight_regs			  =	(regs_t *)isx012_snapshot_lowlight_regs;
	sensor->rows_num_snapshot_lowlight_regs	  = ARRAY_SIZE(isx012_snapshot_lowlight_regs);

	sensor->snapshot_highlight_regs			  =	(regs_t *)isx012_snapshot_highlight_regs;
	sensor->rows_num_snapshot_highlight_regs	  = ARRAY_SIZE(isx012_snapshot_highlight_regs);

	sensor->snapshot_nightmode_regs			  =	(regs_t *)isx012_snapshot_nightmode_regs;
	sensor->rows_num_snapshot_nightmode_regs	  = ARRAY_SIZE(isx012_snapshot_nightmode_regs);

	sensor->snapshot_flash_on_regs			  =	(regs_t *)isx012_snapshot_flash_on_regs;
	sensor->rows_num_snapshot_flash_on_regs	  = ARRAY_SIZE(isx012_snapshot_flash_on_regs);

	sensor->snapshot_af_preflash_on_regs			  =	(regs_t *)isx012_snapshot_af_preflash_on_regs;
	sensor->rows_num_snapshot_af_preflash_on_regs	  = ARRAY_SIZE(isx012_snapshot_af_preflash_on_regs);

	sensor->snapshot_af_preflash_off_regs			  =	(regs_t *)isx012_snapshot_af_preflash_off_regs;
	sensor->rows_num_snapshot_af_preflash_off_regs	  = ARRAY_SIZE(isx012_snapshot_af_preflash_off_regs);


	/*effect*/
	sensor->effect_normal_regs			      =	(regs_t *)isx012_effect_normal_regs;
	sensor->rows_num_effect_normal_regs      = ARRAY_SIZE(isx012_effect_normal_regs);
	
	sensor->effect_negative_regs		      =	(regs_t *)isx012_effect_negative_regs;
	sensor->rows_num_effect_negative_regs	 = ARRAY_SIZE(isx012_effect_negative_regs);
	
	sensor->effect_sepia_regs			      =	(regs_t *)isx012_effect_sepia_regs;
	sensor->rows_num_effect_sepia_regs	  	  = ARRAY_SIZE(isx012_effect_sepia_regs);
	
	sensor->effect_mono_regs			      =	(regs_t *)isx012_effect_mono_regs;
	sensor->rows_num_effect_mono_regs	      = ARRAY_SIZE(isx012_effect_mono_regs);

	sensor->effect_aqua_regs				  =	(regs_t *)isx012_effect_aqua_regs;
	sensor->rows_num_effect_aqua_regs	  	  = ARRAY_SIZE(isx012_effect_aqua_regs);
	
	sensor->effect_sharpen_regs 		      =	(regs_t *)isx012_effect_sharpen_regs;
	sensor->rows_num_effect_sharpen_regs     = ARRAY_SIZE(isx012_effect_sharpen_regs);
	
	sensor->effect_solarization_regs		   = (regs_t *)isx012_effect_solarization_regs;
	sensor->rows_num_effect_solarization_regs = ARRAY_SIZE(isx012_effect_solarization_regs);
	
	sensor->effect_black_white_regs 	       = (regs_t *)isx012_effect_black_white_regs;
	sensor->rows_num_effect_black_white_regs  = ARRAY_SIZE(isx012_effect_black_white_regs);
	

	/*wb*/
	sensor->wb_auto_regs				  =	(regs_t *)isx012_wb_auto_regs;
	sensor->rows_num_wb_auto_regs	  	  = ARRAY_SIZE(isx012_wb_auto_regs);

	//sensor->wb_sunny_regs				 =	isx012_wb_sunny_regs;
	//sensor->rows_num_wb_sunny_regs	  	 = ARRAY_SIZE(isx012_wb_sunny_regs);
	
	sensor->wb_cloudy_regs				 =	(regs_t *)isx012_wb_cloudy_regs;
	sensor->rows_num_wb_cloudy_regs	 = ARRAY_SIZE(isx012_wb_cloudy_regs);
	
	//sensor->wb_tungsten_regs			 =	isx012_wb_tungsten_regs;
	//sensor->rows_num_wb_tungsten_regs	 = ARRAY_SIZE(isx012_wb_tungsten_regs);
	//Changed reg table name to fit UI's name
	sensor->wb_daylight_regs				 = (regs_t *)isx012_wb_daylight_regs;
	sensor->rows_num_wb_daylight_regs	  	 = ARRAY_SIZE(isx012_wb_daylight_regs);
	sensor->wb_incandescent_regs				 = (regs_t *)isx012_wb_incandescent_regs;
	sensor->rows_num_wb_incandescent_regs	  	 = ARRAY_SIZE(isx012_wb_incandescent_regs);

	sensor->wb_fluorescent_regs 		  =	(regs_t *)isx012_wb_fluorescent_regs;
	sensor->rows_num_wb_fluorescent_regs  = ARRAY_SIZE(isx012_wb_fluorescent_regs);



	/*metering*/
	sensor->metering_matrix_regs		  =	(regs_t *)isx012_metering_matrix_regs;
	sensor->rows_num_metering_matrix_regs	  	  = ARRAY_SIZE(isx012_metering_matrix_regs);

	sensor->metering_center_regs		  =	(regs_t *)isx012_metering_center_regs;
	sensor->rows_num_metering_center_regs	  	  = ARRAY_SIZE(isx012_metering_center_regs);

	sensor->metering_spot_regs			  = (regs_t *)isx012_metering_spot_regs;
	sensor->rows_num_metering_spot_regs	  		  = ARRAY_SIZE(isx012_metering_spot_regs);
	
	/*EV*/
	sensor->ev_minus_4_regs 			 =	(regs_t *)isx012_ev_minus_4_regs;
	sensor->rows_num_ev_minus_4_regs	 = ARRAY_SIZE(isx012_ev_minus_4_regs);

	sensor->ev_minus_3_regs 			 =	(regs_t *)isx012_ev_minus_3_regs;
	sensor->rows_num_ev_minus_3_regs	 = ARRAY_SIZE(isx012_ev_minus_3_regs);

	sensor->ev_minus_2_regs 			 =	(regs_t *)isx012_ev_minus_2_regs;
	sensor->rows_num_ev_minus_2_regs	  = ARRAY_SIZE(isx012_ev_minus_2_regs);

	sensor->ev_minus_1_regs 			 =	(regs_t *)isx012_ev_minus_1_regs;
	sensor->rows_num_ev_minus_1_regs	 = ARRAY_SIZE(isx012_ev_minus_1_regs);

	sensor->ev_default_regs 			 =	(regs_t *)isx012_ev_default_regs;
	sensor->rows_num_ev_default_regs	 = ARRAY_SIZE(isx012_ev_default_regs);

	sensor->ev_plus_1_regs				 =	(regs_t *)isx012_ev_plus_1_regs;
	sensor->rows_num_ev_plus_1_regs	 = ARRAY_SIZE(isx012_ev_plus_1_regs);

	sensor->ev_plus_2_regs				 =	(regs_t *)isx012_ev_plus_2_regs;
	sensor->rows_num_ev_plus_2_regs	 = ARRAY_SIZE(isx012_ev_plus_2_regs);

	sensor->ev_plus_3_regs				 =	(regs_t *)isx012_ev_plus_3_regs;
	sensor->rows_num_ev_plus_3_regs	 = ARRAY_SIZE(isx012_ev_plus_3_regs);

	sensor->ev_plus_4_regs				 =	(regs_t *)isx012_ev_plus_4_regs;
	sensor->rows_num_ev_plus_4_regs	 = ARRAY_SIZE(isx012_ev_plus_4_regs);

	
	/*contrast*/
	sensor->contrast_minus_2_regs		 	 =	(regs_t *)isx012_contrast_minus_2_regs;
	sensor->rows_num_contrast_minus_2_regs	 = ARRAY_SIZE(isx012_contrast_minus_2_regs);

	sensor->contrast_minus_1_regs		     =	(regs_t *)isx012_contrast_minus_1_regs;
	sensor->rows_num_contrast_minus_1_regs	 = ARRAY_SIZE(isx012_contrast_minus_1_regs);
  
	sensor->contrast_default_regs			 =	(regs_t *)isx012_contrast_default_regs;
	sensor->rows_num_contrast_default_regs  = ARRAY_SIZE(isx012_contrast_default_regs);

	sensor->contrast_plus_1_regs			 =	(regs_t *)isx012_contrast_plus_1_regs;
	sensor->rows_num_contrast_plus_1_regs	 = ARRAY_SIZE(isx012_contrast_plus_1_regs);

	sensor->contrast_plus_2_regs			 =	(regs_t *)isx012_contrast_plus_2_regs;
	sensor->rows_num_contrast_plus_2_regs	 = ARRAY_SIZE(isx012_contrast_plus_2_regs);
	
	/*sharpness*/ 
	sensor->sharpness_minus_3_regs		     =	(regs_t *)isx012_sharpness_minus_3_regs;
	sensor->rows_num_sharpness_minus_3_regs= ARRAY_SIZE(isx012_sharpness_minus_3_regs);

	sensor->sharpness_minus_2_regs		     =	(regs_t *)isx012_sharpness_minus_2_regs;
	sensor->rows_num_sharpness_minus_2_regs= ARRAY_SIZE(isx012_sharpness_minus_2_regs);

	sensor->sharpness_minus_1_regs		 	 =	(regs_t *)isx012_sharpness_minus_1_regs;
	sensor->rows_num_sharpness_minus_1_regs = ARRAY_SIZE(isx012_sharpness_minus_1_regs);

	sensor->sharpness_default_regs		 	 =	(regs_t *)isx012_sharpness_default_regs;
	sensor->rows_num_sharpness_default_regs  = ARRAY_SIZE(isx012_sharpness_default_regs);

	sensor->sharpness_plus_1_regs		     =	(regs_t *)isx012_sharpness_plus_1_regs;
	sensor->rows_num_sharpness_plus_1_regs	 =	ARRAY_SIZE(isx012_sharpness_plus_1_regs);

	sensor->sharpness_plus_2_regs		     =	(regs_t *)isx012_sharpness_plus_2_regs;
	sensor->rows_num_sharpness_plus_2_regs	 =	ARRAY_SIZE(isx012_sharpness_plus_2_regs);
    
	sensor->sharpness_plus_3_regs		     =	(regs_t *)isx012_sharpness_plus_3_regs;
	sensor->rows_num_sharpness_plus_3_regs	 =	ARRAY_SIZE(isx012_sharpness_plus_3_regs);

		
	/*saturation*/
	sensor->saturation_minus_2_regs 	      =	(regs_t *)isx012_saturation_minus_2_regs;
	sensor->rows_num_saturation_minus_2_regs = ARRAY_SIZE(isx012_saturation_minus_2_regs);

	sensor->saturation_minus_1_regs 	 	  =	(regs_t *)isx012_saturation_minus_1_regs;
	sensor->rows_num_saturation_minus_1_regs = ARRAY_SIZE(isx012_saturation_minus_1_regs);

	sensor->saturation_default_regs 	      =	(regs_t *)isx012_saturation_default_regs;
	sensor->rows_num_saturation_default_regs  = ARRAY_SIZE(isx012_saturation_default_regs);

	sensor->saturation_plus_1_regs		       = (regs_t *)isx012_saturation_plus_1_regs;
	sensor->rows_num_saturation_plus_1_regs	= ARRAY_SIZE(isx012_saturation_plus_1_regs);

	sensor->saturation_plus_2_regs		       = (regs_t *)isx012_saturation_plus_2_regs;
	sensor->rows_num_saturation_plus_2_regs   = ARRAY_SIZE(isx012_saturation_plus_2_regs);

	
	/*zoom*/
	sensor->zoom_00_regs					 =	(regs_t *)isx012_zoom_00_regs;
	sensor->rows_num_zoom_00_regs	  		  = ARRAY_SIZE(isx012_zoom_00_regs);

	sensor->zoom_01_regs					 =	(regs_t *)isx012_zoom_01_regs;
	sensor->rows_num_zoom_01_regs	  		  = ARRAY_SIZE(isx012_zoom_01_regs);

	sensor->zoom_02_regs					 =	(regs_t *)isx012_zoom_02_regs;
	sensor->rows_num_zoom_02_regs	  		  = ARRAY_SIZE(isx012_zoom_02_regs);

	sensor->zoom_03_regs					 =	(regs_t *)isx012_zoom_03_regs;
	sensor->rows_num_zoom_03_regs	  		  = ARRAY_SIZE(isx012_zoom_03_regs);

	sensor->zoom_04_regs					 =	(regs_t *)isx012_zoom_04_regs;
	sensor->rows_num_zoom_04_regs	  		  = ARRAY_SIZE(isx012_zoom_04_regs);

	sensor->zoom_05_regs					 =	(regs_t *)isx012_zoom_05_regs;
	sensor->rows_num_zoom_05_regs	  		  = ARRAY_SIZE(isx012_zoom_05_regs);

	sensor->zoom_06_regs					 =	(regs_t *)isx012_zoom_06_regs;
	sensor->rows_num_zoom_06_regs	  		  = ARRAY_SIZE(isx012_zoom_06_regs);

	sensor->zoom_07_regs					 =	(regs_t *)isx012_zoom_07_regs;
	sensor->rows_num_zoom_07_regs	  		  = ARRAY_SIZE(isx012_zoom_07_regs);

	sensor->zoom_08_regs					 =	(regs_t *)isx012_zoom_08_regs;
	sensor->rows_num_zoom_08_regs	  		  = ARRAY_SIZE(isx012_zoom_08_regs);

	
	/*scene mode*/
	sensor->scene_none_regs 			 		= (regs_t *)isx012_scene_none_regs;
	sensor->rows_num_scene_none_regs	  		 = ARRAY_SIZE(isx012_scene_none_regs);

	sensor->scene_portrait_regs 		 		= (regs_t *)isx012_scene_portrait_regs;
	sensor->rows_num_scene_portrait_regs	  	= ARRAY_SIZE(isx012_scene_portrait_regs);

	sensor->scene_nightshot_regs			   = (regs_t *)isx012_scene_nightshot_regs;
	sensor->rows_num_scene_nightshot_regs	  	 = ARRAY_SIZE(isx012_scene_nightshot_regs);

	sensor->scene_backlight_regs			  =	 (regs_t *)isx012_scene_backlight_regs;
	sensor->rows_num_scene_backlight_regs	   = ARRAY_SIZE(isx012_scene_backlight_regs);

	sensor->scene_landscape_regs			   =  (regs_t *)isx012_scene_landscape_regs;
	sensor->rows_num_scene_landscape_regs	  	 = ARRAY_SIZE(isx012_scene_landscape_regs);

	sensor->scene_sports_regs			      =	(regs_t *)isx012_scene_sports_regs;
	sensor->rows_num_scene_sports_regs	  	 = ARRAY_SIZE(isx012_scene_sports_regs);

	sensor->scene_party_indoor_regs 	 	  =	(regs_t *)isx012_scene_party_indoor_regs;
	sensor->rows_num_scene_party_indoor_regs  = ARRAY_SIZE(isx012_scene_party_indoor_regs);

	sensor->scene_beach_snow_regs				 =	(regs_t *)isx012_scene_beach_snow_regs;
	sensor->rows_num_scene_beach_snow_regs	  	 = ARRAY_SIZE(isx012_scene_beach_snow_regs);

	sensor->scene_sunset_regs			 		 =	(regs_t *)isx012_scene_sunset_regs;
	sensor->rows_num_scene_sunset_regs	  		  = ARRAY_SIZE(isx012_scene_sunset_regs);

	sensor->scene_duskdawn_regs 				 =	(regs_t *)isx012_scene_duskdawn_regs;
	sensor->rows_num_scene_duskdawn_regs	  	 = ARRAY_SIZE(isx012_scene_duskdawn_regs);

	sensor->scene_fall_color_regs				 =	(regs_t *)isx012_scene_fall_color_regs;
	sensor->rows_num_scene_fall_color_regs	  	  = ARRAY_SIZE(isx012_scene_fall_color_regs);

	sensor->scene_fireworks_regs				 =	(regs_t *)isx012_scene_fireworks_regs;
	sensor->rows_num_scene_fireworks_regs	  	  = ARRAY_SIZE(isx012_scene_fireworks_regs);
	
	sensor->scene_candle_light_regs 	 		=	(regs_t *)isx012_scene_candle_light_regs;
	sensor->rows_num_scene_candle_light_regs	= ARRAY_SIZE(isx012_scene_candle_light_regs);

	sensor->scene_text_regs			   =	(regs_t *)isx012_scene_text_regs;
	sensor->rows_num_scene_text_regs	  	 = ARRAY_SIZE(isx012_scene_text_regs);

		
	/*fps*/
	sensor->fps_auto_regs				 =	(regs_t *)isx012_fps_auto_regs;
	sensor->rows_num_fps_auto_regs	  		  = ARRAY_SIZE(isx012_fps_auto_regs);

	sensor->fps_5_regs					 =	(regs_t *)isx012_fps_5_regs;
	sensor->rows_num_fps_5_regs	  		  = ARRAY_SIZE(isx012_fps_5_regs);

	sensor->fps_7_regs					 =	(regs_t *)isx012_fps_7_regs;
	sensor->rows_num_fps_7_regs	  		  = ARRAY_SIZE(isx012_fps_7_regs);

	sensor->fps_10_regs 				 =	(regs_t *)isx012_fps_10_regs;
	sensor->rows_num_fps_10_regs	  		  = ARRAY_SIZE(isx012_fps_10_regs);

	sensor->fps_15_regs 				 =	(regs_t *)isx012_fps_15_regs;
	sensor->rows_num_fps_15_regs	  		  = ARRAY_SIZE(isx012_fps_15_regs);

	sensor->fps_20_regs 				 =	(regs_t *)isx012_fps_20_regs;
	sensor->rows_num_fps_20_regs	  		  = ARRAY_SIZE(isx012_fps_20_regs);

	sensor->fps_25_regs 				 =	(regs_t *)isx012_fps_25_regs;
	sensor->rows_num_fps_25_regs	  		  = ARRAY_SIZE(isx012_fps_25_regs);

	sensor->fps_30_regs 				 =	(regs_t *)isx012_fps_30_regs;
	sensor->rows_num_fps_30_regs 		  = ARRAY_SIZE(isx012_fps_30_regs);
	
	sensor->fps_60_regs 				 =	(regs_t *)isx012_fps_60_regs;
	sensor->rows_num_fps_60_regs	  		  = ARRAY_SIZE(isx012_fps_60_regs);

	sensor->fps_120_regs 				 =	(regs_t *)isx012_fps_120_regs;
	sensor->rows_num_fps_120_regs	  		  = ARRAY_SIZE(isx012_fps_120_regs);
	

	
	/*quality*/
	sensor->quality_superfine_regs			 =	(regs_t *)isx012_quality_superfine_regs;
	sensor->rows_num_quality_superfine_regs	  = ARRAY_SIZE(isx012_quality_superfine_regs);

	sensor->quality_fine_regs			 =	(regs_t *)isx012_quality_fine_regs;
	sensor->rows_num_quality_fine_regs	  = ARRAY_SIZE(isx012_quality_fine_regs);

	sensor->quality_normal_regs 		   =	(regs_t *)isx012_quality_normal_regs;
	sensor->rows_num_quality_normal_regs  = ARRAY_SIZE(isx012_effect_normal_regs);

	sensor->quality_economy_regs			 =	(regs_t *)isx012_quality_economy_regs;
	sensor->rows_num_quality_economy_regs   = ARRAY_SIZE(isx012_quality_economy_regs);

	
	/*preview size */
	sensor->preview_size_176x144_regs	        =	(regs_t *)isx012_preview_size_176x144_regs;
	sensor->rows_num_preview_size_176x144_regs	 = ARRAY_SIZE(isx012_preview_size_176x144_regs);

	sensor->preview_size_320x240_regs	         =	(regs_t *)isx012_preview_size_320x240_regs; 
	sensor->rows_num_preview_size_320x240_regs	  = ARRAY_SIZE(isx012_preview_size_320x240_regs);

	sensor->preview_size_352x288_regs	          = (regs_t *)isx012_preview_size_352x288_regs; 
	sensor->rows_num_preview_size_352x288_regs	  = ARRAY_SIZE(isx012_preview_size_352x288_regs);

	sensor->preview_size_640x480_regs	          =	(regs_t *)isx012_preview_size_640x480_regs; 
	sensor->rows_num_preview_size_640x480_regs	  = ARRAY_SIZE(isx012_preview_size_640x480_regs);

	sensor->preview_size_704x576_regs	 		=	(regs_t *)isx012_preview_size_704x576_regs; 
	sensor->rows_num_preview_size_704x576_regs	 = ARRAY_SIZE(isx012_preview_size_704x576_regs);

	sensor->preview_size_720x480_regs	 		=	(regs_t *)isx012_preview_size_720x480_regs; 
	sensor->rows_num_preview_size_720x480_regs	 = ARRAY_SIZE(isx012_preview_size_720x480_regs);
	
	sensor->preview_size_800x480_regs	        =	(regs_t *)isx012_preview_size_800x480_regs;
	sensor->rows_num_preview_size_800x480_regs	 = ARRAY_SIZE(isx012_preview_size_800x480_regs);

	sensor->preview_size_800x600_regs	        =	(regs_t *)isx012_preview_size_800x600_regs;
	sensor->rows_num_preview_size_800x600_regs	 = ARRAY_SIZE(isx012_preview_size_800x600_regs);

	sensor->preview_size_1024x600_regs	         =	(regs_t *)isx012_preview_size_1024x600_regs; 
	sensor->rows_num_preview_size_1024x600_regs	  = ARRAY_SIZE(isx012_preview_size_1024x600_regs);

	sensor->preview_size_1024x768_regs	          =	(regs_t *)isx012_preview_size_1024x768_regs; 
	sensor->rows_num_preview_size_1024x768_regs	  = ARRAY_SIZE(isx012_preview_size_1024x768_regs);

	sensor->HD_Camcorder_regs	          =	(regs_t *)isx012_HD_Camcorder_regs; 
	sensor->rows_num_HD_Camcorder_regs	  = ARRAY_SIZE(isx012_HD_Camcorder_regs);
	
	sensor->HD_Camcorder_Disable_regs	          =	(regs_t *)isx012_HD_Camcorder_Disable_regs; 
	sensor->rows_num_HD_Camcorder_Disable_regs	  = ARRAY_SIZE(isx012_HD_Camcorder_Disable_regs);

	sensor->preview_size_1280x960_regs	          =	(regs_t *)isx012_preview_size_1280x960_regs; 
	sensor->rows_num_preview_size_1280x960_regs	  = ARRAY_SIZE(isx012_preview_size_1280x960_regs);

	sensor->preview_size_1600x960_regs	 		=	(regs_t *)isx012_preview_size_1600x960_regs; 
	sensor->rows_num_preview_size_1600x960_regs	 = ARRAY_SIZE(isx012_preview_size_1600x960_regs);

	sensor->preview_size_1600x1200_regs	 		=	(regs_t *)isx012_preview_size_1600x1200_regs; 
	sensor->rows_num_preview_size_1600x1200_regs	 = ARRAY_SIZE(isx012_preview_size_1600x1200_regs);

	sensor->preview_size_2048x1232_regs	        =	(regs_t *)isx012_preview_size_2048x1232_regs;
	sensor->rows_num_preview_size_2048x1232_regs	 = ARRAY_SIZE(isx012_preview_size_2048x1232_regs);

	sensor->preview_size_2048x1536_regs	         =	(regs_t *)isx012_preview_size_2048x1536_regs; 
	sensor->rows_num_preview_size_2048x1536_regs	  = ARRAY_SIZE(isx012_preview_size_2048x1536_regs);

	sensor->preview_size_2560x1920_regs	          =	(regs_t *)isx012_preview_size_2560x1920_regs; 
	sensor->rows_num_preview_size_2560x1920_regs	  = ARRAY_SIZE(isx012_preview_size_2560x1920_regs);
  
	
	/*Capture size */
	sensor->capture_size_640x480_regs	 		=	(regs_t *)isx012_capture_size_640x480_regs;
	sensor->rows_num_capture_size_640x480_regs	 = ARRAY_SIZE(isx012_capture_size_640x480_regs);

	sensor->capture_size_720x480_regs  			=	(regs_t *)isx012_capture_size_720x480_regs; 
	sensor->rows_num_capture_size_720x480_regs	 = ARRAY_SIZE(isx012_capture_size_720x480_regs);

	sensor->capture_size_800x480_regs	 		=	(regs_t *)isx012_capture_size_800x480_regs;
	sensor->rows_num_capture_size_800x480_regs	 = ARRAY_SIZE(isx012_capture_size_800x480_regs);

	sensor->capture_size_800x486_regs	 		=	(regs_t *)isx012_capture_size_800x486_regs;
	sensor->rows_num_capture_size_800x486_regs	 = ARRAY_SIZE(isx012_capture_size_800x486_regs);

	sensor->capture_size_800x600_regs  			=	(regs_t *)isx012_capture_size_800x600_regs; 
	sensor->rows_num_capture_size_800x600_regs	 = ARRAY_SIZE(isx012_capture_size_800x600_regs);

    	sensor->capture_size_1024x600_regs	 		=	(regs_t *)isx012_capture_size_1024x600_regs;
	sensor->rows_num_capture_size_1024x600_regs	 = ARRAY_SIZE(isx012_capture_size_1024x600_regs);

	sensor->capture_size_1024x768_regs  			=	(regs_t *)isx012_capture_size_1024x768_regs; 
	sensor->rows_num_capture_size_1024x768_regs	 = ARRAY_SIZE(isx012_capture_size_1024x768_regs);

	sensor->capture_size_1280x960_regs  			=	(regs_t *)isx012_capture_size_1280x960_regs; 
	sensor->rows_num_capture_size_1280x960_regs	 = ARRAY_SIZE(isx012_capture_size_1280x960_regs);

    	sensor->capture_size_1600x960_regs	 		=	(regs_t *)isx012_capture_size_1600x960_regs;
	sensor->rows_num_capture_size_1600x960_regs	 = ARRAY_SIZE(isx012_capture_size_1600x960_regs);

	sensor->capture_size_1600x1200_regs  			=	(regs_t *)isx012_capture_size_1600x1200_regs; 
	sensor->rows_num_capture_size_1600x1200_regs	 = ARRAY_SIZE(isx012_capture_size_1600x1200_regs);

	sensor->capture_size_2048x1232_regs  			=	(regs_t *)isx012_capture_size_2048x1232_regs; 
	sensor->rows_num_capture_size_2048x1232_regs	 = ARRAY_SIZE(isx012_capture_size_2048x1232_regs);

	sensor->capture_size_2048x1536_regs  			=	(regs_t *)isx012_capture_size_2048x1536_regs; 
	sensor->rows_num_capture_size_2048x1536_regs	 = ARRAY_SIZE(isx012_capture_size_2048x1536_regs);

	sensor->capture_size_2560x1536_regs  			=	(regs_t *)isx012_capture_size_2560x1536_regs; 
	sensor->rows_num_capture_size_2560x1536_regs	 = ARRAY_SIZE(isx012_capture_size_2560x1536_regs);

	sensor->capture_size_2560x1920_regs  			=	(regs_t *)isx012_capture_size_2560x1920_regs; 
	sensor->rows_num_capture_size_2560x1920_regs	 = ARRAY_SIZE(isx012_capture_size_2560x1920_regs);

	
	/*pattern*/
	sensor->pattern_on_regs 			  = (regs_t *)isx012_pattern_on_regs;
	sensor->rows_num_pattern_on_regs	  = ARRAY_SIZE(isx012_pattern_on_regs);
	
	sensor->pattern_off_regs			  = (regs_t *)isx012_pattern_off_regs;
	sensor->rows_num_pattern_off_regs	  = ARRAY_SIZE(isx012_pattern_off_regs);

	/*AE*/
	sensor->ae_lock_regs			  = (regs_t *)isx012_ae_lock_regs;
	sensor->rows_num_ae_lock_regs	  = ARRAY_SIZE(isx012_ae_lock_regs);

		
	sensor->ae_unlock_regs			  = (regs_t *)isx012_ae_unlock_regs;
	sensor->rows_num_ae_unlock_regs	  = ARRAY_SIZE(isx012_ae_unlock_regs);


	/*AWB*/

	sensor->awb_lock_regs			  = (regs_t *)isx012_awb_lock_regs;
	sensor->rows_num_awb_lock_regs	  = ARRAY_SIZE(isx012_awb_lock_regs);
		
	sensor->awb_unlock_regs			  = (regs_t *)isx012_awb_unlock_regs;
	sensor->rows_num_awb_unlock_regs	  = ARRAY_SIZE(isx012_awb_unlock_regs);
	
	//ISO//
	sensor->iso_auto_regs			  = (regs_t *)isx012_iso_auto_regs;
	sensor->rows_num_iso_auto_regs	  = ARRAY_SIZE(isx012_iso_auto_regs);

	sensor->iso_50_regs			  = (regs_t *)isx012_iso_50_regs;
	sensor->rows_num_iso_50_regs	  = ARRAY_SIZE(isx012_iso_50_regs);

	sensor->iso_100_regs			  = (regs_t *)isx012_iso_100_regs;
	sensor->rows_num_iso_100_regs	  = ARRAY_SIZE(isx012_iso_100_regs);

	sensor->iso_200_regs			  = (regs_t *)isx012_iso_200_regs;
	sensor->rows_num_iso_200_regs	  = ARRAY_SIZE(isx012_iso_200_regs);

	sensor->iso_400_regs			  = (regs_t *)isx012_iso_400_regs;
	sensor->rows_num_iso_400_regs	  = ARRAY_SIZE(isx012_iso_400_regs);

    /* WDR */
	sensor->wdr_on_regs			  = (regs_t *)isx012_wdr_on_regs;
	sensor->rows_num_wdr_on_regs	  = ARRAY_SIZE(isx012_wdr_on_regs);

	sensor->wdr_off_regs			  = (regs_t *)isx012_wdr_off_regs;
	sensor->rows_num_wdr_off_regs	  = ARRAY_SIZE(isx012_wdr_off_regs);


    /* CCD EV */
	sensor->ev_camcorder_minus_4_regs 			 =	(regs_t *)isx012_ev_camcorder_minus_4_regs;
	sensor->rows_num_ev_camcorder_minus_4_regs	 = ARRAY_SIZE(isx012_ev_camcorder_minus_4_regs);

	sensor->ev_camcorder_minus_3_regs 			 =	(regs_t *)isx012_ev_camcorder_minus_3_regs;
	sensor->rows_num_ev_camcorder_minus_3_regs	 = ARRAY_SIZE(isx012_ev_camcorder_minus_3_regs);

	sensor->ev_camcorder_minus_2_regs 			 =	(regs_t *)isx012_ev_camcorder_minus_2_regs;
	sensor->rows_num_ev_camcorder_minus_2_regs	  = ARRAY_SIZE(isx012_ev_camcorder_minus_2_regs);

	sensor->ev_camcorder_minus_1_regs 			 =	(regs_t *)isx012_ev_camcorder_minus_1_regs;
	sensor->rows_num_ev_camcorder_minus_1_regs	 = ARRAY_SIZE(isx012_ev_camcorder_minus_1_regs);

	sensor->ev_camcorder_default_regs 			 =	(regs_t *)isx012_ev_camcorder_default_regs;
	sensor->rows_num_ev_camcorder_default_regs	 = ARRAY_SIZE(isx012_ev_camcorder_default_regs);

	sensor->ev_camcorder_plus_1_regs				 =	(regs_t *)isx012_ev_camcorder_plus_1_regs;
	sensor->rows_num_ev_camcorder_plus_1_regs	 = ARRAY_SIZE(isx012_ev_camcorder_plus_1_regs);

	sensor->ev_camcorder_plus_2_regs				 =	(regs_t *)isx012_ev_camcorder_plus_2_regs;
	sensor->rows_num_ev_camcorder_plus_2_regs	 = ARRAY_SIZE(isx012_ev_camcorder_plus_2_regs);

	sensor->ev_camcorder_plus_3_regs				 =	(regs_t *)isx012_ev_camcorder_plus_3_regs;
	sensor->rows_num_ev_camcorder_plus_3_regs	 = ARRAY_SIZE(isx012_ev_camcorder_plus_3_regs);

	sensor->ev_camcorder_plus_4_regs				 =	(regs_t *)isx012_ev_camcorder_plus_4_regs;
	sensor->rows_num_ev_camcorder_plus_4_regs	 = ARRAY_SIZE(isx012_ev_camcorder_plus_4_regs);


	/* auto contrast */
	sensor->auto_contrast_on_regs				 =	(regs_t *)isx012_auto_contrast_on_regs;
	sensor->rows_num_auto_contrast_on_regs	 = ARRAY_SIZE(isx012_auto_contrast_on_regs);

	sensor->auto_contrast_off_regs				 =	(regs_t *)isx012_auto_contrast_off_regs;
	sensor->rows_num_auto_contrast_off_regs	 = ARRAY_SIZE(isx012_auto_contrast_off_regs);


	/* af return & focus mode */
	sensor->af_return_inf_pos = (regs_t *)isx012_af_return_inf_pos;
	sensor->rows_num_af_return_inf_pos     = ARRAY_SIZE(isx012_af_return_inf_pos);

	sensor->af_return_macro_pos = (regs_t *)isx012_af_return_macro_pos;
	sensor->rows_num_af_return_macro_pos     = ARRAY_SIZE(isx012_af_return_macro_pos);

	sensor->focus_mode_auto_regs = (regs_t *)isx012_focus_mode_auto_regs;
	sensor->rows_num_focus_mode_auto_regs     = ARRAY_SIZE(isx012_focus_mode_auto_regs);

	sensor->focus_mode_macro_regs = (regs_t *)isx012_focus_mode_macro_regs;
	sensor->rows_num_focus_mode_macro_regs     = ARRAY_SIZE(isx012_focus_mode_macro_regs);
	sensor->focus_mode_infinity_regs = (regs_t *)isx012_focus_mode_auto_regs;
	sensor->rows_num_focus_mode_infinity_regs     = ARRAY_SIZE(isx012_focus_mode_auto_regs);

	sensor->vt_mode_regs = (regs_t *)isx012_vt_mode_regs;
	sensor->rows_num_vt_mode_regs     = ARRAY_SIZE(isx012_vt_mode_regs);
        sensor->main_flash_off_regs    =       (regs_t *)isx012_Main_Flash_End_EVT1;
        sensor->rows_num_main_flash_off_regs  = ARRAY_SIZE(isx012_Main_Flash_End_EVT1);
	sensor->Pre_Flash_Start_EVT1 = (regs_t *)isx012_Pre_Flash_Start_EVT1;
	sensor->rows_num_Pre_Flash_Start_EVT1     = ARRAY_SIZE(isx012_Pre_Flash_Start_EVT1);
	sensor->Pre_Flash_End_EVT1 = (regs_t *)isx012_Pre_Flash_End_EVT1;
	sensor->rows_num_Pre_Flash_End_EVT1     = ARRAY_SIZE(isx012_Pre_Flash_End_EVT1);
	sensor->Main_Flash_Start_EVT1 = (regs_t *)isx012_Main_Flash_Start_EVT1;
	sensor->rows_num_Main_Flash_Start_EVT1     = ARRAY_SIZE(isx012_Main_Flash_Start_EVT1);
	sensor->Main_Flash_End_EVT1 = (regs_t *)isx012_Main_Flash_End_EVT1;
	sensor->rows_num_Main_Flash_End_EVT1     = ARRAY_SIZE(isx012_Main_Flash_End_EVT1);
	
	sensor->focus_mode_auto_regs_cancel1 = (regs_t *)isx012_focus_mode_auto_regs_cancel1;
	sensor->rows_num_focus_mode_auto_regs_cancel1     = ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel1);
	sensor->focus_mode_auto_regs_cancel2 = (regs_t *)isx012_focus_mode_auto_regs_cancel2;
	sensor->rows_num_focus_mode_auto_regs_cancel2     = ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel2);
	sensor->focus_mode_auto_regs_cancel3 = (regs_t *)isx012_focus_mode_auto_regs_cancel3;
	sensor->rows_num_focus_mode_auto_regs_cancel3     = ARRAY_SIZE(isx012_focus_mode_auto_regs_cancel3);
	
	 sensor->focus_mode_macro_regs_cancel1 = (regs_t *)isx012_focus_mode_macro_regs_cancel1;
	sensor->rows_num_focus_mode_macro_regs_cancel1     = ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel1);
	sensor->focus_mode_macro_regs_cancel2 = (regs_t *)isx012_Pre_Flash_End_EVT1;
	sensor->rows_num_focus_mode_macro_regs_cancel2     = ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel2);
	sensor->focus_mode_macro_regs_cancel3 = (regs_t *)isx012_focus_mode_macro_regs_cancel2;
	sensor->rows_num_focus_mode_macro_regs_cancel3     = ARRAY_SIZE(isx012_focus_mode_macro_regs_cancel3);
	sensor->af_restart=(regs_t *)isx012_af_restart;
	sensor->rows_num_af_restart=ARRAY_SIZE(isx012_af_restart);
  /* flicker */
	/* sensor->antibanding_50hz_regs	= isx012_antibanding_50hz_regs;
	sensor->rows_num_antibanding_50hz_regs	= ARRAY_SIZE(isx012_antibanding_50hz_regs);        
	sensor->antibanding_60hz_regs	= isx012_antibanding_60hz_regs;
	sensor->rows_num_antibanding_60hz_regs	= ARRAY_SIZE(isx012_antibanding_60hz_regs); */

	return true;
};

int camdrv_ss_read_device_id_isx012(
		struct i2c_client *client, char *device_id)
{
	//int ret = -1;
	/* NEED to WRITE THE I2c REad code to read the deviceid */
	return 0;
}

static int __init camdrv_ss_isx012_mod_init(void)
{
	struct camdrv_ss_sensor_reg sens;

	CAM_INFO_PRINTK("raju \n");
  
	strncpy(sens.name, ISX012_NAME, sizeof(ISX012_NAME));
	sens.sensor_functions = camdrv_ss_sensor_functions_isx012;
	sens.sensor_power = camdrv_ss_isx012_sensor_power;
	sens.read_device_id = camdrv_ss_read_device_id_isx012;
#ifdef CONFIG_SOC_CAMERA_MAIN_ISX012
	sens.isMainSensor = 1;
#endif

#ifdef CONFIG_SOC_CAMERA_SUB_ISX012
	sens.isMainSensor = 0;
#endif
	camdrv_ss_sensors_register(&sens);

	return 0;
}

module_init(camdrv_ss_isx012_mod_init);

MODULE_DESCRIPTION("SAMSUNG CAMERA SENSOR ISX012 ");
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL");
