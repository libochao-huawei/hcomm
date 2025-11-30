# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_utest_topo_detect_list
    main.cc
    # ut_topoinfo_exchange_agent.cc
)
add_executable(hccl_utest_topo_detect  ${hccl_utest_topo_detect_list})
set_legacy_protobuf()


target_include_directories(hccl_utest_topo_detect  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_topo_detect    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_topo_detect   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(hccl_utest_topo_detect  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:atrace_headers>
    $<BUILD_INTERFACE:kernel_tiling_headers> 
    c_sec_static
    ascend_protobuf
    hccl_llt
    hccl_llt_open
    -lrt
    -ldl
#    exe_graph
)

add_dependencies(hccl_utest_topo_detect hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_topo_detect
   TASK_NUM 1
)