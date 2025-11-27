LOCAL_PATH 		:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	rdma_agent_stest

LOCAL_LDFLAGS	+= 	-lrt
LOCAL_CLASSFILE_RULE := network


#MAIN Function
LOCAL_SRC_FILES	:= main.cc

LOCAL_SRC_FILES += stub/hdc.c stub/rs.c stub/rs_ping.c stub/sec.c stub/devmm_api.c stub/dl.c stub/devdrv_runtime_api.c


SEARCH_SRC_FILES:= ut_dispatch.cc
LOCAL_SRC_FILES += $(addprefix ../../stub/, $(SEARCH_SRC_FILES))

SEARCH_SRC_FILES:= $(wildcard $(TOPDIR)drivers/network/hccp/rdma_agent/client/*.c)
SEARCH_SRC_FILES:= $(notdir $(SEARCH_SRC_FILES))
LOCAL_SRC_FILES += $(addprefix ../../../../../drivers/network/hccp/rdma_agent/client/, $(SEARCH_SRC_FILES))

SEARCH_SRC_FILES:= $(wildcard $(TOPDIR)drivers/network/hccp/rdma_agent/adapter/*.c)
SEARCH_SRC_FILES:= $(notdir $(SEARCH_SRC_FILES))
LOCAL_SRC_FILES += $(addprefix ../../../../../drivers/network/hccp/rdma_agent/adapter/, $(SEARCH_SRC_FILES))

SEARCH_SRC_FILES:= $(wildcard $(TOPDIR)drivers/network/hccp/rdma_agent/hdc/*.c)
SEARCH_SRC_FILES:= $(notdir $(SEARCH_SRC_FILES))
LOCAL_SRC_FILES += $(addprefix ../../../../../drivers/network/hccp/rdma_agent/hdc/, $(SEARCH_SRC_FILES))

SEARCH_SRC_FILES:= $(wildcard $(TOPDIR)drivers/network/hccp/rdma_agent/peer/*.c)
SEARCH_SRC_FILES:= $(notdir $(SEARCH_SRC_FILES))
IO_ROOT_DIR := $(TOPDIR)third_party
LOCAL_C_INCLUDES+= $(IO_ROOT_DIR)/ofed/build/rdma-core/include
LOCAL_SRC_FILES += $(addprefix ../../../../../drivers/network/hccp/rdma_agent/peer/, $(SEARCH_SRC_FILES))

SEARCH_SRC_FILES:= $(wildcard $(TOPDIR)drivers/network/hccp/rdma_agent/comm/*.c)
SEARCH_SRC_FILES:= $(notdir $(SEARCH_SRC_FILES))
LOCAL_SRC_FILES += $(addprefix ../../../../../drivers/network/hccp/rdma_agent/comm/, $(SEARCH_SRC_FILES))

# test_case
LOCAL_SRC_FILES += st_test.cc tc_rdma_agent.c tc_ra_ping.c
				

#第三方头文件搜索路径
LOCAL_C_INCLUDES+= 	$(LOCAL_PATH)/stub \
					$(TOPDIR)inc \
					$(TOPDIR)inc/driver \
					$(TOPDIR)inc/network \
					$(TOPDIR)drivers/network/hccp/rdma_agent/client \
					$(TOPDIR)drivers/network/hccp/rdma_agent/adapter \
					$(TOPDIR)drivers/network/hccp/rdma_agent/hdc \
					$(TOPDIR)drivers/network/hccp/rdma_agent/peer \
					$(LOCAL_PATH)/../../stub/include	\
					$(LOCAL_PATH)/../../ut_include 	\
					$(LOCAL_PATH)/../stub/ \
					$(TOPDIR)abl/libc_sec/include \
					$(TOPDIR)drivers/network/hccp/rdma_agent/inc \
					$(TOPDIR)drivers/network/hccp/rdma_agent/comm \
					$(TOPDIR)drivers/network/hccp/rdma_service \
					$(TOPDIR)drivers/network/include
					
LOCAL_CFLAGS += -U_FORTIFY_SOURCE -O0 -DHNS_ROCE_LLT -g -DUSE_DISPATCH_EXPECT -DCONFIG_SSL

LOCAL_STATIC_LIBRARIES:= 

include $(BUILD_UT_TEST)
