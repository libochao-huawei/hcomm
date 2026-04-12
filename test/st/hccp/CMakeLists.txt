# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(HCCL_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(HCCP_DIR ${HCCL_CODE_ROOT}/src/platform/hccp)
set(TEST_DIR ${HCCL_CODE_ROOT}/test)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


file(GLOB_RECURSE HCCP_COMMON_SRC_LIST "${HCCP_DIR}/common/*.c")
file(GLOB_RECURSE RDMA_SERVICE_SRC_LIST "${HCCP_DIR}/rdma_service/*.c")
file(GLOB_RECURSE HCCP_SERVICE_SRC_LIST "${HCCP_DIR}/hccp_service/*.c")
file(GLOB_RECURSE RDMA_AGENT_SRC_LIST "${HCCP_DIR}/rdma_agent/client/*.c")
file(GLOB_RECURSE RDMA_PEER_SRC_LIST "${HCCP_DIR}/rdma_agent/peer/*.c")
file(GLOB_RECURSE RDMA_HDC_SRC_LIST "${HCCP_DIR}/rdma_agent/hdc/*.c")
file(GLOB_RECURSE RDMA_ADP_SRC_LIST "${HCCP_DIR}/rdma_agent/adapter/*.c")
file(GLOB_RECURSE RDMA_COMMON_SRC_LIST "${HCCP_DIR}/rdma_agent/comm/*.c")
list(FILTER RDMA_SERVICE_SRC_LIST EXCLUDE REGEX "^${HCCP_DIR}/rdma_service/ctx/product/ascend910_96/.*")
list(FILTER RDMA_SERVICE_SRC_LIST EXCLUDE REGEX "^${HCCP_DIR}/rdma_service/ctx/product/stub/.*")

include(${HCCL_CODE_ROOT}/cmake/hcomm_utils.cmake)
include(${HCCL_CODE_ROOT}/cmake/third_party/openssl.cmake)
include(${HCCL_CODE_ROOT}/cmake/third_party/ubengine.cmake)
add_subdirectory(stub)
add_subdirectory(rdma_service)
add_subdirectory(rdma_agent)
add_subdirectory(hccp_service)
add_subdirectory(test_case)
add_dependencies(hccp_stub ssl_static tls_adp)
