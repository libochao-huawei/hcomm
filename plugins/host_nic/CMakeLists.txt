# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

project(hcomm_host_nic_plugins LANGUAGES C CXX)

set(HCOMM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "hcomm repository root")

set(HCOMM_HOST_NIC_LEGACY_INCLUDE_LIST)
file(GLOB_RECURSE HCOMM_HOST_NIC_LEGACY_DIRS LIST_DIRECTORIES TRUE ${HCOMM_SOURCE_DIR}/src/legacy)
foreach(dir ${HCOMM_HOST_NIC_LEGACY_DIRS})
    if(IS_DIRECTORY ${dir})
        list(APPEND HCOMM_HOST_NIC_LEGACY_INCLUDE_LIST ${dir})
    endif()
endforeach()

function(hcomm_host_nic_common target_name output_name)
    set_target_properties(${target_name} PROPERTIES
        OUTPUT_NAME ${output_name}
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(${target_name} PRIVATE
        -Werror
        -Wfloat-equal
        -Wall
        -fno-common
        -fno-strict-aliasing
        -pipe
        -O3
        -fstack-protector-all
        $<$<CONFIG:Debug>:-g>
    )
    target_compile_definitions(${target_name} PRIVATE
        $<$<BOOL:${BUILD_OPEN_PROJECT}>:OPEN_BUILD_PROJECT>
        $<$<BOOL:${BUILD_OPEN_PROJECT}>:LOG_CPP>
    )
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/roce
        ${CMAKE_CURRENT_LIST_DIR}/roce/reged_mems
        ${CMAKE_CURRENT_LIST_DIR}/roce/dpu_notify
        ${CMAKE_CURRENT_LIST_DIR}/ub
        ${CMAKE_CURRENT_LIST_DIR}/ub/reged_mems
        ${HCOMM_SOURCE_DIR}/include
        ${HCOMM_SOURCE_DIR}/include/hccl
        ${HCOMM_SOURCE_DIR}/pkg_inc
        ${HCOMM_SOURCE_DIR}/pkg_inc/hccl
        ${HCOMM_SOURCE_DIR}/pkg_inc/hcomm/ccu
        ${HCOMM_SOURCE_DIR}/src/pub_inc
        ${HCOMM_SOURCE_DIR}/src/pub_inc/inner
        ${HCOMM_SOURCE_DIR}/src/pub_inc/new
        ${HCOMM_SOURCE_DIR}/src/algorithm/pub_inc
        ${HCOMM_SOURCE_DIR}/src/framework
        ${HCOMM_SOURCE_DIR}/src/framework/inc
        ${HCOMM_SOURCE_DIR}/src/framework/common/src
        ${HCOMM_SOURCE_DIR}/src/framework/common/src/config
        ${HCOMM_SOURCE_DIR}/src/framework/common/src/mgr
        ${HCOMM_SOURCE_DIR}/src/framework/common/src/hashtable
        ${HCOMM_SOURCE_DIR}/src/framework/communicator/impl
        ${HCOMM_SOURCE_DIR}/src/framework/communicator/impl/resource_manager
        ${HCOMM_SOURCE_DIR}/src/framework/communicator/impl/one_sided_service
        ${HCOMM_SOURCE_DIR}/src/framework/op_base/src
        ${HCOMM_SOURCE_DIR}/src/framework/next/common
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/ccu/pub_inc
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/ccu/ccu_device
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_pfe
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_channel_ctx_v1
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/api_c_adpt
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/adpt
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/common
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/endpoints
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/endpoint_pairs
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/endpoint_pairs/channels/host
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/endpoint_pairs/sockets
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/endpoints/server_socket
        ${HCOMM_SOURCE_DIR}/src/framework/next/comms/endpoints/reged_mems
        ${HCOMM_SOURCE_DIR}/src/framework/common/src/h2d_tlv
        ${HCOMM_SOURCE_DIR}/src/common/debug/profiling/inc
        ${HCOMM_SOURCE_DIR}/src/common/debug/profiling/inc/host
        ${HCOMM_SOURCE_DIR}/src/common/debug/config
        ${HCOMM_SOURCE_DIR}/src/common/stream
        ${HCOMM_SOURCE_DIR}/src/common/launch_device
        ${HCOMM_SOURCE_DIR}/src/algorithm/base/inc
        ${HCOMM_SOURCE_DIR}/src/algorithm/base/alg_template
        ${HCOMM_SOURCE_DIR}/src/algorithm/base/communicator
        ${HCOMM_SOURCE_DIR}/src/algorithm/base/communicator/legacy
        ${HCOMM_SOURCE_DIR}/src/algorithm/impl
        ${HCOMM_SOURCE_DIR}/src/algorithm/impl/resource_manager
        ${HCOMM_SOURCE_DIR}/src/algorithm/impl/task
        ${HCOMM_SOURCE_DIR}/src/algorithm/impl/legacy
        ${HCOMM_SOURCE_DIR}/src/algorithm/impl/coll_executor
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/communicator/aicpu
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/resource_manager/socket
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/common
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/phy_topo
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/phy_topo_builder
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/rank_graph
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/rank_graph_builder
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/rank_table_info
        ${HCOMM_SOURCE_DIR}/src/legacy/framework/topo/new_topo_builder/topo_info
        ${HCOMM_SOURCE_DIR}/src/legacy/common
        ${HCOMM_SOURCE_DIR}/src/legacy/common/exception
        ${HCOMM_SOURCE_DIR}/src/legacy/common/types
        ${HCOMM_SOURCE_DIR}/src/legacy/common/utils
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/common
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/pub_inc
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/pub_inc/resource/transport
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/external_system
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/resource
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/resource/buffer
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/resource/connection
        ${HCOMM_SOURCE_DIR}/src/legacy/unified_platform/resource/socket
        ${HCOMM_SOURCE_DIR}/src/platform/common
        ${HCOMM_SOURCE_DIR}/src/platform/inc
        ${HCOMM_SOURCE_DIR}/src/platform/inc/adapter
        ${HCOMM_SOURCE_DIR}/src/platform/hccp
        ${HCOMM_SOURCE_DIR}/src/platform/hccp/inc
        ${HCOMM_SOURCE_DIR}/src/platform/hccp/inc/network
        ${HCOMM_SOURCE_DIR}/src/platform/hccp/inc/private/network
        ${HCOMM_SOURCE_DIR}/src/platform/hccp/external_depends/ubengine
        ${HCOMM_SOURCE_DIR}/src/platform/resource/notify
        ${HCOMM_SOURCE_DIR}/src/platform/resource/dispatcher_ctx
        ${HCOMM_SOURCE_DIR}/src/platform/resource/socket
        ${HCOMM_SOURCE_DIR}/src/platform/resource/transport
        ${HCOMM_SOURCE_DIR}/src/platform/resource/transport/heterog
        ${HCOMM_SOURCE_DIR}/src/platform/resource/transport/host
        ${HCOMM_SOURCE_DIR}/src/platform/resource/transport/onesided
        ${HCOMM_SOURCE_DIR}/src/platform/typical
        ${HCOMM_HOST_NIC_LEGACY_INCLUDE_LIST}
        ${THIRD_PARTY_NLOHMANN_PATH}
        ${RDMA_CORE_INCLUDE_DIR}
        ${URMA_INCLUDE_DIR}
        ${HCOMM_SOURCE_DIR}/third_party/rdma-core-build/include
    )
    target_link_libraries(${target_name} PRIVATE
        $<$<TARGET_EXISTS:c_sec_headers>:c_sec_headers>
        $<$<TARGET_EXISTS:slog_headers>:slog_headers>
        $<$<TARGET_EXISTS:runtime_headers>:runtime_headers>
        $<$<TARGET_EXISTS:acl_rt_headers>:acl_rt_headers>
        $<$<TARGET_EXISTS:ascend_hal_headers>:ascend_hal_headers>
        -lrt
        -ldl
        -lpthread
    )
    if(NOT TARGET c_sec_headers AND DEFINED ASCEND_CANN_PACKAGE_PATH)
        target_include_directories(${target_name} PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/include
        )
    endif()
    if(NOT TARGET slog_headers AND DEFINED ASCEND_CANN_PACKAGE_PATH)
        target_include_directories(${target_name} PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base
        )
    endif()
    if(NOT TARGET runtime_headers AND DEFINED ASCEND_CANN_PACKAGE_PATH)
        target_include_directories(${target_name} PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/runtime
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu
            ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/aicpu_schedule
        )
    endif()
    if(NOT TARGET acl_rt_headers AND DEFINED ASCEND_CANN_PACKAGE_PATH)
        target_include_directories(${target_name} PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/include/acl
        )
    endif()
    if(NOT TARGET ascend_hal_headers AND DEFINED ASCEND_CANN_PACKAGE_PATH)
        target_include_directories(${target_name} PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/include/driver
            ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/pkg_inc/driver
        )
    endif()
endfunction()

add_library(hcomm_host_roce_plugin SHARED
    plugin_core.cc
    roce/host_roce_plugin.cc
    roce/cpu_roce_endpoint.cc
    roce/reged_mems/roce_mem.cc
    roce/host_cpu_roce_channel.cc
    roce/host_rdma_connection.cc
    roce/dpu_notify/dpu_notify_manager.cc
)
hcomm_host_nic_common(hcomm_host_roce_plugin hcomm_host_roce_plugin)

add_library(hcomm_host_ub_plugin SHARED
    plugin_core.cc
    ub/host_ub_plugin.cc
    ub/cpu_urma_endpoint.cc
    ub/reged_mems/urma_mem.cc
    ub/host_cpu_urma_channel.cc
)
hcomm_host_nic_common(hcomm_host_ub_plugin hcomm_host_ub_plugin)

install(TARGETS hcomm_host_roce_plugin hcomm_host_ub_plugin
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}/hcomm_plugin ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
