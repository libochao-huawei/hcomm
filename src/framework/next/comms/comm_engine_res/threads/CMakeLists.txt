# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(src_list
    cpu_ts_thread.cc
    aicpu_ts_thread.cc
    aiv_thread.cc
    cpu_thread.cc
    thread.cc
)


target_sources(hcomm PRIVATE
    ${src_list}
)

add_subdirectory(slaves)
if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
    target_sources(ccl_kernel PRIVATE
        ${src_list}
    )
endif()

# 创建 ccl_cpu 共享动态库
add_library(ccl_cpu SHARED)
# 指定 ccl_cpu 需要包含的文件搜索路径
target_compile_definitions(ccl_cpu PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

target_include_directories(ccl_cpu PRIVATE
    ${hccl_include_list}
    ${CMAKE_CURRENT_SOURCE_DIR}/base/inc
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HCCL_BASE_DIR}/../pkg_inc
    ${HCCL_BASE_DIR}/../pkg_inc/hccl
    ${HCCL_BASE_DIR}/../include
    ${HCCL_BASE_DIR}/../include/hccl
    ${HCCL_BASE_DIR}/include/
    ${HCCL_BASE_DIR}/include/hccl
    ${HCCL_BASE_DIR}/pkg_inc
    ${HCCL_BASE_DIR}/pub_inc
    ${HCCL_BASE_DIR}/pub_inc/hccl
    ${HCCL_BASE_DIR}/legacy/framework/communicator/hostdpu
    ${HCCL_BASE_DIR}/legacy/unified_platform/pub_inc
    ${HCCL_BASE_DIR}/legacy/common
    ${HCCL_BASE_DIR}/legacy/common/types
    ${HCCL_BASE_DIR}/legacy/common/utils
    ${HCCL_BASE_DIR}/legacy/common/exception
    ${HCCL_BASE_DIR}/legacy/framework/communicator/aicpu
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/stream/aicpu
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/buffer
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/buffer/aicpu
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/notify
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/notify/aicpu
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/connection/aicpu
    ${HCCL_BASE_DIR}/legacy/framework/dfx/common
    ${HCCL_BASE_DIR}/legacy/framework/dfx/profiling
    ${HCCL_BASE_DIR}/legacy/unified_platform/external_system
    ${HCCL_BASE_DIR}/legacy/service/collective
    ${HCCL_BASE_DIR}/common/debug/profiling/inc
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res
    ${ASCEND_CANN_PACKAGE_ENV_PATH}/include/driver
    ${ASCEND_CANN_PACKAGE_ENV_PATH}/pkg_inc/driver
    ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/include/driver
    ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/pkg_inc/driver
    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${ASCEND_CANN_PACKAGE_PATH}/include/hccl
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/runtime
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
)
# 指定 ccl_cpu 的构建依赖关系
add_dependencies(hcomm ccl_cpu)
# 指定源文件
set(CCL_CPU_SRC_LIST
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads/slaves/cpu_thread_kernel.cc
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads/cpu_thread.cc
)
target_sources(ccl_cpu PRIVATE
    ${CCL_CPU_SRC_LIST}
)

# 指定 ccl_cpu 的编译选项
target_compile_options(ccl_cpu PRIVATE
    -Werror
    -Wall
    -fno-common
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
    -fstack-protector-all
    $<$<CONFIG:Debug>:-g>
)
# 为 ccl_cpu 添加编译定义
target_compile_definitions(ccl_cpu PRIVATE
    HCCD
    OPEN_BUILD_PROJECT
)
# 向 ccl_cpu 添加链接器选项
target_link_options(ccl_cpu PRIVATE
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
    $<$<CONFIG:Release>:-s>
)
# 将库文件链接到目标ccl_cpu
target_link_libraries(ccl_cpu
    -Wl,--no-as-needed
    c_sec
    mmpa
    runtime
    acl_rt
    -Wl,--as-needed
    -lrt
    -ldl
    -lpthread
)

# 指定 ccl_cpu 构建完成后安装到指定的目标位置
install(TARGETS  ccl_cpu
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)
