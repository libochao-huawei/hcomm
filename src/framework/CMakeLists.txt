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

if(BUILD_OPEN_PROJECT)
    message(STATUS "open TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)
    set(HCCL_OPEN_CODE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../)
    set(HCCL_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../../)
else()
    message(STATUS "close TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${TOP_DIR}/hcomm/src/)
    set(HCCL_OPEN_CODE_BASE_DIR ${TOP_DIR}/hcomm/src/)
    set(HCCL_THIRD_PARTY_DIR ${TOP_DIR})
    set(INSTALL_CCL_KERNEL_JSON_DIR ${INSTALL_LIBRARY_DIR})
    set(INSTALL_DPU_KERNEL_JSON_DIR ${INSTALL_LIBRARY_DIR})
endif()


set(HCCL_V2_CODE_ROOT ${ORION_REAL_PATH})


message(STATUS "HCCL_OPEN_CODE_BASE_DIR = ${HCCL_OPEN_CODE_BASE_DIR}")
message(STATUS "HCCL_BASE_DIR = ${HCCL_BASE_DIR}")
add_library(hcomm  SHARED)

set(hccl_include_list
    ${HCCL_BASE_DIR}/../include/hccl
    ${HCCL_BASE_DIR}/algorithm/base/inc
    ${HCCL_BASE_DIR}/algorithm/pub_inc
    ${HCCL_BASE_DIR}/pub_inc
    ${HCCL_BASE_DIR}/pub_inc/inner
    ${HCCL_BASE_DIR}/pub_inc/new
    ${HCCL_BASE_DIR}/algorithm/base/alg_aiv_template
    ${HCCL_BASE_DIR}/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/algorithm/base/mc2_handler
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/component
    ${HCCL_BASE_DIR}/algorithm/base/communicator
    ${HCCL_BASE_DIR}/algorithm/base/communicator/legacy
    ${HCCL_BASE_DIR}/algorithm/impl
    ${HCCL_BASE_DIR}/algorithm/impl/legacy
    ${HCCL_BASE_DIR}/algorithm/impl/inc
    ${HCCL_BASE_DIR}/algorithm/impl/task
    ${HCCL_BASE_DIR}/algorithm/impl/resource_manager
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor

    ${HCCL_BASE_DIR}/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/recovery/operator_retry/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/snapshot/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/aclgraph/
    ${HCCL_BASE_DIR}/framework/common/src/
    ${HCCL_BASE_DIR}/framework/common/src/host
    ${HCCL_BASE_DIR}/framework/common/src/config
    ${HCCL_BASE_DIR}/framework/common/src/topo
    ${HCCL_BASE_DIR}/framework/common/src/task
    ${HCCL_BASE_DIR}/framework/common/src/exception
    ${HCCL_BASE_DIR}/framework/common/src/h2d_tlv
    ${HCCL_BASE_DIR}/framework/common/src/thread
    ${HCCL_BASE_DIR}/framework/common/src/h2d_dto
    ${HCCL_BASE_DIR}/framework/common/src/onesided_memory_management
    ${HCCL_BASE_DIR}/framework/common/src/mgr
    ${HCCL_BASE_DIR}/framework/communicator/impl/
    ${HCCL_BASE_DIR}/framework/communicator/impl/resource_manager
    ${HCCL_BASE_DIR}/framework/communicator/impl/one_sided_service
    ${HCCL_BASE_DIR}/framework/communicator/impl/zero_copy
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/rank_graph
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/resource/engine
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel/device

    ${HCCL_BASE_DIR}/framework/hcom/
    ${HCCL_BASE_DIR}/framework/hcom/gradient_segment
    ${HCCL_BASE_DIR}/framework/inc/
    ${HCCL_BASE_DIR}/framework/
    ${HCCL_BASE_DIR}/framework/nslbdp/
    ${HCCL_BASE_DIR}/framework/op_base/src
    ${HCCL_BASE_DIR}/framework/group

    ${HCCL_BASE_DIR}/common/
    ${HCCL_BASE_DIR}/common/health/
    ${HCCL_BASE_DIR}/common/stream/
    ${HCCL_BASE_DIR}/common/launch_device/

    ${HCCL_BASE_DIR}/platform/resource/socket
    ${HCCL_BASE_DIR}/platform/inc
    ${HCCL_BASE_DIR}/platform/inc/adapter

    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads/slaves/
    ${HCCL_BASE_DIR}/framework/next/coll_comms/
    ${HCCL_BASE_DIR}/framework/next/coll_comms/communicator
    ${HCCL_BASE_DIR}/framework/next/coll_comms/rank
    ${HCCL_BASE_DIR}/framework/next/coll_comms/rank/comm_mems/
    ${HCCL_BASE_DIR}/framework/next/coll_comms/rank_pairs
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/profiling
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/profiling/aicpu
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/profiling/host
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/taskException
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/taskException/aicpu
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/taskException/host
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx/cluster_monitor
    ${HCCL_BASE_DIR}/framework/next/coll_comms/common/loggers
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/channels
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/channels/aicpu/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/channels/aicpu/device/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoints/server_socket/

    # ccu 头文件
    ${HCCL_BASE_DIR}/framework/next/comms/adpt
    ${HCCL_BASE_DIR}/framework/next/comms/api_c_adpt

    ${HCCL_BASE_DIR}/framework/next/comms/ccu/pub_inc/
    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_device/
    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_device/ccu_comp
    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel
    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_pfe
    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_channel_ctx_v1

    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_transport/
    ${HCCL_BASE_DIR}/framework/next/comms/ccu/ccu_kernel
    ${HCCL_BASE_DIR}/framework/next/comms/common/device/
)

target_include_directories(hcomm PRIVATE
    ${hccl_include_list}
)

if(BUILD_OPEN_PROJECT)

    target_compile_definitions(hcomm PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP)

    target_include_directories(hcomm PRIVATE
        ${HCCL_BASE_DIR}/pub_inc
        ${HCCL_BASE_DIR}/pub_inc/new/
        ${HCCL_BASE_DIR}/../include
        ${HCCL_BASE_DIR}/../include/hccl
        ${HCCL_BASE_DIR}/../pkg_inc/
        ${HCCL_BASE_DIR}/../pkg_inc/hccl/
        ${HCCL_BASE_DIR}/../pkg_inc/hcomm/ccu
        ${HCCL_BASE_DIR}/platform/common
        ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/include/legacy
        ${HCCL_BASE_DIR}/platform/legacy/inc
        ${RDMA_CORE_INCLUDE_DIR}
        ${HCCL_BASE_DIR}/platform/hccp/inc/
        ${HCCL_BASE_DIR}/platform/resource/dispatcher_ctx
        ${HCCL_BASE_DIR}/platform/hccp/inc/network
        ${URMA_INCLUDE_DIR}

        ${HCCL_BASE_DIR}/platform/inc/adapter
        
        ${THIRD_PARTY_NLOHMANN_PATH}

        ${HCCL_BASE_DIR}/common/debug/profiling/inc
        ${HCCL_BASE_DIR}/common/debug/profiling/inc/host
        ${HCCL_BASE_DIR}/common/debug/config
        ${HCCL_BASE_DIR}/legacy/interface
        ${HCCL_BASE_DIR}/legacy/framework/topo/new_topo_builder/rank_graph

        # runtime头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/

        # mmpa头文件
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/

        # acl头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/acl/

        # driver头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/

        # highlevel_api
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/

        # 包间接口
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/

        ${THIRD_PARTY_NLOHMANN_PATH}

        ${HCCL_BASE_DIR}/framework/op_base/src
    )

    target_link_directories(hcomm PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
else()
    target_include_directories(hcomm PRIVATE
        ${TOP_DIR}/hcomm/include/hccl/                      # ${ASCEND_CANN_PACKAGE_PATH}/include
        ${TOP_DIR}/hcomm/src/algorithm/pub_inc
        ${TOP_DIR}/abl/msprof/inc                           # experiment/msprof/
        ${TOP_DIR}/ace/npuruntime/inc                       # experiment/runtime/
        ${TOP_DIR}/inc/driver                               # experiment/ascend_hal/driver/
        ${TOP_DIR}/inc/                                     # experiment/ascend_hal/driver/
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/hcomm/
        ${HCCL_BASE_DIR}/common/debug/profiling/inc
        ${HCCL_BASE_DIR}/common/debug/profiling/inc/host
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/debug/config
        ${TOP_DIR}/ace/npuruntime/inc/runtime
        ${TOP_DIR}/runtime/include/external

        ${TOP_DIR}/abl/msprof/inc/toolchain
        ${HCCL_BASE_DIR}/platform/common
        ${HCCL_BASE_DIR}/inc
        ${TOP_DIR}/inc/aicpu
        ${HCCL_BASE_DIR}/pkg_inc

        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc

        ${HCCL_BASE_DIR}/../pkg_inc/hcomm/ccu
    )
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

target_compile_options(hcomm PRIVATE
    -Werror
    -Wfloat-equal
    -Wall
    -fno-common
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
    -fstack-protector-all
    $<$<CONFIG:Debug>:-g>
)

target_link_options(hcomm PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -s
)

target_compile_definitions(hcomm PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

if(BUILD_OPEN_PROJECT)
    target_link_libraries(hcomm
            -Wl,--no-as-needed
            c_sec
            unified_dlog
            acl_rt
            hccl_alg
            hccl_plf
            -Wl,--as-needed
            -lrt
            -ldl
            -lpthread
    )
else()
    target_link_libraries(hcomm
            $<BUILD_INTERFACE:kernel_tiling_headers>
            -Wl,--no-as-needed
            c_sec
            unified_dlog
            mmpa
            hccl_alg
            hccl_plf
            -Wl,--as-needed
            -lrt
            -ldl
            -lpthread
    )
endif()

install(TARGETS hcomm
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)

message(STATUS "framework KERNEL_MODE=${KERNEL_MODE}")
message(STATUS "framework BUILD_OPEN_PROJECT=${BUILD_OPEN_PROJECT}")
message(STATUS "framework ASCEND_CANN_PACKAGE_PATH=${ASCEND_CANN_PACKAGE_PATH}")

set(ORION_HEAD_LIST)
FILE(GLOB_RECURSE sources_list LIST_DIRECTORIES TRUE ${HCCL_V2_CODE_ROOT})

foreach (dir ${sources_list})
    IF ((IS_DIRECTORY ${dir}) AND (${dir} MATCHES "src/legacy") AND (NOT ${dir} MATCHES "src/legacy/mdpi"))
        list(APPEND ORION_HEAD_LIST ${dir})
    endif()
endforeach()

include(ccl_kernel.cmake)

target_include_directories(hcomm PRIVATE ${ORION_HEAD_LIST})
add_subdirectory(${HCCL_BASE_DIR}/legacy ${CMAKE_CURRENT_BINARY_DIR}/legacy)
add_dependencies(hcomm hccl_v2 hccl_aiv_op_910_95)
target_link_libraries(hcomm
    hccl_v2
)

add_subdirectory(common)
add_subdirectory(communicator)
add_subdirectory(hcom)
add_subdirectory(op_base)
add_subdirectory(group)
add_subdirectory(cluster_maintenance)
add_subdirectory(nslbdp)
add_subdirectory(next)

if(NOT KERNEL_MODE)
    SET(HCCL_CMAKE_DIR ${HCCL_OPEN_CODE_BASE_DIR}/../cmake/)
    message(STATUS "HCCL_CMAKE_DIR = ${HCCL_CMAKE_DIR}")
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
        COMMAND ${HI_PYTHON} ${HCCL_CMAKE_DIR}/scripts/parser_ini.py ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/ccl_kernel.ini ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(ccl_kernel_json DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/config ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        COMMAND ${HI_PYTHON} ${HCCL_BASE_DIR}/../cmake/scripts/parser_ini.py ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/aicpu_custom.ini ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(aicpu_custom_json DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json
        COMMAND ${HI_PYTHON} ${HCCL_BASE_DIR}/../cmake/scripts/parser_ini.py ${CMAKE_CURRENT_SOURCE_DIR}/../legacy/framework/communicator/hostdpu/ccl_dpu.ini ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(dpu_kernel_json DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libccl_dpu.json
        DESTINATION ${INSTALL_DPU_KERNEL_JSON_DIR} ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
    add_dependencies(hcomm ccl_kernel_json aicpu_custom_json dpu_kernel_json)
endif()

include(aicpu_custom.cmake)
