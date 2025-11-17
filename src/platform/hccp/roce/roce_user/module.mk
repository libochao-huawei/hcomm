LOCAL_PATH 		:= 	$(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	libhns-rdmav17

LOCAL_LDFLAGS	+= 	-lrt


# LOCAL_PATH用于build核心加上LOCAL_SRC_FILES组合成源文件, 如果需要编译
# 源代码文件. 则需要PATH_BRIDGE引过去, PATH_BRIDGE和LOCAL_PATH组合成build
# 核心需要的源代码路径
PATH_BRIDGE		:=

LOCAL_SRC_FILES :=

LOCAL_SRC_FILES += $(PATH_BRIDGE)hns_roce_u_buf.c	\
		   $(PATH_BRIDGE)hns_roce_u.c	\
	           $(PATH_BRIDGE)hns_roce_u_db.c	\
		   $(PATH_BRIDGE)hns_roce_u_hw_v2.c	\
		   $(PATH_BRIDGE)hns_roce_u_hw_v2_qp.c	\
		   $(PATH_BRIDGE)hns_roce_u_hw_v2_cq.c	\
		   $(PATH_BRIDGE)hns_roce_u_hw_v2_opreation.c	\
		   $(PATH_BRIDGE)hns_roce_u_hw_v2_send.c	\
		   $(PATH_BRIDGE)hns_roce_u_verbs.c	\
		   $(PATH_BRIDGE)hns_roce_u_qp.c	\
		   $(PATH_BRIDGE)hns_roce_u_cmd.c	\
		   $(PATH_BRIDGE)hns_roce_u_mm.c	\
		   $(PATH_BRIDGE)hns_roce_u_stdio.c	\
		   $(PATH_BRIDGE)verbs_exp.c

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

## add more LOCAL_SRC_FILES and LOCAL_C_INCLUDES

LOCAL_SHARED_LIBRARIES := libibverbs libslog libc_sec

include $(BUILD_HOST_SHARED_LIBRARY)

# add host euleros .o and so depend
ifeq ($(host_os),euleros)
$(all_objects):$(all_libraries)
endif

include $(CLEAR_VARS)

LOCAL_MODULE 	:= 	libhns-rdmav17

LOCAL_LDFLAGS	+= 	-lrt


# LOCAL_PATH用于build核心加上LOCAL_SRC_FILES组合成源文件, 如果需要编译
# 源代码文件. 则需要PATH_BRIDGE引过去, PATH_BRIDGE和LOCAL_PATH组合成build
# 核心需要的源代码路径
PATH_BRIDGE		:=

LOCAL_SRC_FILES :=

LOCAL_SRC_FILES += $(PATH_BRIDGE)hns_roce_u_buf.c       \
                   $(PATH_BRIDGE)hns_roce_u.c   \
                   $(PATH_BRIDGE)hns_roce_u_db.c        \
                   $(PATH_BRIDGE)hns_roce_u_hw_v2.c     \
                   $(PATH_BRIDGE)hns_roce_u_hw_v2_qp.c  \
                   $(PATH_BRIDGE)hns_roce_u_hw_v2_cq.c  \
                   $(PATH_BRIDGE)hns_roce_u_hw_v2_opreation.c   \
                   $(PATH_BRIDGE)hns_roce_u_hw_v2_send.c        \
                   $(PATH_BRIDGE)hns_roce_u_verbs.c     \
                   $(PATH_BRIDGE)hns_roce_u_qp.c        \
                   $(PATH_BRIDGE)hns_roce_u_cmd.c	\
                   $(PATH_BRIDGE)hns_roce_u_mm.c        \
                   $(PATH_BRIDGE)hns_roce_u_stdio.c     \
		   $(PATH_BRIDGE)verbs_exp.c

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

## add more LOCAL_SRC_FILES and LOCAL_C_INCLUDES

LOCAL_SHARED_LIBRARIES := libibverbs libslog libc_sec

include $(BUILD_SHARED_LIBRARY)
