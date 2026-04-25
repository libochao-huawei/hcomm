# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 ccl_dpu 动态链接库，在 device 侧使用
add_library(ccl_dpu SHARED)

# 预处理宏定义
target_compile_definitions(ccl_dpu PRIVATE
    HCCD
    OPEN_BUILD_PROJECT
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

# 头文件搜索路径
target_include_directories(ccl_dpu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/base/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${HCCL_BASE_DIR}/include/
    ${HCCL_BASE_DIR}/include/hccl
    ${HCCL_BASE_DIR}/pkg_inc
    ${HCCL_BASE_DIR}/src/pub_inc/hccl

    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${ASCEND_CANN_PACKAGE_PATH}/include/hccl
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/runtime
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
)

# 添加源文件
target_sources(ccl_dpu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/communicator/hostdpu/dpu_kernel_entrance.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/communicator/hostdpu/task_service.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/common/log.cc
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
target_link_libraries(ccl_dpu PRIVATE
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

# 安装
install(TARGETS ccl_dpu
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)
install(TARGETS ccl_dpu
    LIBRARY DESTINATION ${INSTALL_DPU_KERNEL_JSON_DIR} OPTIONAL
)

if(NOT KERNEL_MODE)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json
        COMMAND ${HI_PYTHON} ${HCOMM_ROOT_DIR}/cmake/scripts/parser_ini.py ${CMAKE_CURRENT_SOURCE_DIR}/framework/communicator/hostdpu/ccl_dpu.ini ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(dpu_kernel_json DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json
        DESTINATION ${INSTALL_DPU_KERNEL_JSON_DIR} OPTIONAL
    )
endif()
