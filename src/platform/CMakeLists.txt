# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if(BUILD_OPEN_PROJECT)
    message(STATUS "open CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
    set(HCCL_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)
    set(HCCL_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/hccl)
    message(STATUS "open HCCL_INC_DIR = ${HCCL_INC_DIR}")
    set(HCCL_OPEN_CODE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)
    set(HCCL_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../../)
else()
    message(STATUS "close TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${TOP_DIR}/hcomm/src/)
    set(HCCL_OPEN_CODE_BASE_DIR ${TOP_DIR}/hcomm/src/)
    set(HCCL_THIRD_PARTY_DIR ${TOP_DIR})
endif()

# 定义 hccl_plf 库，在 host 侧使用
include(hccl_plf.cmake)
# 定义 ccl_kernel_plf 库，在 device 侧使用
include(ccl_kernel_plf.cmake)

add_subdirectory(comm_primitive)
add_subdirectory(common)
add_subdirectory(debug)
add_subdirectory(remote_access)
add_subdirectory(resource)
add_subdirectory(task)
add_subdirectory(ping_mesh)
add_subdirectory(typical)
add_subdirectory(hccp)
