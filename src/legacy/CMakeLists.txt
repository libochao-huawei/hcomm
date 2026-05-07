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

include(ccl_dpu.cmake)

# 将hccl编译出的动态库加入CANN的安装包
IF (NOT DEFINED ENV{HCCL_LOCAL_BUILD})
install(TARGETS hccl_v2
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)

ENDIF ()
