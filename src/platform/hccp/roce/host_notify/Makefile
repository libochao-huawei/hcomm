# SPDX-License-Identifier: GPL-2.0+
#
# Makefile for the HISILICON network.
#
ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

export CONFIG_HOST_CDEV = m

ccflags-y += -Wall -Werror

obj-$(CONFIG_HOST_CDEV):= host_notify.o

host_notify-objs := host_roce_cdev.o host_roce_notify.o host_roce_devmm.o



