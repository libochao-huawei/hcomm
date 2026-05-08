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

message(STATUS "HCCL_OPEN_CODE_BASE_DIR = ${HCCL_OPEN_CODE_BASE_DIR}")
message(STATUS "HCCL_BASE_DIR = ${HCCL_BASE_DIR}")

# 定义 hcomm 库，在 host 侧使用
include(hcomm.cmake)
# 定义 ccl_kernel 库，在 device 侧使用
include(ccl_kernel.cmake)

# 添加子目录
add_subdirectory(common)
add_subdirectory(communicator)
add_subdirectory(hcom)
add_subdirectory(op_base)
add_subdirectory(group)
add_subdirectory(cluster_maintenance)
add_subdirectory(nslbdp)
add_subdirectory(next)

if(DEVICE_MODE)
    add_subdirectory(device)
endif()

if(TARGET ccl_kernel)
    target_include_directories(ccl_kernel PRIVATE
        ${HCCL_BASE_DIR}/framework/inc
        ${HCCL_BASE_DIR}/framework/cluster_maintenance/health/heartbeat/
        ${HCCL_BASE_DIR}/framework/cluster_maintenance/detect/detect_connect_anomalies/
        ${HCCL_BASE_DIR}/framework/cluster_maintenance/snapshot/
        ${HCCL_BASE_DIR}/framework/cluster_maintenance/recovery/operator_retry
        ${HCCL_BASE_DIR}/framework/common/src
        ${HCCL_BASE_DIR}/framework/common/src/aicpu
        ${HCCL_BASE_DIR}/framework/common/src/config
        ${HCCL_BASE_DIR}/framework/common/src/task
        ${HCCL_BASE_DIR}/framework/common/src/topo
        ${HCCL_BASE_DIR}/framework/common/src/exception
        ${HCCL_BASE_DIR}/framework/hcom
        ${HCCL_BASE_DIR}/framework/op_base/src/
        ${HCCL_BASE_DIR}/framework/device/
        ${HCCL_BASE_DIR}/framework/device/aicpu_kfc
        ${HCCL_BASE_DIR}/framework/device/aicpu_kfc/inc
        ${HCCL_BASE_DIR}/framework/device/aicpu_kfc/dfx
        ${HCCL_BASE_DIR}/framework/device/common
        ${HCCL_BASE_DIR}/framework/device/inc
        ${HCCL_BASE_DIR}/framework/device/framework
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/resource
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/thread
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/rank_graph
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel/device
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/data_api
        ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/sockets/
        ${HCCL_BASE_DIR}/framework/next/comms/endpoints/
        ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads
        ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads/slaves/
        ${HCCL_BASE_DIR}/framework/next/coll_comms/communicator/aicpu
        ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx
    )
endif()

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
