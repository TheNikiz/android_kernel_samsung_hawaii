#ifndef __ARM_ARCH_DEV_MUIC_TSU6111_H
#define __ARM_ARCH_DEV_MUIC_TSU6111_H

extern struct platform_device hawaii_bcmbt_rfkill_device;
extern struct i2c_board_info micro_usb_i2c_devices_info[];

void __init dev_muic_tsu6111_init(void);

extern void uas_jig_force_sleep_tsu6111(void);
extern int bcm_ext_bc_status_tsu6111(void);
extern unsigned int musb_get_charger_type_tsu6111(void);
extern void musb_vbus_changed_tsu6111(int state);
#endif