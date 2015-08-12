/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/video/broadcom/displays/HX8369.h
*
* Unless you and Broadcom execute a separate DISPCTRL_WRitten software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior DISPCTRL_WRitten consent.
*******************************************************************************/

/****************************************************************************
*
*  HX8369.h
*
*  PURPOSE:
*    This is the LCD-specific code for a HX8369 module.
*
*****************************************************************************/

/* ---- Include Files ---------------------------------------------------- */
#ifndef __HX8369_BOE_H__
#define __HX8369_BOE_H__

#include "display_drv.h"
#include "lcd.h"

//  LCD command definitions
#define PIXEL_FORMAT_RGB565	0x05   // for 16 bits
#define PIXEL_FORMAT_RGB666	0x06   // for 18 bits
#define PIXEL_FORMAT_RGB888	0x07   // for 24 bits

#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS	160
#define DEFAULT_GAMMA_LEVEL	14/*180cd*/
#define MAX_GAMMA_VALUE 	24
#define MAX_GAMMA_LEVEL		25

#define DIM_BL 20
#define MIN_BL 30
#define MAX_BL 255

#define GAMMA_INDEX_NOT_SET	0xFFFF

typedef enum {
	HX8369_CMD_BOE_NOP					= 0x00,
	HX8369_CMD_BOE_SLEEP_IN				= 0x10,		
	HX8369_CMD_BOE_SLEEP_OUT				= 0x11,	
	HX8369_CMD_BOE_DISP_ON				= 0x29,
	HX8369_CMD_BOE_DISP_OFF				= 0x28,
	HX8369_CMD_BOE_PIXEL					= 0x3A,
	HX8369_CMD_BOE_DISP_DIRECTION		= 0x36,	
	HX8369_CMD_BOE_PWR_CTL				= 0xB1,
	HX8369_CMD_BOE_RGB_SET				= 0xB3,	
	HX8369_CMD_BOE_DISP_INVERSION		= 0xB4,	
	HX8369_CMD_BOE_BGP_VOL				= 0xB5,
	HX8369_CMD_BOE_VCOM					= 0xB6,		
	HX8369_CMD_BOE_ENABLE_CMD			= 0xB9,
	HX8369_CMD_BOE_MIPI_CMD				= 0xBA,
	HX8369_CMD_BOE_GOA_CTL				= 0xD5,
	HX8369_CMD_BOE_SRC_OPT				= 0xC0,		
	HX8369_CMD_BOE_SRC_DGC_OFF			= 0xC1,			
	HX8369_CMD_BOE_SRC					= 0xC6,	
	HX8369_CMD_BOE_PANEL					= 0xCC,	
	HX8369_CMD_BOE_GAMMA					= 0xE0,	
	HX8369_CMD_BOE_EQ						= 0xE3,	
	HX8369_CMD_BOE_MESSI					= 0xEA,
	HX8369_CMD_BOE_BL					= 0x53,
} HX8369_CMD_BOE_T;

