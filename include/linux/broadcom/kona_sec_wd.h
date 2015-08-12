/*************************************************************************
 * * Copyright 2010  Broadcom Corporation
 * *
 * * Unless you and Broadcom execute a separate written software license
 * * agreement governing use of this software, this software is licensed to you
 * * under the terms of the GNU General Public License version 2 (the GPL),
 * * available at http://www.broadcom.com/licenses/GPLv2.php, with the following
 * * added to such license:
 * * As a special exception, the copyright holders of this software give you
 * * permission to link this software with independent modules, and to copy and
 * * distribute the resulting executable under terms of your choice, provided that
 * * you also meet, for each linked independent module, the terms and conditions
 * * of the license of that module. An independent module is a module which is not
 * * derived from this software.  The special exception does not apply to any
 * * modifications of the software.
 * * Notwithstanding the above, under no circumstances may you combine this
 * * software in any way with any other Broadcom software provided under a license
 * * other than the GPL, without Broadcom's express prior written consent.
 * *****************************************************************************/
/*
 *  * linux/broadcom/wd-tapper.h
 *   *
 *    * Watchdog Tapper for PMU.
 *     */

#ifndef __KONA_SEC_WD_H
#define __KONA_SEC_WD_H

unsigned int sec_wd_activate(void);
void sec_wd_suspend(void);
void sec_wd_resume(void);
void sec_wd_enable(void);
void sec_wd_disable(void);
unsigned int is_sec_wd_enabled(void);

#endif
