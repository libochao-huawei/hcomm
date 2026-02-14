# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_library(hccp_stub SHARED
    hccp_stub.cc
    sec_stub.cc
    dl_func_stub.cc
)

target_include_directories(hccp_stub PRIVATE

        ${HCCP_DIR}/rdma_agent/inc
        ${HCCP_DIR}/rdma_agent/hdc
        ${HCCP_DIR}/rdma_agent/hdc/async
        ${HCCP_DIR}/rdma_agent/comm
        ${HCCP_DIR}/rdma_agent/peer
        ${HCCP_DIR}/rdma_agent/client/
        ${HCCP_DIR}/inc/network
        ${HCCP_DIR}/inc/private/network
        ${HCCP_DIR}/common/

        ${HCCP_DIR}/external_depends/rdma-core/include
        ${HCCP_DIR}/external_depends/host_lite

        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/include/toolchain/
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime
        ${ASCEND_CANN_PACKAGE_PATH}/include/
)
