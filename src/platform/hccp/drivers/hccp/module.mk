LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := hccp_service.bin

LOCAL_SRC_FILES := hccp_service/main.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
                    $(LOCAL_PATH)/../../../libc_sec/include \
                    $(LOCAL_PATH)/../../../inc/driver \
                    $(LOCAL_PATH)/../../../inc/tdt \
                    $(LOCAL_PATH)/../../../inc/network
LOCAL_C_INCLUDES+=  $(TOPDIR)inc/toolchain $(TOPDIR)drivers/network/include \
                    $(TOPDIR)libc_sec/include \
                    $(TOPDIR)drivers/network/hccp/rdma_service \
                    $(TOPDIR)drivers/network/hccp/rdma_agent/adapter \
                    $(TOPDIR)drivers/network/hccp/rdma_agent/hdc \
                    $(TOPDIR)drivers/network/hccp/rdma_agent/inc

LOCAL_SHARED_LIBRARIES := libc_sec libslog libra_adp librs libibverbs libascend_hal libtsdppc libascend_protobuf libaicpu_processer libhns-rdmav17
LOCAL_STATIC_LIBRARIES := libcrypto
LOCAL_CFLAGS += -Werror -Wall -fPIC -DDEBUG_SHOW_EXECUTE_TIME=\"$(DEBUG_SHOW_EXECUTE_TIME)\" -rdynamic -fvisibility=hidden -Dgoogle=ascend_private
LOCAL_LDFLAGS +=  -ldl -lra_adp -lrs -libverbs
LOCAL_LDFLAGS +=  -Wl,-Bsymbolic
include $(BUILD_EXECUTABLE)