__initdata struct DSI_COUNTER hx8369_boe_timing[] = {
	/* LP Data Symbol Rate Calc - MUST BE FIRST RECORD */
	{"ESC2LP_RATIO", DSI_C_TIME_ESC2LPDT, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0x0000003F, 1, 1},
	/* SPEC:  min =  100[us] + 0[UI] */
	/* SET:   min = 1000[us] + 0[UI]                             <= */
	{"HS_INIT", DSI_C_TIME_HS, 0,
		0, 100000, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0, 0},
	/* SPEC:  min = 1[ms] + 0[UI] */
	/* SET:   min = 1[ms] + 0[UI] */
	{"HS_WAKEUP", DSI_C_TIME_HS, 0,
		0, 1000000, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 0, 0},
	/* SPEC:  min = 1[ms] + 0[UI] */
	/* SET:   min = 1[ms] + 0[UI] */
	{"LP_WAKEUP", DSI_C_TIME_ESC, 0,
		0, 1000000, 0, 0, 0, 0, 0, 0, 0x00FFFFFF, 1, 1},
	/* SPEC:  min = 0[ns] +  8[UI] */
	/* SET:   min = 0[ns] + 12[UI]                               <= */
	{"HS_CLK_PRE", DSI_C_TIME_HS, 0,
		0, 0, 12, 0, 0, 0, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min = 38[ns] + 0[UI]   max= 95[ns] + 0[UI] */
	/* SET:   min = 48[ns] + 0[UI]   max= 95[ns] + 0[UI]         <= */
	{"HS_CLK_PREPARE", DSI_C_TIME_HS, DSI_C_HAS_MAX,
		0, 48, 0, 0, 0, 95, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min = 262[ns] + 0[UI] */
	/* SET:   min = 262[ns] + 0[UI]                              <= */
	{"HS_CLK_ZERO", DSI_C_TIME_HS, 0,
		0, 320, 0, 0, 0, 0, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min =  60[ns] + 52[UI] */
	/* SET:   min =  70[ns] + 52[UI]                             <= */
	{"HS_CLK_POST", DSI_C_TIME_HS, 0,
		0, 70, 52, 0, 0, 0, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min =  60[ns] + 0[UI] */
	/* SET:   min =  70[ns] + 0[UI]                              <= */
	{"HS_CLK_TRAIL", DSI_C_TIME_HS, 0,
		0, 70, 0, 0, 0, 0, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min =  50[ns] + 0[UI] */
	/* SET:   min =  60[ns] + 0[UI]                              <= */
	{"HS_LPX", DSI_C_TIME_HS, 0,
		0, 60, 0, 0, 0, 75, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min = 40[ns] + 4[UI]      max= 85[ns] + 6[UI] */
	/* SET:   min = 50[ns] + 4[UI]      max= 85[ns] + 6[UI]      <= */
	{"HS_PRE", DSI_C_TIME_HS, DSI_C_HAS_MAX,
		0, 50, 4, 0, 0, 85, 6, 0, 0x000001FF, 0, 0},
	/* SPEC:  min = 105[ns] + 6[UI] */
	/* SET:   min = 105[ns] + 6[UI]                              <= */
	{"HS_ZERO", DSI_C_TIME_HS, 0,
		0, 105, 6, 0, 0, 0, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min = max(0[ns]+32[UI],60[ns]+16[UI])  n=4 */
	/* SET:   min = max(0[ns]+32[UI],60[ns]+16[UI])  n=4 */
	{"HS_TRAIL", DSI_C_TIME_HS, DSI_C_MIN_MAX_OF_2,
		0, 60, 4, 60, 16, 0, 0, 0, 0x000001FF, 0, 0},
	/* SPEC:  min = 100[ns] + 0[UI] */
	/* SET:   min = 110[ns] + 0[UI]                              <= */
	{"HS_EXIT", DSI_C_TIME_HS, 0,
		0, 110, 0, 0, 0, 0, 0, 0, 0x000001FF, 0, 0},
	/* min = 50[ns] + 0[UI] */
	/* LP esc counters are speced in LP LPX units.
	   LP_LPX is calculated by chal_dsi_set_timing
	   and equals LP data clock */
	{"LPX", DSI_C_TIME_ESC, 0,
		1, 0, 0, 0, 0, 0, 0, 0, 0x000000FF, 1, 1},
	/* min = 4*[Tlpx]  max = 4[Tlpx], set to 4 */
	{"LP-TA-GO", DSI_C_TIME_ESC, 0,
		4, 0, 0, 0, 0, 0, 0, 0, 0x000000FF, 1, 1},
	/* min = 1*[Tlpx]  max = 2[Tlpx], set to 2 */
	{"LP-TA-SURE", DSI_C_TIME_ESC, 0,
		2, 0, 0, 0, 0, 0, 0, 0, 0x000000FF, 1, 1},
	/* min = 5*[Tlpx]  max = 5[Tlpx], set to 5 */
	{"LP-TA-GET", DSI_C_TIME_ESC, 0,
		5, 0, 0, 0, 0, 0, 0, 0, 0x000000FF, 1, 1},
};

__initdata DISPCTRL_REC_T hx8369_boe_scrn_on[] = {
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_DISP_ON},
	{DISPCTRL_SLEEP_MS, 100},
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_BL},
	{DISPCTRL_WR_DATA, 0x2C},
	{DISPCTRL_LIST_END, 0}
};

__initdata DISPCTRL_REC_T hx8369_boe_scrn_off[] = {
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_BL},
	{DISPCTRL_WR_DATA, 0x08},
	{DISPCTRL_SLEEP_MS, 50},
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_DISP_OFF},
	{DISPCTRL_SLEEP_MS, 50},	
	{DISPCTRL_LIST_END, 0}
};

__initdata DISPCTRL_REC_T hx8369_boe_id[] = {
	{DISPCTRL_WR_CMND, 0xDA},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_CMND, 0xDB},
	{DISPCTRL_WR_DATA, 0x80},
	{DISPCTRL_WR_CMND, 0xDC},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_LIST_END, 0}
};


__initdata DISPCTRL_REC_T hx8369_boe_slp_in[] = {
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_SLEEP_IN},
	{DISPCTRL_LIST_END, 0}
};

