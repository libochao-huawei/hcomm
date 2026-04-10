# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_kfc_aicpu_server.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_kfc_dispatcher.cc
)

set(OPEN_HCCL_INCLUDE_ROOT
    ${CMAKE_CURRENT_SOURCE_DIR}/../open_hccl_include
)

set(open_src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_kfc_open_kernel_adapter.cc
)

set(open_exec_src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/comm_kfc_open_kernel_executor.cc
)

add_library(ccl_kernel_decoupler OBJECT ${src_list})
set_target_properties(ccl_kernel_decoupler PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(ccl_kernel_decoupler PRIVATE
    $<TARGET_PROPERTY:ccl_kernel,COMPILE_DEFINITIONS>
)

target_compile_options(ccl_kernel_decoupler PRIVATE
    $<TARGET_PROPERTY:ccl_kernel,COMPILE_OPTIONS>
)

target_include_directories(ccl_kernel_decoupler PRIVATE
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/inc
    ${HCCL_BASE_DIR}/legacy/unified_platform/resource/stream/aicpu
    $<TARGET_PROPERTY:ccl_kernel,INCLUDE_DIRECTORIES>
)

add_library(ccl_kernel_decoupler_open OBJECT ${open_src_list})
set_target_properties(ccl_kernel_decoupler_open PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(ccl_kernel_decoupler_open PRIVATE
    $<TARGET_PROPERTY:ccl_kernel,COMPILE_DEFINITIONS>
)

target_compile_options(ccl_kernel_decoupler_open PRIVATE
    $<TARGET_PROPERTY:ccl_kernel,COMPILE_OPTIONS>
)

target_include_directories(ccl_kernel_decoupler_open BEFORE PRIVATE
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/inc
    ${OPEN_HCCL_INCLUDE_ROOT}/common
    ${OPEN_HCCL_INCLUDE_ROOT}/common/hcomm_dlsym
    ${HCCL_BASE_DIR}/../pkg_inc/hcomm/ccu
    $<TARGET_PROPERTY:ccl_kernel,INCLUDE_DIRECTORIES>
)

add_library(ccl_kernel_decoupler_open_exec OBJECT ${open_exec_src_list})
set_target_properties(ccl_kernel_decoupler_open_exec PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(ccl_kernel_decoupler_open_exec PRIVATE
    $<TARGET_PROPERTY:ccl_kernel,COMPILE_DEFINITIONS>
)

target_compile_options(ccl_kernel_decoupler_open_exec PRIVATE
    $<TARGET_PROPERTY:ccl_kernel,COMPILE_OPTIONS>
)

target_include_directories(ccl_kernel_decoupler_open_exec BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/inc
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/topo
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/executor
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/executor/channel
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/executor/registry
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/template
    ${OPEN_HCCL_INCLUDE_ROOT}/ops/op_common/template/registry
    ${OPEN_HCCL_INCLUDE_ROOT}/common
    ${OPEN_HCCL_INCLUDE_ROOT}/common/hcomm_dlsym
    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/include
    ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/include/hccl
    ${HCCL_BASE_DIR}/../include
    ${HCCL_BASE_DIR}/../include/hccl
    ${HCCL_BASE_DIR}/../pkg_inc
    ${HCCL_BASE_DIR}/../pkg_inc/hccl
    ${HCCL_BASE_DIR}/../pkg_inc/hcomm/ccu
    ${HCCL_BASE_DIR}/platform/hccp/external_depends/inc/log
)

target_sources(ccl_kernel PRIVATE
    $<TARGET_OBJECTS:ccl_kernel_decoupler>
    $<TARGET_OBJECTS:ccl_kernel_decoupler_open>
    $<TARGET_OBJECTS:ccl_kernel_decoupler_open_exec>
)
