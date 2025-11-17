include $(BUILD_SYSTEM)/base_rules.mk

ROCE_KERNEL_DEVICE_OUT_TIMESTAMP := $(DEVICE_OUT_THIRD_PARTY_LIBS)/roce_kernel/.timestamp

ROCE_KERNEL_DEVICE_SRC_DIR := drivers/network/roce/roce_kernel

ROCE_COMPILE_ARCH := \
$(strip $(if $(shell uname -a | grep x86_64),x86_64,\
    $(if $(shell uname -a | grep aarch64),arm64,others)))

OFED_CONFIG_DEVICE_PATH := $(ofed_config_adk_path)/$(notdir $(DEVICE_KERNEL_PATH))

$(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP): PRIVATE_SRC_DIR := $(ROCE_KERNEL_DEVICE_SRC_DIR)
ifneq (,$(wildcard $(PWD)/$($(DEVICE_DEFAULT_COMPILER)_TOOLS_PREFIX)gcc))
ROCE_KERNEL_DEVICE_CROSS_COMPILE_PREFIX_DEVICE := $(PWD)/$($(DEVICE_DEFAULT_COMPILER)_TOOLS_PREFIX)
else
ROCE_KERNEL_DEVICE_CROSS_COMPILE_PREFIX_DEVICE := $($(DEVICE_DEFAULT_COMPILER)_TOOLS_PREFIX)
endif

LOCAL_DEPEND_VALID_KO := $(addprefix $(DEVICE_OUT_ROOT)/, $(addsuffix $(LOCAL_MODULE_SUFFIX), $(LOCAL_DEPEND_KO)))

$(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP): PRIVATE_KERNEL_DIR := $(PWD)/$(DEVICE_KERNEL_PATH)
$(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP): PRIVATE_KERNEL_CONFIG_FILE := $(notdir $(DEVICE_KERNEL_DEFCONFIG))
$(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP): PRIVATE_THIRDPARTY := $(PWD)/$(DEVICE_OUT_THIRD_PARTY_LIBS)/roce_kernel/SRPMS/compat-rdma-4.17
$(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP): PRIVATE_OFED_CONFIG_PATH := $(PWD)/$(OFED_CONFIG_DEVICE_PATH)
$(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP): $(DEVICE_KERNEL_PATH)
	mkdir -p $(dir $@) && \
	cp -rf third_party/ofed/* $(PWD)/$(dir $@) && cd $(PWD)/$(dir $@)SRPMS && rpm2cpio compat-rdma-4.17-1.1.ge153426.src.rpm | cpio -id && tar xf compat-rdma-4.17.tgz && \
	cd $(PWD)/$(dir $@)SRPMS/compat-rdma-4.17 && patch -p2 < ../../huawei_patches/001-compat-rdma-4.17.patch && echo "insert patch succ!" && \
	cd $(PWD)/$(dir $@)SRPMS/compat-rdma-4.17 && mkdir -p tmp && \
	cp -rf $(PWD)/ofed_adk/config.h $(PRIVATE_THIRDPARTY)/compat/ &&\
	make -C $(DEVICE_KERNEL_PATH) O=$(PWD)/$(dir $@)SRPMS/compat-rdma-4.17/tmp ARCH=arm64 TARGET_CHIP_ID=$(TARGET_CHIP_ID) CROSS_COMPILE=$(ROCE_KERNEL_DEVICE_CROSS_COMPILE_PREFIX_DEVICE) $(PRIVATE_KERNEL_CONFIG_FILE); \
	cd $(PWD)/$(dir $@)SRPMS/compat-rdma-4.17/tmp &&\
	make -C $(DEVICE_KERNEL_PATH) O=$(PWD)/$(dir $@)SRPMS/compat-rdma-4.17/tmp ARCH=arm64 TARGET_CHIP_ID=$(TARGET_CHIP_ID) CROSS_COMPILE=$(ROCE_KERNEL_DEVICE_CROSS_COMPILE_PREFIX_DEVICE) modules_prepare; \
	cp -rf $(PWD)/$(PRIVATE_SRC_DIR) $(PWD)/$(dir $@) &&  \
	cp -rf $(PWD)/$(dir $@)SRPMS/compat-rdma-4.17/tmp  $(PWD)/$(dir $@)roce_kernel &&\
	cd $(PWD)/$(dir $@)roce_kernel &&\
	make TOP_DIR=$(PWD) ARCH=arm64 hns_esl_platform=false PRODUCT=ascend910B CROSS_COMPILE=$(ROCE_KERNEL_DEVICE_CROSS_COMPILE_PREFIX_DEVICE) THIRDPARTY=$(PRIVATE_THIRDPARTY)
	touch $@

$(LOCAL_BUILT_MODULE): $(ROCE_KERNEL_DEVICE_OUT_TIMESTAMP)
	mkdir -p $(dir $@)
	cp -rf $(dir $<)roce_kernel/hns_roce.ko $@
