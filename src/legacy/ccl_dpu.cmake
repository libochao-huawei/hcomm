# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 ccl_dpu 动态链接库，在 host 侧使用
add_library(ccl_dpu SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/communicator/hostdpu/dpu_kernel_entrance.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/communicator/hostdpu/task_service.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/common/log.cc
)

# 宏定义
target_compile_definitions(ccl_dpu PRIVATE
    HCCD
    OPEN_BUILD_PROJECT
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

# 编译选项
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

# 链接选项
target_link_options(ccl_dpu PRIVATE
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
    $<$<CONFIG:Release>:-s>
)

# 链接库
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

# 头文件搜索路径
target_include_directories(ccl_dpu PRIVATE
    ${HCOMM_DIR}/src/include/
    ${HCOMM_DIR}/src/include/hccl
    ${HCOMM_DIR}/src/pkg_inc
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/base/inc
)

if(BUILD_OPEN_PROJECT)
    target_include_directories(ccl_dpu PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/include
        ${ASCEND_CANN_PACKAGE_PATH}/include/hccl
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
    )
endif()

# 指定 ccl_dpu 构建完成后安装到指定的目标位置
install(TARGETS ccl_dpu
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
install(TARGETS ccl_dpu
    LIBRARY DESTINATION ${INSTALL_DPU_KERNEL_JSON_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
