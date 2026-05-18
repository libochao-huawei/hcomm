# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if(NOT BUILD_OPEN_PROJECT)
    set(INSTALL_CCL_KERNEL_JSON_DIR ${INSTALL_LIBRARY_DIR})
    set(INSTALL_DPU_KERNEL_JSON_DIR ${INSTALL_LIBRARY_DIR})
    message(STATUS "BUILD_OPEN_PROJECT=${BUILD_OPEN_PROJECT}")
    message(STATUS "TOP_DIR=${TOP_DIR}")
    message(STATUS "CANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH}")
    message(STATUS "INSTALL_LIBRARY_DIR=${INSTALL_LIBRARY_DIR}")
endif()

if(NOT DEFINED HCOMM_DIR)
    set(HCOMM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()

# AIV算子调测阶段可配置为1以打开AscendC打印功能
if(NOT DEFINED ASCENDC_DUMP)
    set(ASCENDC_DUMP "0")
endif()

message(STATUS "HCOMM_DIR=${HCOMM_DIR}")
message(STATUS "ASCENDC_DUMP=${ASCENDC_DUMP}")
message(STATUS "PRODUCT_SIDE=${PRODUCT_SIDE}")

# 组件间对外头文件目录
add_library(hccl_headers INTERFACE)
target_include_directories(hccl_headers INTERFACE
    $<BUILD_INTERFACE:${TOP_DIR}/hcomm/include>
    $<BUILD_INTERFACE:${TOP_DIR}/hcomm/include/hccl>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/hccl>
    $<INSTALL_INTERFACE:include/hccl/external>
    $<INSTALL_INTERFACE:include/hccl/external/hccl>
)

# 获取 src/legacy 所有子目录，用于设置头文件搜索路径
set(LEGACY_INCLUDE_LIST)
FILE(GLOB_RECURSE LEGACY_DIRS LIST_DIRECTORIES TRUE ${HCOMM_DIR}/src/legacy)
foreach(dir ${LEGACY_DIRS})
    if((IS_DIRECTORY ${dir}) AND (${dir} MATCHES "src/legacy") AND (NOT ${dir} MATCHES "src/legacy/mdpi"))
        list(APPEND LEGACY_INCLUDE_LIST ${dir}) 
    endif()
endforeach()

include_directories( BEFORE${HCOMM_DIR}/src/common/error_manager)

# 将子目录添加到构建过程
add_subdirectory(hccd)
add_subdirectory(platform)
add_subdirectory(framework)
add_subdirectory(legacy)
add_subdirectory(algorithm)
add_subdirectory(common)

# 定义 aicpu_custom 库，在 device 侧使用
# 注意：aicpu_custom 依赖 ccl_kernel 的源文件列表，因此需要在其后定义
if(PRODUCT_SIDE STREQUAL "device")
    include(framework/aicpu_custom.cmake)
endif()
