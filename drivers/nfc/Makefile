#
# Makefile for nfc devices
#

obj-$(CONFIG_PN544_NFC)		+= pn544.o
obj-$(CONFIG_BCM2079X_NFC_I2C)	+= bcm2079x-i2c.o
obj-$(CONFIG_BCM2079X_NFC_SPI)	+= bcm2079x-spi.o
obj-$(CONFIG_SEC_NFC_I2C)	+= sec_nfc.o
obj-$(CONFIG_SEC_NFC_I2C_SB)	+= sec_nfc_sb.o
ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
