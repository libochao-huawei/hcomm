# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_utest_inter_executor_inter_list
    main.cc
    ut_executor_inter_all_reduce_doubling_direct.cc
    ut_executor_inter_all_reduce_doubling.cc
    ut_executor_inter_all_reduce_nb.cc
    ut_executor_inter_all_reduce_nhr_v1.cc
    ut_executor_inter_all_reduce_nhr.cc
    ut_executor_inter_all_reduce_recursive_hd.cc
    ut_executor_inter_alltoallv_staged_ffts.cc
    ut_executor_inter_bcast_recursive_hd.cc
    ut_executor_inter_broadcast_nb.cc
    ut_executor_inter_broadcast_nb_binary.cc
    ut_executor_inter_broadcast_nhr_v1.cc
    ut_executor_inter_broadcast_nhr.cc
    ut_executor_inter_reduce_recursive_hd.cc
    ut_executor_inter_reduce_scatter_nhr_v1.cc
    ut_executor_inter_reduce_scatter_nhr.cc
    ut_executor_inter_reduce_scatter_recursive_hd.cc
    ut_executor_inter_scatter_nb.cc
    ut_executor_inter_scatter_ring.cc
    ut_executor_inter_send_receive.cc
    ut_executor_inter_scatter_double_ring_direct.cc

)
add_executable(hccl_utest_inter_executor_inter  ${hccl_utest_inter_executor_inter_list})

#头文件搜索路径
if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_include_directories(hccl_utest_inter_executor_inter  PRIVATE
        ${package_include_list}
        ${include_list}
        ${algorithm_include_list}
        ${framework_include_list}
        ${common_include_list}
        ${platform_include_list}
        ${other_include_list}
        ${THIRD_PARTY_NLOHMANN_PATH}
        ${mockcpp_INCLUDE_DIRS}
        ${HCCL_STUB_CODE_ROOT}/framework_stub
    )

    target_compile_definitions(hccl_utest_inter_executor_inter    PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
        -DOPEN_HCCL_TEST
        OPEN_BUILD_PROJECT
    )
else()
    target_include_directories(hccl_utest_inter_executor_inter  PRIVATE
        ${include_list}
    )

    target_compile_definitions(hccl_utest_inter_executor_inter    PRIVATE
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
    )
endif()
 
target_compile_options(hccl_utest_inter_executor_inter   PRIVATE
    -Os
    -g
    -U_FORTIFY_SOURCE
)

if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_link_directories(hccl_utest_inter_executor_inter PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    find_library(LIBRARY_ASCEND_HAL libascend_hal.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib/stub)
    target_link_libraries(hccl_utest_inter_executor_inter PRIVATE
        framework_stub
        ${LIBRARY_ASCEND_HAL}
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
        ${LIBRARY_GTEST}
        ${LIBRARY_MOCKCPP}
    )
    add_dependencies(hccl_utest_inter_executor_inter hccl hccl_alg hccl_plf framework_stub)
else() 
    target_link_libraries(hccl_utest_inter_executor_inter  PRIVATE
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:runtime_headers>
        $<BUILD_INTERFACE:cce_headers>
        $<BUILD_INTERFACE:atrace_headers>
        c_sec_static
        ascend_protobuf
        hccl_llt
        hccl_llt_open
        -lrt
        -ldl
    )
    add_dependencies(hccl_utest_inter_executor_inter hccl_llt hccl_llt_open)
    run_llt_test(
        TARGET  hccl_utest_inter_executor_inter
        TASK_NUM 1
    )
endif()