# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
add_subdirectory(all_gather)
add_subdirectory(all_gather_v)
add_subdirectory(all_reduce)
add_subdirectory(all_to_all)
add_subdirectory(broadcast)
add_subdirectory(reduce_scatter)
add_subdirectory(reduce_scatter_v)
add_subdirectory(aiv_sync)

if(TARGET ccl_kernel)
    target_include_directories(ccl_kernel PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
    )
endif()

if(PRODUCT_SIDE STREQUAL "host")
    set(AIV_TEMPLATE_TARGETS
        hccl_aiv_all_reduce_op
        hccl_aiv_all_gather_op
        hccl_aiv_all_to_all_op
        hccl_aiv_broadcast_op
        hccl_aiv_reduce_scatter_op
        hccl_aiv_reduce_scatter_v_op
        hccl_aiv_all_gather_v_op
        hccl_aiv_sync_op
        hccl_ag_superkernel_int8
        hccl_ag_superkernel_int16
        hccl_ag_superkernel_int32
        hccl_ag_superkernel_float16
        hccl_ag_superkernel_float32
        hccl_ag_superkernel_bfloat16
        hccl_ag_superkernel_uint8
        hccl_ag_superkernel_uint16
        hccl_ag_superkernel_uint32
        hccl_ag_superkernel_int64
        hccl_ag_superkernel_uint64
        hccl_ag_superkernel_float64
        hccl_sk_ag_crossnode
        hccl_ar_superkernel_int8
        hccl_ar_superkernel_int16
        hccl_ar_superkernel_int32
        hccl_ar_superkernel_float16
        hccl_ar_superkernel_float32
        hccl_ar_superkernel_bfloat16
        hccl_sk_ar_deter
        hccl_sk_ar_crossnode
        hccl_aiv_all_to_all_v_op
        hccl_aiv_all_to_all_vc_op
        hccl_a2a_superkernel_int8
        hccl_a2a_superkernel_int16
        hccl_a2a_superkernel_int32
        hccl_a2a_superkernel_float16
        hccl_a2a_superkernel_float32
        hccl_a2a_superkernel_bfloat16
        hccl_a2a_superkernel_uint8
        hccl_a2a_superkernel_uint16
        hccl_a2a_superkernel_uint32
        hccl_a2a_superkernel_int64
        hccl_a2a_superkernel_uint64
        hccl_a2a_superkernel_float64
        hccl_sk_a2a_crossnode
        hccl_rs_superkernel_int8
        hccl_rs_superkernel_int16
        hccl_rs_superkernel_int32
        hccl_rs_superkernel_float16
        hccl_rs_superkernel_float32
        hccl_rs_superkernel_bfloat16
        hccl_sk_rs_deter
        hccl_sk_rs_crossnode
    )
    foreach(tgt ${AIV_TEMPLATE_TARGETS})
        if(TARGET ${tgt})
            add_dependencies(${tgt} hccl_alg hccl_plf hcomm)
        endif()
    endforeach()
endif()
