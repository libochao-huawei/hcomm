# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
# 本地单独构建使用
IF (DEFINED ENV{HCCL_LOCAL_BUILD})
    # 定义后续需要使用的变量
    cmake_minimum_required(VERSION 3.20.5)
    project(hccl)
    set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
ENDIF ()

set(HCCL_V2_CODE_ROOT ${ORION_REAL_PATH})
set(HCCL_BASE_DIR ${HCOMM_REAL_PATH})

include(${CMAKE_CURRENT_LIST_DIR}/hccl_v2.cmake)

# 创建 ccl_dpu 共享动态库
add_library(ccl_dpu SHARED)
# 指定 ccl_dpu 需要包含的文件搜索路径
target_compile_definitions(ccl_dpu PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

target_include_directories(ccl_dpu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/base/inc
    ${HCCL_BASE_DIR}/include/
    ${HCCL_BASE_DIR}/include/hccl
    ${HCCL_BASE_DIR}/pkg_inc
    ${HCCL_BASE_DIR}/src/pub_inc/hccl
    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${ASCEND_CANN_PACKAGE_PATH}/include/hccl
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
    ${HCCL_V2_CODE_ROOT}/common/
)
# 指定 ccl_dpu 的构建依赖关系
add_dependencies(hccl_v2 ccl_dpu)
# 指定源文件
set(CCL_DPU_SRC_LIST
    ${HCCL_V2_CODE_ROOT}/framework/communicator/hostdpu/dpu_kernel_entrance.cc
    ${HCCL_V2_CODE_ROOT}/framework/communicator/hostdpu/task_service.cc
    ${HCCL_V2_CODE_ROOT}/common/log.cc
)
target_sources(ccl_dpu PRIVATE
    ${CCL_DPU_SRC_LIST}
)

# 指定 ccl_dpu 的编译选项
target_compile_options(ccl_dpu PRIVATE
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
# 为 ccl_dpu 添加编译定义
target_compile_definitions(ccl_dpu PRIVATE
    HCCD
    OPEN_BUILD_PROJECT
)
# 向 ccl_dpu 添加链接器选项
target_link_options(ccl_dpu PRIVATE
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
    $<$<CONFIG:Release>:-s>
)
# 将库文件链接到目标ccl_dpu
target_link_libraries(ccl_dpu
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

# 指定 ccl_dpu 构建完成后安装到指定的目标位置
install(TARGETS  ccl_dpu
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
install(TARGETS  ccl_dpu
    LIBRARY DESTINATION ${INSTALL_DPU_KERNEL_JSON_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)

# 需要链接的库，后需增加
IF (NOT DEFINED ENV{HCCL_LOCAL_BUILD})
target_link_libraries(hccl_v2 PRIVATE
    -Wl,--no-as-needed
    c_sec
    unified_dlog
    mmpa
    runtime
    ascendcl
    error_manager
    ccl_dpu
    tsdclient
    ra
    -Wl,--as-needed
    hccl_headers
    topoaddrinfo
)
ENDIF ()

# 将hccl编译出的动态库加入CANN的安装包
IF (NOT DEFINED ENV{HCCL_LOCAL_BUILD})
install(TARGETS hccl_v2
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)

ENDIF ()
