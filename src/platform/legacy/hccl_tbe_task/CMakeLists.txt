# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(src_list
    tbe_common.cc
    tbe_vector_reduce.cc
    tbe_crack_cleared.cc
    eletwise_v1.cc
    eletwise_v2.cc
    eletwise_v3.cc
    eletwise_v4.cc
    elewise_memset.cc
    auto_tiling_rt2.cc
    tbe_gatherv2_aicore.cpp
    tbe_unsorted_segment_sum_aicore.cpp
    legacy_log.cc
)

target_sources(hccl_legacy PRIVATE
    ${src_list}
)

add_subdirectory(operator)