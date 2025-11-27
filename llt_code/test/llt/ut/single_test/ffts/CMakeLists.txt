# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_utest_ffts_list
    main.cc
    # ut_adapter.cc
    # ut_dispatcher_ffts.cc
    # ut_ffts_ctx_provider.cc
    # ut_hccl_comm.cc
    # ut_zero_copy_memory_agent.cc
    # ut_preempt_port_manager.cc
    # ut_local_dispatcher_ctx.cc
)
add_executable(hccl_utest_ffts  ${hccl_utest_ffts_list})
#头文件搜索路径
target_include_directories(hccl_utest_ffts  PRIVATE
    ${include_list}

)

target_compile_definitions(hccl_utest_ffts    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_ffts   PRIVATE
    -U_FORTIFY_SOURCE
)

target_link_libraries(hccl_utest_ffts  PRIVATE
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
add_dependencies(hccl_utest_ffts hccl_llt hccl_llt_open)

if(NOT BUILD_OPEN_PROJECT) # 蓝区构建
run_llt_test(
   TARGET  hccl_utest_ffts
   TASK_NUM 1
)
endif()
