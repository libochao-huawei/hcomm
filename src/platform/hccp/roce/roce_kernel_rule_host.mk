include $(BUILD_SYSTEM)/base_rules.mk

ROCE_KERNEL_OUT_TIMESTAMP := $(HOST_OUT_THIRD_PARTY_LIBS)/roce_kernel/.timestamp

ROCE_KERNEL_SRC_DIR := drivers/network/roce/roce_kernel

#RDMA_CORE_CROSS_COMPILE_PREFIX_HOST := $(if $(wildcard $($(HOST_DEFAULT_COMPILER)_TOOLS_PREFIX)gcc),$(PWD)/$($(HOST_DEFAULT_COMPILER)_TOOLS_PREFIX),)


ifeq ($(HOST_KERNEL_PATH),kernel/linux-3.10-euler2.5)
PRIVATE_KERNEL_SYMBOLS_PATH := $(HOST_KERNEL_PATH)/Module.symvers
else
ifeq ($(HOST_KERNEL_PATH),kernel/linux-4.19-arm-euleros2.8)
PRIVATE_KERNEL_SYMBOLS_PATH := $(HOST_KERNEL_PATH)/README
else
PRIVATE_KERNEL_SYMBOLS_PATH := $(HOST_KERNEL_PATH)/README
endif
endif

OFED_CONFIG_PATH := $(ofed_config_adk_path)/$(notdir $(HOST_KERNEL_PATH))

ifeq ($(HOST_COMPILE_ARCH),arm64)
ROCE_KERNEL_ARCH := arm64
RDMA_CORE_CROSS_COMPILE_PREFIX_HOST := $(if $(wildcard $($(HOST_DEFAULT_COMPILER)_TOOLS_PREFIX)gcc),$(PWD)/$($(HOST_DEFAULT_COMPILER)_TOOLS_PREFIX),)
else
ROCE_KERNEL_ARCH := x86
RDMA_CORE_CROSS_COMPILE_PREFIX_HOST := 
endif

KBUILD_EXTRA_SYMBOLS_VAL :=
$(foreach m,$(LOCAL_DEPEND_KO),$(eval KBUILD_EXTRA_SYMBOLS_VAL += $(addprefix $(PWD)/$($(KO_TYPE)_OUT_INTERMEDIATES)/, $(strip $(m))_ko/Module.symvers)))

KBUILD_EXTRA_SYMBOLS_VAL += $(PWD)/$(HOST_OUT_THIRD_PARTY_LIBS)/roce_kernel/roce_kernel/compat-rdma-4.17/Module.symvers

ifneq ($(strip $(symbol_check)), false)
ifeq ($($(KO_TYPE)_DEPEND_KERNEL_SYMVERS), true)
LOCAL_DEPEND_KERNEL := $($(KO_TYPE)_OUT_INTERMEDIATES)/symvers/$($(KO_TYPE)_KERNEL_PATH)/Module.symvers
endif
endif

LOCAL_DEPEND_VALID_KO := $(addprefix $(HOST_OUT_ROOT)/, $(addsuffix $(LOCAL_MODULE_SUFFIX), $(LOCAL_DEPEND_KO))) 

$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_EXTRA_SYMBOLS := $(strip $(KBUILD_EXTRA_SYMBOLS_VAL))
$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_KERNEL_DIR := $(PWD)/$(HOST_KERNEL_PATH)
$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_SYMBOLS_FILE := $(PWD)/$(PRIVATE_KERNEL_SYMBOLS_PATH)
$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_KERNEL_CONFIG_FILE := $(notdir $(HOST_KERNEL_DEFCONFIG))
$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_THIRDPARTY := $(PWD)/$(HOST_OUT_THIRD_PARTY_LIBS)/roce_kernel/roce_kernel/compat-rdma-4.17
$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_SRC_DIR := $(ROCE_KERNEL_SRC_DIR)
$(ROCE_KERNEL_OUT_TIMESTAMP): PRIVATE_OFED_CONFIG_PATH := $(PWD)/$(OFED_CONFIG_PATH)
$(ROCE_KERNEL_OUT_TIMESTAMP): $(LOCAL_DEPEND_VALID_KO) $(LOCAL_DEPEND_KERNEL)
	mkdir -p $(dir $@) && cp -rf $(PRIVATE_SRC_DIR) $(dir $@) && \
	cp -rf third_party/ofed/ $(dir $@) && cd $(dir $@)/ofed/open_source && tar xf OFED-4.17-1.tgz && cd OFED-4.17-1/SRPMS && rpm2cpio compat-rdma-4.17-1.1.ge153426.src.rpm | cpio -id && tar xf compat-rdma-4.17.tgz && \
	cd $(PWD)/$(dir $@)/ofed/open_source/OFED-4.17-1/SRPMS/compat-rdma-4.17 && patch -p2 < ../../../../huawei_patches/001-compat-rdma-4.17.patch && echo "insert patch succ!" && \
	cp -rf $(PWD)/$(dir $@)/ofed/open_source/OFED-4.17-1/SRPMS/compat-rdma-4.17 $(PWD)/$(dir $@)/roce_kernel && \
	cp -rf $(PRIVATE_OFED_CONFIG_PATH)/config.h $(PRIVATE_THIRDPARTY)/compat/ && \
	cp -rf $(PRIVATE_OFED_CONFIG_PATH)/Module.symvers $(PRIVATE_THIRDPARTY) && \
	cd $(PWD)/$(dir $@)/roce_kernel && mkdir -p tmp && \
	cp -rf $(PRIVATE_SYMBOLS_FILE) tmp && \
        make -C $(PRIVATE_KERNEL_DIR) O=$(PWD)/$(dir $@)/roce_kernel/tmp ARCH=$(ROCE_KERNEL_ARCH) CROSS_COMPILE=$(RDMA_CORE_CROSS_COMPILE_PREFIX_HOST) $(PRIVATE_KERNEL_CONFIG_FILE) && \
        make -C $(PRIVATE_KERNEL_DIR) O=$(PWD)/$(dir $@)/roce_kernel/tmp ARCH=$(ROCE_KERNEL_ARCH) CROSS_COMPILE=$(RDMA_CORE_CROSS_COMPILE_PREFIX_HOST) modules_prepare && \
	make TOP_DIR=$(PWD) ARCH=$(ROCE_KERNEL_ARCH) CROSS_COMPILE=$(RDMA_CORE_CROSS_COMPILE_PREFIX_HOST) THIRDPARTY=$(PRIVATE_THIRDPARTY)  KBUILD_EXTRA_SYMBOLS="$(PRIVATE_EXTRA_SYMBOLS)"
	touch $@

$(LOCAL_BUILT_MODULE): $(ROCE_KERNEL_OUT_TIMESTAMP)
	mkdir -p $(dir $@)
	cp -rf $(dir $<)/roce_kernel/hns_roce.ko $@
