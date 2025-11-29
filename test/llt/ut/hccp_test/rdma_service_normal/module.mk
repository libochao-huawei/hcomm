LOCAL_PATH 		:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	rs_ut_normal

LOCAL_LDFLAGS	+= 	-lrt
LOCAL_CLASSFILE_RULE := network

ROCE_USER_SRC_DIR := $(LOCAL_PATH)/../../../../../drivers/network/hccp/rdma_service/
HNS_SRC_DIR := 

#MAIN Function
LOCAL_SRC_FILES	:= main.cc

LOCAL_SRC_FILES += stub/cmd.c stub/init.c stub/device.c stub/mm.c stub/ssl.c stub/drv_usr_cfg.c

LOCAL_SRC_FILES += ../../stub/crypto/encrypt.c  ../../stub/crypto/kmc_crypt.c
LOCAL_SRC_FILES+= ../../stub/roce_user/stub_hns_roce_u_cmd.c

SEARCH_SRC_FILES:= ut_dispatch.cc
LOCAL_SRC_FILES += $(addprefix ../../stub/, $(SEARCH_SRC_FILES))

SEARCH_SRC_FILES:= $(wildcard $(TOPDIR)drivers/network/hccp/rdma_service/*.c)

SEARCH_SRC_FILES:= $(notdir $(SEARCH_SRC_FILES))
LOCAL_SRC_FILES += $(addprefix ../../../../../drivers/network/hccp/rdma_service/, $(SEARCH_SRC_FILES))

# test_case
LOCAL_SRC_FILES += ut_test.cc tc_ut_rs.c tc_ut_rs_ping.c
				
IO_ROOT_DIR := $(TOPDIR)third_party

#第三方头文件搜索路径
LOCAL_C_INCLUDES+= 	$(LOCAL_PATH)/include	\
					$(TOPDIR)inc \
					$(TOPDIR)inc/driver \
					$(TOPDIR)inc/network \
					$(TOPDIR)abl/libc_sec/include	\
					$(LOCAL_PATH)/../../stub/include \
					$(LOCAL_PATH)/../../ut_include 	\
					$(LOCAL_PATH)/../stub/	\
					$(LOCAL_PATH)/stub \
					$(TOPDIR)drivers/network/include \
					$(TOPDIR)drivers/network/crypt/inc \
					$(LOCAL_PATH)/../../stub/roce_user \
					${TOP_DIR}/drivers/network/common \
					$(ROCE_USER_SRC_DIR)

LOCAL_CFLAGS += -U_FORTIFY_SOURCE -O0 -DHNS_ROCE_LLT -g -DUSE_DISPATCH_EXPECT -DCA_CONFIG_LLT -DDEVICE_TLS_CONFIG -DCONFIG_SSL -DCONFIG_RDMA -DRS_DEVICE_CONFIG

LOCAL_STATIC_LIBRARIES:= 

include $(BUILD_UT_TEST)
