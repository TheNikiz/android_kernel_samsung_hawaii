# COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
# All rights are reserved. Reproduction and redistiribution in whole or
# in part is prohibited without the written consent of the copyright owner.
 
# Samsung Variant Makefile
# 
# @author kumhyun.cho@samsung.com
# @since 2014.02.20

obj-$(CONFIG_TRACE_MARK) += base.o
obj-$(CONFIG_TRACE_MARK) += kmem.o
obj-$(CONFIG_TRACE_MARK) += net.o
obj-$(CONFIG_TRACE_MARK) += syscall.o

obj-$(CONFIG_TRACE_MARK_MM_RSS) += mm.o

subdir-ccflags-$(CONFIG_TRACE_MARK) += -Werror
