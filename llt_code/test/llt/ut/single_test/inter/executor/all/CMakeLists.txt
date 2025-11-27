# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_utest_inter_executor_all_list
    main.cc
    # ut_executor_inter_all_gather_halvingdoubling.cc
    # ut_executor_inter_all_gather_nb.cc
    # ut_executor_inter_all_gather_nhr_v1.cc
    # ut_executor_inter_all_gather_nhr.cc
    # ut_executor_inter_all_gather_recursive_hd.cc
    # ut_executor_inter_all_gather.cc
    # ut_executor_inter_all_reduce_doubling_direct.cc
    # ut_executor_inter_all_reduce_doubling.cc
    # ut_executor_inter_all_reduce_nb.cc
    # ut_executor_inter_all_reduce_nhr.cc
    # ut_executor_inter_all_reduce_nhr_v1.cc
    # ut_executor_inter_all_reduce_recursive_hd.cc
    # ut_executor_inter_all_reduce.cc
    # ut_executor_inter_allgather_mesh.cc
    # ut_executor_inter_alltoallv_staged_ffts.cc
    # ut_executor_inter_scatter_nhr.cc
    # ut_executor_inter_scatter_ring.cc
    # ut_executor_inter_alltoallv_staged.cc
)
add_executable(hccl_utest_inter_executor_all  ${hccl_utest_inter_executor_all_list})
#头文件搜索路径
target_include_directories(hccl_utest_inter_executor_all  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_inter_executor_all    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_inter_executor_all   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1
)

target_link_libraries(hccl_utest_inter_executor_all  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:atrace_headers>
    c_sec_static
    ascend_protobuf
    hccl_llt
    hccl_llt_open
    -lrt
    -ldl
#    exe_graph
)

add_dependencies(hccl_utest_inter_executor_all hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_inter_executor_all
   TASK_NUM 1
)
