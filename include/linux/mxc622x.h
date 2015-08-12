/*
 * Copyright (C) 2012 MEMSIC, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef	__MXC622X_H__
#define	__MXC622X_H__

#include <linux/ioctl.h>
#include <linux/input.h>

#ifndef DEBUG
#define DEBUG
#endif

#define	MXC622X_ACC_IOCTL_BASE 77
/** The following define the IOCTL command values via the ioctl macros */
#define	MXC622X_ACC_IOCTL_SET_DELAY _IOW(MXC622X_ACC_IOCTL_BASE, 0, int)
#define	MXC622X_ACC_IOCTL_GET_DELAY _IOR(MXC622X_ACC_IOCTL_BASE, 1, int)
#define	MXC622X_ACC_IOCTL_SET_ENABLE _IOW(MXC622X_ACC_IOCTL_BASE, 2, int)
#define	MXC622X_ACC_IOCTL_GET_ENABLE _IOR(MXC622X_ACC_IOCTL_BASE, 3, int)
#define	MXC622X_ACC_IOCTL_GET_COOR_XYZ _IOW(MXC622X_ACC_IOCTL_BASE, 22, int)
#define	MXC622X_ACC_IOCTL_GET_CHIP_ID _IOR(MXC622X_ACC_IOCTL_BASE, 255, \
					char[32])

#define MXC622X_ACC_DEV_NAME "mxc622x"
#define MXC622X_ACC_INPUT_NAME "accelerometer"
#define MXC622X_ACC_I2C_ADDR 0x15
#define MXC622X_ACC_I2C_NAME MXC622X_ACC_DEV_NAME
#define MXC622X_ACC_CHIPID 0x25
/*CONTROL REGISTERS*/
/* MXC622X register address */
#define MXC622X_REG_CTRL 0x04
#define MXC622X_REG_DATA 0x00
#define MXC622X_REG_CHIPID 0x08	/*ID register*/
#define MXC622X_REG_XOUT (0x00)
#define MXC622X_REG_YOUT (0x01)
#define MXC622X_REG_STATUS (0x02)
#define MXC622X_REG_DETECTION (0x04)

/* MXC622X control bit */
#define MXC622X_CTRL_PWRON 0x00	/*power on*/
#define MXC622X_CTRL_PWRDN 0x80	/*power down*/

/*#define I2C_BUS_NUM_STATIC_ALLOC*/
#define I2C_STATIC_BUS_NUM (2)

#ifdef	__KERNEL__

enum mxc622x_orientation {
	MXC622X_ORIE_10_10,
	MXC622X_ORIE_01_01,
	MXC622X_ORIE_10_01,
	MXC622X_ORIE_01_10,
};
struct mxc622x_acc_platform_data {
	int poll_interval;
	int min_interval;
	int orientation;
	int (*init) (void);
	void (*exit) (void);
	int (*power_on) (void);
	int (*power_off) (void);

};
#endif /* __KERNEL__ */

#endif /* __MXC622X_H__ */
