
LOCAL_PATH := $(call my-dir)

ifneq ($(fpga),true)
export hns_esl_platform=true
endif
ifneq ($(build_device),true)
#build host roce_kernel
include $(CLEAR_VARS)

LOCAL_MODULE := hns_roce
LOCAL_DEPEND_KO := hnae3 hns3 drv_seclib_host

#LOCAL_MODULE_CLASS

LOCAL_MODULE_SUFFIX := .ko

#LOCAL_SONAME_SUFFIX

include $(LOCAL_PATH)/roce_kernel_rule_host.mk

else
#build device roce_kernel
include $(CLEAR_VARS)

LOCAL_MODULE := hns_roce

#LOCAL_MODULE_CLASS
LOCAL_IS_DEVICE_MODULE := true

LOCAL_MODULE_SUFFIX := .ko

#LOCAL_SONAME_SUFFIX 

include $(LOCAL_PATH)/roce_kernel_rule_device.mk
endif
