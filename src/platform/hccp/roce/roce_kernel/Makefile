# SPDX-License-Identifier: GPL-2.0+
#
# Makefile for the HISILICON network device drivers.
#
ifeq ($(TOP_DIR),)
KERNEL_DIR := $(srctree)

EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../kernel/vendor/bbox/include
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/network/hns
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/network/hns/hns3pf
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/network/hns/hnae3
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_space/src/tsdrv/ts_drv/ts_drv_host
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_space/src/tsdrv/ts_drv/ts_drv_common
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/network/inc/private/network
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../libc_sec/include
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../abl/bbox/inc/bbox/device/
EXTRA_CFLAGS += -I$(KERNEL_DIR)/../../drivers/ai_sdk/arc/linux/kernel_space/inc/

else

EXTRA_CFLAGS += -I$(TOP_DIR)/kernel/vendor/bbox/include
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/network/hns
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/network/hns/hns3pf
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/network/hns/hnae3
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/tsdrv/ts_drv/ts_drv_host
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/src/tsdrv/ts_drv/ts_drv_common
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/network/inc/private/network
EXTRA_CFLAGS += -I$(TOP_DIR)/libc_sec/include
EXTRA_CFLAGS += -I$(TOP_DIR)/abl/bbox/inc/bbox/device/
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/inc/
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_open/src/kernel_adapt/include

endif

EXTRA_CFLAGS += -DCONFIG_INFINIBAND_HNS_DFX -DCONFIG_HNS3_TEST -DHNS_ROCE_DEVICE

ccflags-y += -fstack-protector-strong
ccflags-y += -fstack-protector-all
ccflags-y += -Wall -Werror
obj-m += hns_roce.o

hns_roce-objs := hns_roce_ah.o         \
            hns_roce_alloc.o        \
            hns_roce_cmd.o          \
            hns_roce_cq.o           \
            hns_roce_db.o           \
            hns_roce_hem.o          \
            hns_roce_hw_v2_qp.o     \
            hns_roce_hw_v2_eq.o     \
            hns_roce_hw_v2_ops.o    \
            hns_roce_hw_v2.o        \
            hns_roce_hw_sysfs_v2.o  \
            hns_roce_hw_v2_dfx.o    \
            hns_roce_sysfs.o        \
            hns_roce_main.o         \
            hns_roce_mr.o           \
            hns_roce_pd.o           \
            hns_roce_qp.o           \
            hns_roce_srq.o          \
            hns_roce_peer.o         \
            hns_roce_sm.o           \
            hns_roce_restrack.o     \
            hns_roce_notify.o       \
            hns_roce_cdev.o
