# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_utest_inter_comm_list
    main.cc
    ut_adapter.cc
    ut_alg_registry.cc
    ut_coll_alg_operator.cc
    # st_allgather_common.cc
    ut_allreduce_mesh_opbase.cc
    ut_alltoall_aiv.cc
    ut_alltoallv_staged.cc
    ut_callback_thread_manager.cc
    ut_cltm.cc
    ut_comm_base.cc
    ut_comm_factory.cc
    ut_comm_halving_doubling.cc
    ut_comm_inner.cc
    ut_comm_mesh.cc
    ut_comm_p2p.cc
    ut_comm_ring.cc
    ut_command_handle.cc
    ut_component_reducer.cc
    ut_dispatcher_ffts.cc
    ut_eletwise.cc
    ut_hccl_comm_910B.cc
    ut_hccl_impl_alg_double_ring.cc
    ut_hccl_impl_alg_nhr.cc
    ut_hccl_impl_alg_nb.cc
    ut_hccl_impl_alg_ahc.cc
    ut_hccl_impl_alg.cc
    # st_hcom_executor.cc  构图相关
    ut_hccl_impl_alg_double_ring_concurrent.cc
    ut_hccl_impl_alg_double_ring_concurrent_allreduce.cc
    ut_allgather_mix_op_base.cc
    ut_comm_init.cc
)
add_executable(hccl_utest_inter_comm  ${hccl_utest_inter_comm_list})

set_legacy_protobuf()

#头文件搜索路径
if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_include_directories(hccl_utest_inter_comm  PRIVATE
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
        ${HCCL_OPEN_CODE_ROOT}/external_depends
    )

    target_compile_definitions(hccl_utest_inter_comm    PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
        # -DOPEN_HCCL_TEST
        OPEN_BUILD_PROJECT
    )
else()
    target_include_directories(hccl_utest_inter_comm  PRIVATE
        ${include_list}
    )

    target_compile_definitions(hccl_utest_inter_comm    PRIVATE
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
    )
endif()

target_compile_options(hccl_utest_inter_comm   PRIVATE
    -Os
    -g
    -U_FORTIFY_SOURCE
)

if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_link_directories(hccl_utest_inter_comm PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    find_library(LIBRARY_ASCEND_HAL libascend_hal.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib/stub)
    target_link_libraries(hccl_utest_inter_comm PRIVATE
        framework_stub
        ${LIBRARY_ASCEND_HAL}
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
        ${LIBRARY_GTEST}
        ${LIBRARY_MOCKCPP}
        hccl_heterog
    )
    add_dependencies(hccl_utest_inter_comm hccl hccl_alg hccl_plf framework_stub)
else() 
    target_link_libraries(hccl_utest_inter_comm  PRIVATE
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
    add_dependencies(hccl_utest_inter_comm hccl_llt hccl_llt_open)
    run_llt_test(
        TARGET  hccl_utest_inter_comm
        TASK_NUM 1
    )
endif()
