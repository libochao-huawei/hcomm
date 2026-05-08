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
    ${CMAKE_CURRENT_SOURCE_DIR}/hcclCommDfx.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hcclCommOp.cc
)

if(TARGET hcomm)
    target_sources(hcomm PRIVATE
        ${src_list}
    )
endif()

set(device_src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/hcclCommDfxLite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hcclCommOp.cc
)

if(TARGET ccl_kernel)
    target_sources(ccl_kernel PRIVATE
        ${device_src_list}
    )
endif()

add_subdirectory(profiling)
add_subdirectory(taskException)
add_subdirectory(cluster_monitor)