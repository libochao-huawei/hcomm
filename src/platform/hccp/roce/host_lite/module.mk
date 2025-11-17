LOCAL_PATH 		:= 	$(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	libascend_rdma_lite

LOCAL_LDFLAGS	+= 	-lrt


# LOCAL_PATH用于build核心加上LOCAL_SRC_FILES组合成源文件, 如果需要编译
# 源代码文件. 则需要PATH_BRIDGE引过去, PATH_BRIDGE和LOCAL_PATH组合成build
# 核心需要的源代码路径
PATH_BRIDGE		:=

LOCAL_SRC_FILES :=

LOCAL_SRC_FILES += $(PATH_BRIDGE)hns_roce_lite.c	\
					$(PATH_BRIDGE)rdma_lite.c

LOCAL_C_INCLUDES:=

IO_ROOT_DIR := $(TOPDIR)third_party

#第三方头文件搜索路径
LOCAL_C_INCLUDES+= 	$(IO_ROOT_DIR)/ofed/build/rdma-core/include	\
			$(TOPDIR)inc/toolchain		\
			$(HOST_OUT_THIRD_PARTY_LIBS)/rmda_core/open_source/OFED-4.17-1/SRPMS/RH/rdma-core-17.5/build/include \
			$(TOPDIR)inc/driver

LOCAL_C_INCLUDES+=      $(TOPDIR)libc_sec/include
LOCAL_C_INCLUDES+=      $(TOPDIR)drivers/network/include
#第三方库搜索路径
LOCAL_LD_DIRS :=

LOCAL_CFLAGS += -DNETWORK_HOST -O2 -Werror
LOCAL_CFLAGS += -D_FORTIFY_SOURCE=2 -O2

## add more LOCAL_SRC_FILES and LOCAL_C_INCLUDES

LOCAL_SHARED_LIBRARIES := libc_sec

include $(BUILD_HOST_SHARED_LIBRARY)

# add host euleros .o and so depend
ifeq ($(host_os),euleros)
$(all_objects):$(all_libraries)
endif

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	libascend_rdma_lite

LOCAL_LDFLAGS	+= 	-lrt


# LOCAL_PATH用于build核心加上LOCAL_SRC_FILES组合成源文件, 如果需要编译
# 源代码文件. 则需要PATH_BRIDGE引过去, PATH_BRIDGE和LOCAL_PATH组合成build
# 核心需要的源代码路径
PATH_BRIDGE		:=

LOCAL_SRC_FILES :=

LOCAL_SRC_FILES += $(PATH_BRIDGE)hns_roce_lite.c	\
					$(PATH_BRIDGE)rdma_lite.c

LOCAL_C_INCLUDES:=

IO_ROOT_DIR := $(TOPDIR)third_party

#第三方头文件搜索路径
LOCAL_C_INCLUDES+= 	$(IO_ROOT_DIR)/ofed/build/rdma-core/include \
			$(TOPDIR)inc/toolchain

LOCAL_C_INCLUDES+=      $(TOPDIR)libc_sec/include
LOCAL_C_INCLUDES+=      $(TOPDIR)drivers/network/include
LOCAL_C_INCLUDES+=      $(TOPDIR)inc/driver
#第三方库搜索路径
LOCAL_LD_DIRS :=

LOCAL_CFLAGS += -DHNS_ROCE_DEVICE -O2 -Werror
LOCAL_CFLAGS += -D_FORTIFY_SOURCE=2 -O2

## add more LOCAL_SRC_FILES and LOCAL_C_INCLUDES

LOCAL_SHARED_LIBRARIES := libc_sec

include $(BUILD_SHARED_LIBRARY)