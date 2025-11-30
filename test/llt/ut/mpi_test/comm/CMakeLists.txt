# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_mpi_utest_comm_list
    main.cc
    ut_mpi_reduce_scatter_pipeline.cc
    ut_mpi_all_reduce_pipeline.cc
    ut_mpi_hostnic.cc
    ut_mpi_sal.cc
    ut_mpi_transport_p2p.cc
    ut_mpi_transport_inverbs.cc
    ut_mpi_transport_ibv_exp.cc
    ut_mpi_transport_shm_event.cc
    ut_mpi_comm_factory.cc
    ut_mpi_comm_inner.cc
    ut_mpi_comm_ring.cc
    ut_mpi_comm_halving_doubling.cc
    ut_mpi_allgather_pipeline_opbase.cc
    ut_mpi_raw.cc
    ut_mpi_allgather_mix.cc
)

add_executable(hccl_mpi_utest_comm  ${hccl_mpi_utest_comm_list} ${hccl_src_list})

set_legacy_protobuf()

#头文件搜索路径
if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_include_directories(hccl_mpi_utest_comm  PRIVATE
        ${package_include_list}
        ${include_list}
        ${algorithm_include_list}
        ${framework_include_list}
        ${common_include_list}
        ${platform_include_list}
        ${other_include_list}
        ${THIRD_PARTY_NLOHMANN_PATH}
        ${mockcpp_INCLUDE_DIRS}
        ${openmpi_INCLUDE_DIRS}
        ${HCCL_STUB_CODE_ROOT}/framework_stub
    )
 
    target_compile_definitions(hccl_mpi_utest_comm    PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
        -DOPEN_HCCL_TEST
        OPEN_BUILD_PROJECT
    )
else()
    target_include_directories(hccl_mpi_utest_comm PRIVATE
        ${include_list}
    )

    target_compile_definitions(hccl_mpi_utest_comm  PRIVATE
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
    )
endif()

if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_link_directories(hccl_mpi_utest_comm PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
        ${LIBRARY_OPENMPI}
    )
    find_library(LIBRARY_ASCEND_HAL libascend_hal.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib/stub)
    target_link_libraries(hccl_mpi_utest_comm PRIVATE
        framework_stub
        ${LIBRARY_ASCEND_HAL}
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
        ${LIBRARY_GTEST}
        ${LIBRARY_MOCKCPP}
        mpi
    )
    add_dependencies(hccl_mpi_utest_comm hccl hccl_alg hccl_plf framework_stub)
else() 
	target_link_directories(hccl_mpi_utest_comm  PRIVATE
	    ${TOP_DIR}/llt/third_party/mpi/lib
	)

    target_link_libraries(hccl_mpi_utest_comm  PRIVATE
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:runtime_headers>
        $<BUILD_INTERFACE:cce_headers>
        $<BUILD_INTERFACE:atrace_headers>
        c_sec_static
        libmpi.a
        ascend_protobuf
        hccl_llt
        hccl_llt_open
        -lrt
        -ldl
        -fno-inline
    )
    add_dependencies(hccl_mpi_utest_comm hccl_llt hccl_llt_open)
    run_llt_test(
    TARGET  hccl_mpi_utest_comm
    TASK_NUM 2
    )
endif()