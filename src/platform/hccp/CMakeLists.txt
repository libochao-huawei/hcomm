# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
include(version/add_version.cmake)
set(PROJECT_TYPE HCOMM_PROJECT)
set(HCCP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEVICE_MODE)
    set(RUN_TYPE HCOMM_HOST)
elseif(USE_ALOG STREQUAL "1")
    set(RUN_TYPE HCOMM_HCCD)
else()
    set(RUN_TYPE HCOMM_DEVICE)
endif()

if(DEFINED CANN_VERSION_hcomm_VERSION)
    set(BUILD_VERSION ${CANN_VERSION_hcomm_VERSION})
else()
    set(BUILD_VERSION UNKBOWN)
endif()
message(STATUS "BUILDING ${RUN_TYPE}, VERSIONG:${CANN_VERSION_hcomm_VERSION}")

if(BUILD_OPEN_PROJECT AND (PRODUCT_SIDE STREQUAL "device"))
    # 添加子目录
    add_subdirectory(stub_tsd)
    add_subdirectory(netco)
    # 定义 hccp_service.bin 可执行文件，在 Device 侧使用
    include(hccp_service.cmake)
endif()

# 添加子目录，Host和Device都涉及
add_subdirectory(rdma_agent)
add_subdirectory(rdma_service)
