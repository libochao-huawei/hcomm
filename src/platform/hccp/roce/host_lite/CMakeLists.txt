# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
add_library(ascend_rdma_lite SHARED
    hns_roce_lite.c
    hns_roce_lite_stdio.c
    rdma_lite.c)

target_include_directories(ascend_rdma_lite PRIVATE
    # ${TOP_DIR}/inc/driver
    ${TOP_DIR}/hccl/src/platform/hccp/roce/roce_user
    ${TOP_DIR}/hccl/src/platform/hccp/inc
    ${TOP_DIR}/hccl/src/platform/hccp/inc/network
    ${TOP_DIR}/hccl/src/platform/hccp/inc/private/network
)

target_compile_options(ascend_rdma_lite PRIVATE
    -O2
    -Werror
    -fno-strict-aliasing
    -fsigned-char
    -fno-common
)

target_link_options(ascend_rdma_lite PRIVATE
    -Wl,-Bsymbolic
    -rdynamic
    -Wl,--no-undefined
)

target_compile_definitions(ascend_rdma_lite PRIVATE
    HNS_ROCE_DEVICE
     $<$<STREQUAL:${PRODUCT_SIDE},host>:LOG_HOST>)

target_link_libraries(ascend_rdma_lite PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    -Wl,--no-as-needed
    slog
    c_sec
    ascend_hal
    -Wl,--as-needed
    ofed_headers
)

install(TARGETS ascend_rdma_lite
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)
