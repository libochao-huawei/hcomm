# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
add_subdirectory(adpt)
add_subdirectory(common)
add_subdirectory(ccu)
add_subdirectory(endpoints)
add_subdirectory(endpoint_pairs)
add_subdirectory(comm_engine_res)
add_subdirectory(api_c_adpt)

target_include_directories(hcomm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/api_c_adpt
    ${CMAKE_CURRENT_SOURCE_DIR}/endpoint_pairs
    # ${CMAKE_CURRENT_SOURCE_DIR}/endpoint_pairs/channels
    # ${CMAKE_CURRENT_SOURCE_DIR}/endpoint_pairs/channels/slaves
    ${CMAKE_CURRENT_SOURCE_DIR}/endpoints
    ${CMAKE_CURRENT_SOURCE_DIR}/endpoints/reged_mems
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_engine_res
    #${CMAKE_CURRENT_SOURCE_DIR}/comm_engine_res/engine_ctxs
    #${CMAKE_CURRENT_SOURCE_DIR}/comm_engine_res/notifys
    #${CMAKE_CURRENT_SOURCE_DIR}/comm_engine_res/threads
)
