# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(HCCL_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../)
set(HCCP_DIR ${HCCL_CODE_ROOT}/src/platform/hccp)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../)

add_subdirectory(stub)
add_subdirectory(test_case)

file(GLOB_RECURSE RDMA_CLIENT_SOURCES "${HCCL_CODE_ROOT}/src/platform/hccp/*.c")

add_library(hccp_llt SHARED
    ${RDMA_CLIENT_SOURCES}
)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/third_party/openssl-${PRODUCT_SIDE})
    include(${HCCL_CODE_ROOT}/cmake/third_party/openssl.cmake)
    add_dependencies(hccp_llt openssl_project)
endif()

include(${HCCL_CODE_ROOT}/cmake/utils.cmake)

target_include_directories(hccp_llt PUBLIC
        ${HCCP_DIR}/rdma_agent/inc
        ${HCCP_DIR}/rdma_agent/hdc
        ${HCCP_DIR}/rdma_agent/hdc/async
        ${HCCP_DIR}/rdma_agent/comm
        ${HCCP_DIR}/rdma_agent/peer
        ${HCCP_DIR}/rdma_agent/client/
        ${HCCP_DIR}/rdma_agent/adapter/tlv
        ${HCCP_DIR}/rdma_agent/adapter
        ${HCCP_DIR}/rdma_agent/adapter/async/
        ${HCCP_DIR}/rdma_service
        ${HCCP_DIR}/rdma_service/tlv
        ${HCCP_DIR}/common
        ${HCCP_DIR}/inc/network
        ${HCCP_DIR}/inc/private
        ${HCCP_DIR}/inc/private/network
        ${HCCP_DIR}/crypt/inc

        ${HCCP_DIR}/external_depends/rdma-core/include
        ${HCCP_DIR}/external_depends/rdma-core/providers/hns
        ${HCCP_DIR}/external_depends/host_lite
        ${HCCP_DIR}/external_depends/inc/driver
        ${HCCP_DIR}/external_depends/inc/log

        ${OPENSSL_INCLUDE_DIR}/
        ${TEST_DIR}/ut/depends/include/tls_adp/
        ${HCCL_CODE_ROOT}/third_party/openssl-host/include

        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/include/toolchain/
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/tsd
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        
)

target_compile_definitions(hccp_llt PRIVATE
    CCL_LLT
    HALF_T=float
    google=ascend_private
)

target_compile_options(hccp_llt   PRIVATE
    -O0
    -U_FORTIFY_SOURCE
    -g
)

target_link_options(hccp_llt   PRIVATE
    -Wl,--no-undefined
)

target_link_libraries(hccp_llt PRIVATE
    hccp_stub
    tls_adp
    -lpthread
    -lrt
)