__initdata DISPCTRL_REC_T hx8369_boe_slp_out[] = {
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_SLEEP_OUT},
	{DISPCTRL_SLEEP_MS, 150},
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_DISP_ON},
	{DISPCTRL_SLEEP_MS, 50},
	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_BL},
	{DISPCTRL_WR_DATA, 0x24},
	{DISPCTRL_LIST_END, 0}
};

__initdata DISPCTRL_REC_T hx8369_boe_init_panel_vid[] = {
	 {DISPCTRL_WR_CMND, HX8369_CMD_BOE_ENABLE_CMD},
	{DISPCTRL_WR_DATA, 0xFF},		
	{DISPCTRL_WR_DATA, 0x83},
	{DISPCTRL_WR_DATA, 0x69},
	
	{DISPCTRL_WR_CMND, 0xB1},
	{DISPCTRL_WR_DATA, 0x0B},
	{DISPCTRL_WR_DATA, 0x83},
	{DISPCTRL_WR_DATA, 0x77},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x11},
	{DISPCTRL_WR_DATA, 0x11},
	{DISPCTRL_WR_DATA, 0x08},
	{DISPCTRL_WR_DATA, 0x08},	
	{DISPCTRL_WR_DATA, 0x0C},
	{DISPCTRL_WR_DATA, 0x22},
	
	{DISPCTRL_WR_CMND, 0xC6},
	{DISPCTRL_WR_DATA, 0x41},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0x7A},
	{DISPCTRL_WR_DATA, 0xFF},

	{DISPCTRL_WR_CMND, 0xE3},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_CMND, 0xC0},
	{DISPCTRL_WR_DATA, 0x73},
	{DISPCTRL_WR_DATA, 0x50},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x34},
	{DISPCTRL_WR_DATA, 0xC4},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_SLEEP_MS, 120},

	{DISPCTRL_WR_CMND, 0xBA},
	{DISPCTRL_WR_DATA, 0x31},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x16},
	{DISPCTRL_WR_DATA, 0xCA},	
	{DISPCTRL_WR_DATA, 0xB0},
	{DISPCTRL_WR_DATA, 0x0A},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x10},	
	{DISPCTRL_WR_DATA, 0x28},
	{DISPCTRL_WR_DATA, 0x02},
	
	{DISPCTRL_WR_DATA, 0x21},
	{DISPCTRL_WR_DATA, 0x21},
	{DISPCTRL_WR_DATA, 0x9A},	
	{DISPCTRL_WR_DATA, 0x1A},
	{DISPCTRL_WR_DATA, 0x8F},

	{DISPCTRL_WR_CMND, 0x3A},
	{DISPCTRL_WR_DATA, 0x70},

	{DISPCTRL_WR_CMND, 0xB3},
	{DISPCTRL_WR_DATA, 0x83},
	{DISPCTRL_WR_DATA, 0x06},
	{DISPCTRL_WR_DATA, 0x31},
	{DISPCTRL_WR_DATA, 0x03},
	{DISPCTRL_WR_DATA, 0x13},
	{DISPCTRL_WR_DATA, 0x06},
	 

	{DISPCTRL_WR_CMND, 0xB4},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_CMND, 0xCC},
	{DISPCTRL_WR_DATA, 0x0C},

	{DISPCTRL_WR_CMND, 0xEA},
	{DISPCTRL_WR_DATA, 0x72},

	{DISPCTRL_WR_CMND, 0xB2},
	{DISPCTRL_WR_DATA, 0x03},
		
	{DISPCTRL_WR_CMND, 0xD5},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x0D},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x12},
	{DISPCTRL_WR_DATA, 0x40},
	{DISPCTRL_WR_DATA, 0x00},	
	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x01},
	{DISPCTRL_WR_DATA, 0x60},	
	{DISPCTRL_WR_DATA, 0x37},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x0F},
	{DISPCTRL_WR_DATA, 0x01},
	{DISPCTRL_WR_DATA, 0x05},	
	
	{DISPCTRL_WR_DATA, 0x47},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x03},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x03},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x18},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x89},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_DATA, 0x11},
	{DISPCTRL_WR_DATA, 0x33},
	{DISPCTRL_WR_DATA, 0x55},
	{DISPCTRL_WR_DATA, 0x77},	
	{DISPCTRL_WR_DATA, 0x31},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x98},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x66},

	{DISPCTRL_WR_DATA, 0x44},
	{DISPCTRL_WR_DATA, 0x22},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x02},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x89},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x22},	

	{DISPCTRL_WR_DATA, 0x44},
	{DISPCTRL_WR_DATA, 0x66},
	{DISPCTRL_WR_DATA, 0x20},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x98},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x77},  //72
	{DISPCTRL_WR_DATA, 0x55},
	{DISPCTRL_WR_DATA, 0x33},	

	{DISPCTRL_WR_DATA, 0x11},
	{DISPCTRL_WR_DATA, 0x13},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x01},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x03},	

	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0xCF},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0xFF},	
	{DISPCTRL_WR_DATA, 0x03},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0xCF},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_DATA, 0x8C},
	{DISPCTRL_WR_DATA, 0x5A},	

	{DISPCTRL_WR_CMND, 0xE0},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x02},
	{DISPCTRL_WR_DATA, 0x0D},	
	{DISPCTRL_WR_DATA, 0x0C},
	{DISPCTRL_WR_DATA, 0x3F},
	{DISPCTRL_WR_DATA, 0x19},
	{DISPCTRL_WR_DATA, 0x2D},
	{DISPCTRL_WR_DATA, 0x04},
	{DISPCTRL_WR_DATA, 0x0F},	
	
	{DISPCTRL_WR_DATA, 0x0E},
	{DISPCTRL_WR_DATA, 0x14},
	{DISPCTRL_WR_DATA, 0x17},
	{DISPCTRL_WR_DATA, 0x15},	
	{DISPCTRL_WR_DATA, 0x16},
	{DISPCTRL_WR_DATA, 0x10},
	{DISPCTRL_WR_DATA, 0x11},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x02},	
	
	{DISPCTRL_WR_DATA, 0x0C},
	{DISPCTRL_WR_DATA, 0x12},
	{DISPCTRL_WR_DATA, 0x3F},
	{DISPCTRL_WR_DATA, 0x19},	
	{DISPCTRL_WR_DATA, 0x2C},
	{DISPCTRL_WR_DATA, 0x05},
	{DISPCTRL_WR_DATA, 0x08},
	{DISPCTRL_WR_DATA, 0x0E},
	{DISPCTRL_WR_DATA, 0x12},
	{DISPCTRL_WR_DATA, 0x16},
	
	{DISPCTRL_WR_DATA, 0x14},
	{DISPCTRL_WR_DATA, 0x15},
	{DISPCTRL_WR_DATA, 0x10},
	{DISPCTRL_WR_DATA, 0x12},
	{DISPCTRL_WR_DATA, 0x01},

	{DISPCTRL_WR_CMND, 0xC1},
	{DISPCTRL_WR_DATA, 0x01},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x08},
	{DISPCTRL_WR_DATA, 0x10},	
	{DISPCTRL_WR_DATA, 0x18},
	{DISPCTRL_WR_DATA, 0x1F},
	{DISPCTRL_WR_DATA, 0x27},
	{DISPCTRL_WR_DATA, 0x2E},
	{DISPCTRL_WR_DATA, 0x34},
	{DISPCTRL_WR_DATA, 0x3E},	
	
	{DISPCTRL_WR_DATA, 0x48},
	{DISPCTRL_WR_DATA, 0x50},
	{DISPCTRL_WR_DATA, 0x58},
	{DISPCTRL_WR_DATA, 0x60},	
	{DISPCTRL_WR_DATA, 0x68},
	{DISPCTRL_WR_DATA, 0x70},
	{DISPCTRL_WR_DATA, 0x78},
	{DISPCTRL_WR_DATA, 0x80},
	{DISPCTRL_WR_DATA, 0x88},
	{DISPCTRL_WR_DATA, 0x90},	
	
	{DISPCTRL_WR_DATA, 0x98},
	{DISPCTRL_WR_DATA, 0xA0},
	{DISPCTRL_WR_DATA, 0xA8},
	{DISPCTRL_WR_DATA, 0xB0},	
	{DISPCTRL_WR_DATA, 0xB8},
	{DISPCTRL_WR_DATA, 0xC0},
	{DISPCTRL_WR_DATA, 0xC8},
	{DISPCTRL_WR_DATA, 0xD0},
	{DISPCTRL_WR_DATA, 0xD8},
	{DISPCTRL_WR_DATA, 0xE0},
	
	{DISPCTRL_WR_DATA, 0xE8},
	{DISPCTRL_WR_DATA, 0xF0},
	{DISPCTRL_WR_DATA, 0xF7},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x08},
	{DISPCTRL_WR_DATA, 0x10},
	{DISPCTRL_WR_DATA, 0x18},
	{DISPCTRL_WR_DATA, 0x1F},
	{DISPCTRL_WR_DATA, 0x27},
	{DISPCTRL_WR_DATA, 0x2E},	
	
	{DISPCTRL_WR_DATA, 0x34},
	{DISPCTRL_WR_DATA, 0x3E},
	{DISPCTRL_WR_DATA, 0x48},
	{DISPCTRL_WR_DATA, 0x50},	
	{DISPCTRL_WR_DATA, 0x58},
	{DISPCTRL_WR_DATA, 0x60},
	{DISPCTRL_WR_DATA, 0x68},
	{DISPCTRL_WR_DATA, 0x70},
	{DISPCTRL_WR_DATA, 0x78},
	{DISPCTRL_WR_DATA, 0x80},	
	
	{DISPCTRL_WR_DATA, 0x88},
	{DISPCTRL_WR_DATA, 0x90},
	{DISPCTRL_WR_DATA, 0x98},
	{DISPCTRL_WR_DATA, 0xA0},	
	{DISPCTRL_WR_DATA, 0xA8},
	{DISPCTRL_WR_DATA, 0xB0},
	{DISPCTRL_WR_DATA, 0xB8},
	{DISPCTRL_WR_DATA, 0xC0},
	{DISPCTRL_WR_DATA, 0xC8},
	{DISPCTRL_WR_DATA, 0xD0},
	
	{DISPCTRL_WR_DATA, 0xD8},
	{DISPCTRL_WR_DATA, 0xE0},
	{DISPCTRL_WR_DATA, 0xE8},
	{DISPCTRL_WR_DATA, 0xF0},
	{DISPCTRL_WR_DATA, 0xF7},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},	
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x08},
	{DISPCTRL_WR_DATA, 0x10},
	{DISPCTRL_WR_DATA, 0x18},
	{DISPCTRL_WR_DATA, 0x1F},	
	
	{DISPCTRL_WR_DATA, 0x27},
	{DISPCTRL_WR_DATA, 0x2E},
	{DISPCTRL_WR_DATA, 0x34},
	{DISPCTRL_WR_DATA, 0x3E},	
	{DISPCTRL_WR_DATA, 0x48},
	{DISPCTRL_WR_DATA, 0x50},
	{DISPCTRL_WR_DATA, 0x58},
	{DISPCTRL_WR_DATA, 0x60},
	{DISPCTRL_WR_DATA, 0x68},
	{DISPCTRL_WR_DATA, 0x70},	
	
	{DISPCTRL_WR_DATA, 0x78},
	{DISPCTRL_WR_DATA, 0x80},
	{DISPCTRL_WR_DATA, 0x88},
	{DISPCTRL_WR_DATA, 0x90},	
	{DISPCTRL_WR_DATA, 0x98},
	{DISPCTRL_WR_DATA, 0xA0},
	{DISPCTRL_WR_DATA, 0xA8},
	{DISPCTRL_WR_DATA, 0xB0},
	{DISPCTRL_WR_DATA, 0xB8},
	{DISPCTRL_WR_DATA, 0xC0},
	
	{DISPCTRL_WR_DATA, 0xC8},
	{DISPCTRL_WR_DATA, 0xD0},
	{DISPCTRL_WR_DATA, 0xD8},
	{DISPCTRL_WR_DATA, 0xE0},
	{DISPCTRL_WR_DATA, 0xE8},
	{DISPCTRL_WR_DATA, 0xF0},
	{DISPCTRL_WR_DATA, 0xF7},
	{DISPCTRL_WR_DATA, 0xFF},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},
	{DISPCTRL_WR_DATA, 0x00},

	{DISPCTRL_WR_CMND, 0xC9}, //Temporary BL control on lcd initialization
	{DISPCTRL_WR_DATA, 0x0F},
	{DISPCTRL_WR_DATA, 0x00},	

	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_SLEEP_OUT},
	{DISPCTRL_SLEEP_MS, 150},

	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_DISP_ON},
	{DISPCTRL_SLEEP_MS, 50},


	{DISPCTRL_WR_CMND, HX8369_CMD_BOE_BL},
	{DISPCTRL_WR_DATA, 0x2C},

	{DISPCTRL_LIST_END, 0},
};

