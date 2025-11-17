LOCAL_PATH := $(call my-dir)

#compile for host

include $(CLEAR_VARS)

LOCAL_MODULE := host_notify

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)

LOCAL_INSTALLED_KO_FILES := host_notify.ko

LOCAL_CFLAGS += -Werror

include $(BUILD_HOST_KO)

