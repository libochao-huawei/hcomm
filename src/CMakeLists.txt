# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(HCCL_PACKET_FLAG 1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED HCOMM_DIR)
    set(HCOMM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()

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

if(BUILD_OPEN_PROJECT)
    message(STATUS "open TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)
    set(HCCL_THIRD_PARTY_DIR ${HCCL_BASE_DIR}/../../../../../../../)
else()
    message(STATUS "close TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${TOP_DIR}/hcomm/src/)
    set(HCCL_THIRD_PARTY_DIR ${TOP_DIR})
endif()

set(ORION_REAL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/legacy)
set(HCOMM_REAL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(hccl_include_list
    ${HCOMM_DIR}/include/hccl
    ${HCOMM_DIR}/src/algorithm/base/inc
    ${HCOMM_DIR}/src/algorithm/pub_inc
    ${HCOMM_DIR}/src/pub_inc
    ${HCOMM_DIR}/src/pub_inc/inner
    ${HCOMM_DIR}/src/pub_inc/new
    ${HCOMM_DIR}/src/algorithm/base/alg_aiv_template
    ${HCOMM_DIR}/src/algorithm/base/alg_template
    ${HCOMM_DIR}/src/algorithm/base/mc2_handler
    ${HCOMM_DIR}/src/algorithm/base/alg_template/component
    ${HCOMM_DIR}/src/algorithm/base/communicator
    ${HCOMM_DIR}/src/algorithm/base/communicator/legacy
    ${HCOMM_DIR}/src/algorithm/impl
    ${HCOMM_DIR}/src/algorithm/impl/legacy
    ${HCOMM_DIR}/src/algorithm/impl/inc
    ${HCOMM_DIR}/src/algorithm/impl/task
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor

    ${HCOMM_DIR}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/recovery/operator_retry/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/snapshot/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/aclgraph/
    ${HCOMM_DIR}/src/framework/common/src/
    ${HCOMM_DIR}/src/framework/common/src/host
    ${HCOMM_DIR}/src/framework/common/src/config
    ${HCOMM_DIR}/src/framework/common/src/topo
    ${HCOMM_DIR}/src/framework/common/src/task
    ${HCOMM_DIR}/src/framework/common/src/exception
    ${HCOMM_DIR}/src/framework/common/src/h2d_tlv
    ${HCOMM_DIR}/src/framework/common/src/thread
    ${HCOMM_DIR}/src/framework/common/src/h2d_dto
    ${HCOMM_DIR}/src/framework/common/src/onesided_memory_management
    ${HCOMM_DIR}/src/framework/common/src/mgr
    ${HCOMM_DIR}/src/framework/communicator/impl/
    ${HCOMM_DIR}/src/framework/communicator/impl/resource_manager
    ${HCOMM_DIR}/src/framework/communicator/impl/one_sided_service
    ${HCOMM_DIR}/src/framework/communicator/impl/zero_copy
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/resource/engine
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel/device

    ${HCOMM_DIR}/src/framework/hcom/
    ${HCOMM_DIR}/src/framework/hcom/gradient_segment
    ${HCOMM_DIR}/src/framework/inc/
    ${HCOMM_DIR}/src/framework/
    ${HCOMM_DIR}/src/framework/nslbdp/
    ${HCOMM_DIR}/src/framework/op_base/src
    ${HCOMM_DIR}/src/framework/group

    ${HCOMM_DIR}/src/common/
    ${HCOMM_DIR}/src/common/health/
    ${HCOMM_DIR}/src/common/stream/
    ${HCOMM_DIR}/src/common/launch_device/

    ${HCOMM_DIR}/src/platform/resource/socket
    ${HCOMM_DIR}/src/platform/inc
    ${HCOMM_DIR}/src/platform/inc/adapter

    ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads
    ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads/slaves/
    ${HCOMM_DIR}/src/framework/next/coll_comms/
    ${HCOMM_DIR}/src/framework/next/coll_comms/communicator
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank/comm_mems/
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank_pairs
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling/aicpu
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling/host
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/taskException
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/taskException/aicpu
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/taskException/host
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels/aicpu/
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels/aicpu/device/
    ${HCOMM_DIR}/src/framework/next/comms/endpoints/server_socket/

    # ccu 头文件
    ${HCOMM_DIR}/src/framework/next/comms/adpt

    ${HCOMM_DIR}/src/framework/next/comms/ccu/pub_inc/
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_pfe
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_channel_ctx_v1

    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_transport/
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_kernel
    ${HCOMM_DIR}/src/framework/next/comms/common/device/
)

# 获取 src/legacy 所有子目录，用于设置头文件搜索路径
set(LEGACY_INCLUDE_LIST)
FILE(GLOB_RECURSE LEGACY_DIRS LIST_DIRECTORIES TRUE ${HCOMM_DIR}/src/legacy/*)
foreach(dir ${LEGACY_DIRS})
    if((IS_DIRECTORY ${dir}) AND (${dir} MATCHES "src/legacy") AND (NOT ${dir} MATCHES "src/legacy/mdpi"))
        list(APPEND LEGACY_INCLUDE_LIST ${dir}) 
    endif()
endforeach()

# 将子目录添加到构建过程
add_subdirectory(platform)
add_subdirectory(framework)
add_subdirectory(legacy)
add_subdirectory(hccd)
add_subdirectory(algorithm)
add_subdirectory(common)

# 定义 aicpu_hcomm 库，在 device 侧使用
# 注意：aicpu_hcomm 依赖 ccl_kernel 的源文件u列表，因此需要在其后定义
include(framework/aicpu_custom.cmake)