void hx8369_boe_winset(char *msgData, DISPDRV_WIN_t *p_win)
{
	return;
}

__initdata struct lcd_config hx8369_boe_cfg = {
	.name = "HX8369_BOE",
	.mode_supp = LCD_VID_ONLY,
	.phy_timing = &hx8369_boe_timing[0],
	.max_lanes = 2,
	.max_hs_bps = 500000000,
	.max_lp_bps = 9500000,
	.phys_width = 55,
	.phys_height = 99,
	.init_cmd_seq = NULL,
	.init_vid_seq = &hx8369_boe_init_panel_vid[0],
	.slp_in_seq = &hx8369_boe_slp_in[0],
	.slp_out_seq = &hx8369_boe_slp_out[0],
	.scrn_on_seq = &hx8369_boe_scrn_on[0],
	.scrn_off_seq = &hx8369_boe_scrn_off[0],
	.id_seq = &hx8369_boe_id[0],
	.verify_id = false,	
	.updt_win_fn = hx8369_boe_winset,
	.updt_win_seq_len = 0,
	.vid_cmnds = false,
	.vburst = true,
	.cont_clk = false,
	.hs = 65,
	.hbp = 55,
	.hfp = 215,
	.vs = 4,
	.vbp = 19,
	.vfp = 6,
};

#endif /* __HX8369_H__ */

