# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 创建 ccl_kernel_plf 动态链接库
add_library(ccl_kernel_plf SHARED)
# 创建 ccl_kernel_plf 静态链接库
add_library(ccl_kernel_plf_a STATIC)

# 编译宏定义
set(CCL_KERNEL_PLF_DEFINES
    HCCD
    CCL_KERNEL
    CCL_KERNEL_AICPU
    USE_AICORE_REDUCESUM
    USE_AICORE_GATHERV2
    USE_AICORE_GATHERV2_INFER
)
target_compile_definitions(ccl_kernel_plf PRIVATE ${CCL_KERNEL_PLF_DEFINES})
target_compile_definitions(ccl_kernel_plf_a PRIVATE ${CCL_KERNEL_PLF_DEFINES})

# 编译选项
set(CCL_KERNEL_PLF_COMPILE_OPTIONS
    -Werror
    -Wall
    -fno-common
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
    -D_FORTIFY_SOURCE=2 -O2
    -fstack-protector-all
)
target_compile_options(ccl_kernel_plf PRIVATE ${CCL_KERNEL_PLF_COMPILE_OPTIONS})
target_compile_options(ccl_kernel_plf_a PRIVATE
    ${CCL_KERNEL_PLF_COMPILE_OPTIONS}
    -ftrapv
    -fPIC
)

# 链接选项
set(CCL_KERNEL_PLF_LINK_OPTIONS
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
)
target_link_options(ccl_kernel_plf PRIVATE
    ${CCL_KERNEL_PLF_LINK_OPTIONS}
    -s
)
target_link_options(ccl_kernel_plf_a PRIVATE
    ${CCL_KERNEL_PLF_LINK_OPTIONS}
)

# 头文件搜索路径
set(CCL_KERNEL_PLF_INCLUDE_DIRS
    ${include_list}
    ${HCCL_BASE_DIR}/hccl/hccl_comm/wrapper/notify/
    ${HCCL_BASE_DIR}/common/debug/profiling/inc/aicpu
    ${HCCL_BASE_DIR}/framework/common/src/aicpu
)

target_include_directories(ccl_kernel_plf PRIVATE ${CCL_KERNEL_PLF_INCLUDE_DIRS})
target_include_directories(ccl_kernel_plf_a PRIVATE ${CCL_KERNEL_PLF_INCLUDE_DIRS})

if(BUILD_OPEN_PROJECT)
    # 编译宏定义
    target_compile_definitions(ccl_kernel_plf PRIVATE OPEN_BUILD_PROJECT)
    target_compile_definitions(ccl_kernel_plf_a PRIVATE OPEN_BUILD_PROJECT)

    # 头文件搜索路径
    set(CCL_KERNEL_PLF_OPEN_INCLUDE_DIRS
        ${HCCL_BASE_DIR}/../include
        ${HCCL_BASE_DIR}/../include/hccl
        ${HCCL_BASE_DIR}/../pkg_inc
        ${HCCL_BASE_DIR}/../pkg_inc/hccl
        ${HCCL_BASE_DIR}/pub_inc
        ${HCCL_BASE_DIR}/platform/hccp/inc
        ${HCCL_BASE_DIR}/platform/hccp/inc/network
        ${RDMA_CORE_INCLUDE_DIR}

        # runtime头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/

        # mmpa头文件
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/

        # 包间接口
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/common/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/

        # driver头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver

        # 临时依赖头文件，待删除
        ${HCCL_BASE_DIR}/../externel_depends/tsch/
        ${RDMA_CORE_INCLUDE_DIR}

        ${THIRD_PARTY_NLOHMANN_PATH}
    )
    target_include_directories(ccl_kernel_plf PRIVATE ${CCL_KERNEL_PLF_OPEN_INCLUDE_DIRS})
    target_include_directories(ccl_kernel_plf_a PRIVATE ${CCL_KERNEL_PLF_OPEN_INCLUDE_DIRS})

    # 链接库搜索路径
    target_link_directories(ccl_kernel_plf PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径
    )
    target_link_directories(ccl_kernel_plf_a PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/
    )

    # 链接库
    target_link_libraries(ccl_kernel_plf
        -Wl,--no-as-needed
        c_sec
        aicpu_sharder
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )

    target_link_libraries(ccl_kernel_plf_a
        -Wl,--no-as-needed
        c_sec
        aicpu_sharder
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
else()
    # 头文件搜索路径
    set(CCL_KERNEL_PLF_NON_OPEN_INCLUDE_DIRS
        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/aicpu/tsd
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/include/external/acl
        ${TOP_DIR}/runtime/include/driver
        ${TOP_DIR}/runtime/pkg_inc
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/runtime/pkg_inc/profiling
        ${TOP_DIR}/runtime/pkg_inc/trace
        ${TOP_DIR}/runtime/pkg_inc/base
        ${TOP_DIR}/runtime/pkg_inc/aicpu_sched
        ${TOP_DIR}/runtime/src/runtime/platform/inc
        ${TOP_DIR}/inc/external
        ${TOP_DIR}/inc/aicpu/
        ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/runtime/src/dfx/adump/inc/adump
        ${TOP_DIR}/inc/aicpu/common
        ${TOP_DIR}/ace/csruntime/inc/tsch
        ${TOP_DIR}/metadef
        ${TOP_DIR}/metadef/inc
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/metadef/inc/common
        ${TOP_DIR}/metadef/inc/external

        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/hcomm/pkg_inc
        ${TOP_DIR}/hcomm/pkg_inc/hccl
        ${TOP_DIR}/hcomm/src/
        ${TOP_DIR}/hcomm/src/include/hccl/
        ${TOP_DIR}/hcomm/src/platform
        ${TOP_DIR}/hcomm/src/platform/hccp/inc
        ${TOP_DIR}/hcomm/src/platform/hccp/inc/network

        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc
    )
    target_include_directories(ccl_kernel_plf PRIVATE ${CCL_KERNEL_PLF_NON_OPEN_INCLUDE_DIRS})
    target_include_directories(ccl_kernel_plf_a PRIVATE
        ${CCL_KERNEL_PLF_NON_OPEN_INCLUDE_DIRS}
        ${TOP_DIR}/abl/qos/qosmng
    )

    # 链接库
    target_link_libraries(ccl_kernel_plf
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:npu_runtime_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:kernel_tiling_headers>
        -Wl,--no-as-needed
        c_sec
        aicpu_sharder
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )

    target_link_libraries(ccl_kernel_plf_a PRIVATE
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:npu_runtime_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:kernel_tiling_headers>
        -Wl,--no-as-needed
        c_sec
        aicpu_sharder
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )
endif()

# 设置依赖
add_dependencies(ccl_kernel_plf json)
add_dependencies(hccd json)
if(BUILD_OPEN_PROJECT)
    add_dependencies(ccl_kernel_plf hccl_legacy)
    add_dependencies(hccd hccl_legacy)
endif()

# 安装目录
if(NOT BUILD_OPEN_PROJECT)
    install(TARGETS ccl_kernel_plf
        LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    )
    install(TARGETS ccl_kernel_plf_a
        ARCHIVE DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    )
endif()

# 设置 ccl_kernel_plf_a 输出文件名字
set_target_properties(ccl_kernel_plf_a
    PROPERTIES
    OUTPUT_NAME ccl_kernel_plf
)
