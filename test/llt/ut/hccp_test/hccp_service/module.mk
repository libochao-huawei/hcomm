LOCAL_PATH 		:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	hccp_utest

LOCAL_LDFLAGS	+= 	-lrt
LOCAL_CLASSFILE_RULE := network


#MAIN Function
LOCAL_SRC_FILES	:= main.cc

LOCAL_SRC_FILES += stub/tsd.c stub/mm.c stub/hccp_pub.c


SEARCH_SRC_FILES:= ut_dispatch.cc
LOCAL_SRC_FILES += $(addprefix ../../stub/, $(SEARCH_SRC_FILES))

PATH_BRIDGE             := ../../../../../drivers/network
HCCP_PATH       = $(PATH_BRIDGE)/hccp/hccp_service
LOCAL_SRC_FILES += $(HCCP_PATH)/main.c

# test_case
LOCAL_SRC_FILES += st_test.cc tc_hccp.c
				

#第三方头文件搜索路径
LOCAL_C_INCLUDES+= 	$(LOCAL_PATH)/stub \
					$(TOPDIR)inc \
					$(TOPDIR)inc/driver \
					$(TOPDIR)inc/network \
					$(LOCAL_PATH)/../../stub/include	\
					$(LOCAL_PATH)/../../ut_include 	\
					$(LOCAL_PATH)/../stub/ \
					$(TOPDIR)abl/libc_sec/include \
					$(TOPDIR)drivers/network/hccp/rdma_agent/adapter \
					$(TOPDIR)drivers/network/hccp/rdma_agent/hdc \
					$(TOPDIR)drivers/network/hccp/rdma_agent/inc \
					$(TOPDIR)drivers/network/hccp/rdma_service \
					$(TOPDIR)drivers/network/include

LOCAL_CFLAGS += -U_FORTIFY_SOURCE -O0 -DHNS_ROCE_LLT -g -DUSE_DISPATCH_EXPECT -DCONFIG_HCCP_LLT
LOCAL_STATIC_LIBRARIES:= 

include $(BUILD_UT_TEST)
