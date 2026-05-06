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

#需要编译的文件list
set(include_list
    ${HCCL_BASE_DIR}/pub_inc
    ${HCCL_BASE_DIR}/pub_inc/aicpu/
    ${HCCL_BASE_DIR}/pub_inc/inner
    ${HCCL_BASE_DIR}/pub_inc/new

    ${HCCL_BASE_DIR}/algorithm/base/inc
    ${HCCL_BASE_DIR}/algorithm/pub_inc
    ${HCCL_BASE_DIR}/algorithm/base/alg_aiv_template
    ${HCCL_BASE_DIR}/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/component
    ${HCCL_BASE_DIR}/algorithm/base/mc2_handler
    ${HCCL_BASE_DIR}/algorithm/base/communicator
    ${HCCL_BASE_DIR}/algorithm/impl
    ${HCCL_BASE_DIR}/algorithm/impl/inc
    ${HCCL_BASE_DIR}/algorithm/impl/legacy
    ${HCCL_BASE_DIR}/algorithm/impl/legacy/operator
    ${HCCL_BASE_DIR}/algorithm/base/communicator/legacy
    ${HCCL_BASE_DIR}/algorithm/impl/resource_manager
    ${HCCL_BASE_DIR}/algorithm/impl/task
    ${HCCL_BASE_DIR}/algorithm/impl/operator
    ${HCCL_BASE_DIR}/algorithm/impl/operator/registry
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/registry
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_to_all
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_scatter
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_all_gather
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_alltoall
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_broadcast
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_reduce
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_scatter
    ${HCCL_BASE_DIR}/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/algorithm/pub_inc

    ${HCCL_BASE_DIR}/common/health
    ${HCCL_BASE_DIR}/common/debug/profiling/inc
    ${HCCL_BASE_DIR}/common/debug/config/
    ${HCCL_BASE_DIR}/common/stream
    ${HCCL_BASE_DIR}/common/launch_device

    ${HCCL_BASE_DIR}/framework/inc
    ${HCCL_BASE_DIR}/framework/inc/hccd
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/recovery/operator_retry
    ${HCCL_BASE_DIR}/framework/
    ${HCCL_BASE_DIR}/framework/common/src/
    ${HCCL_BASE_DIR}/framework/common/src/exception
    ${HCCL_BASE_DIR}/framework/common/src/config
    ${HCCL_BASE_DIR}/framework/common/src/task
    ${HCCL_BASE_DIR}/framework/common/src/topo
    ${HCCL_BASE_DIR}/framework/common/src/mgr
    ${HCCL_BASE_DIR}/framework/hcom
    ${HCCL_BASE_DIR}/framework/communicator/impl/
    ${HCCL_BASE_DIR}/framework/communicator/impl/one_sided_service/
    ${HCCL_BASE_DIR}/framework/communicator/impl/zero_copy
    ${HCCL_BASE_DIR}/framework/communicator/impl/resource_manager
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/rank_graph
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/resource/engine
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel/device
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads
    ${HCCL_BASE_DIR}/framework/next/coll_comms/rank
    ${HCCL_BASE_DIR}/framework/next/coll_comms/communicator
    ${HCCL_BASE_DIR}/framework/next/coll_comms/communicator/aicpu
    ${HCCL_BASE_DIR}/framework/op_base/src/

    ${HCCL_BASE_DIR}/platform/
    ${HCCL_BASE_DIR}/platform/inc
    ${HCCL_BASE_DIR}/platform/inc/adapter
    ${HCCL_BASE_DIR}/platform/inc/adapter/hccd
    ${HCCL_BASE_DIR}/platform/resource/dispatcher_ctx
    ${HCCL_BASE_DIR}/platform/common/
    ${HCCL_BASE_DIR}/platform/common/unique
    ${HCCL_BASE_DIR}/platform/common/buffer_manager
    ${HCCL_BASE_DIR}/platform/common/unfold_cache
    ${HCCL_BASE_DIR}/platform/resource/transport
    ${HCCL_BASE_DIR}/platform/resource/transport/device
    ${HCCL_BASE_DIR}/platform/resource/transport/host
    ${HCCL_BASE_DIR}/platform/resource/transport/heterog
    ${HCCL_BASE_DIR}/platform/resource/transport/onesided
    ${HCCL_BASE_DIR}/platform/resource/transport/onesided/device
    ${HCCL_BASE_DIR}/platform/resource/notify
    ${HCCL_BASE_DIR}/platform/ping_mesh
    ${HCCL_BASE_DIR}/platform/resource/socket
    ${HCCL_BASE_DIR}/platform/task
    ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/include/legacy
    ${HCCL_BASE_DIR}/platform/typical

    ${HCCL_BASE_DIR}/framework/next/coll_comms/rank_pairs
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/transport
    ${HCCL_BASE_DIR}/legacy/common/utils/
    ${HCCL_BASE_DIR}/legacy/common/exception/
    ${HCCL_BASE_DIR}/legacy/common/types/
    ${HCCL_BASE_DIR}/legacy/common/
    ${HCCL_BASE_DIR}/legacy/framework/resource_manager/socket/
    ${HCCL_BASE_DIR}/legacy/unified_platform/external_system/
    ${HCCL_BASE_DIR}/legacy/unified_platform/common/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/sockets/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoints/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/

)

# 定义 ccl_kernel_plf 库，在 device 侧使用
include(platform/ccl_kernel_plf.cmake)
# 定义 ccl_kernel 库，在 device 侧使用
include(framework/ccl_kernel.cmake)

# 将子目录添加到构建过程
add_subdirectory(framework)
add_subdirectory(hccd)
add_subdirectory(algorithm)
add_subdirectory(platform)
add_subdirectory(common)

# 定义 aicpu_hcomm 库，在 device 侧使用
# 注意：aicpu_hcomm 依赖 ccl_kernel 的源文件u列表，因此需要再其后定义
include(framework/aicpu_custom.cmake)
