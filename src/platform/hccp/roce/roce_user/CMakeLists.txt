# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
add_library(hns-rdmav17 SHARED 
    hns_roce_u_stdio.c
    hns_roce_u.c
    hns_roce_u_hw_v2.c
    hns_roce_u_hw_v2_opreation.c
    hns_roce_u_hw_v2_send.c
    hns_roce_u_qp.c
    hns_roce_u_db.c
    hns_roce_u_hw_v2_cq.c
    hns_roce_u_verbs.c
    verbs_exp.c
    hns_roce_u_buf.c
    hns_roce_u_hw_v2_qp.c
    hns_roce_u_mm.c
    hns_roce_u_cmd.c)

target_include_directories(hns-rdmav17 PRIVATE
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/hccl/src/platform/hccp/inc
    ${TOP_DIR}/hccl/src/platform/hccp/inc/network
    ${TOP_DIR}/hccl/src/platform/hccp/inc/private/network
)

target_compile_options(hns-rdmav17 PRIVATE
    -O2
    -Werror
    -fno-strict-aliasing
    -fsigned-char
    -fno-common
)

target_link_options(hns-rdmav17 PRIVATE
    -Wl,-Bsymbolic
    -rdynamic
    -Wl,--no-undefined
)

target_compile_definitions(hns-rdmav17 PRIVATE
    HNS_ROCE_DEVICE
     $<$<STREQUAL:${PRODUCT_SIDE},host>:LOG_HOST>)

target_link_libraries(hns-rdmav17 PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    -Wl,--no-as-needed
    ibverbs
    slog
    c_sec
    ascend_hal
    -Wl,--as-needed
)

install(TARGETS  hns-rdmav17
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)
