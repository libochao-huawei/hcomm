# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if(BUILD_OPEN_PROJECT)
    set(hccl_stest_device_src_list
        main.cc
        st_aiv_device_stest.cc
        st_stream_device_stest.cc
        st_communicator_device_stest.cc
        st_zero_copy_device_stest.cc

        ${HCCL_OPEN_CODE_COMMON}/stream/stream_utils_aicpu.cc
        ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_aicpu.cc
        ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/hccl_comm_device.cc
        ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/hccl_communicator_device.cc
        ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/hccl_communicator_attrs_device.cc
        ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/zero_copy/zero_copy_address_mgr_device.cc # 未开源
        ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/config/env_config_aicpu.cc
        ${HCCL_OPEN_CODE_FRAMEWORK}/op_base/src/op_base_device.cc
        ${HCCL_OPEN_CODE_COMMON}/debug/profiling/task_exception_handler_device.cc
        ${HCCL_OPEN_CODE_COMMON}/debug/profiling/plugin_runner_device.cc
        ${HCCL_OPEN_CODE_ALGORITHM}/impl/hccl_aiv_device.cc
        ${HCCL_OPEN_CODE_ALGORITHM}/impl/hccl_alg_device.cc
    )
else() 
    set(hccl_stest_device_src_list
        main.cc
        st_aiv_device_stest.cc
        st_stream_device_stest.cc
        st_communicator_device_stest.cc
        st_zero_copy_device_stest.cc

        ${TOP_DIR}/hcomm/src/common/stream/stream_utils_aicpu.cc
        ${TOP_DIR}/hcomm/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_aicpu.cc
        ${TOP_DIR}/hcomm/src/framework/communicator/hccl_comm_device.cc
        ${TOP_DIR}/hcomm/src/framework/communicator/impl/hccl_communicator_device.cc
        ${TOP_DIR}/hcomm/src/framework/communicator/impl/hccl_communicator_attrs_device.cc
        ${TOP_DIR}/hcomm/src/framework/communicator/impl/zero_copy/zero_copy_address_mgr_device.cc
        ${TOP_DIR}/hcomm/src/framework/common/src/config/env_config_aicpu.cc
        ${TOP_DIR}/hcomm/src/framework/op_base/src/op_base_device.cc
        ${TOP_DIR}/hcomm/src/common/debug/profiling/task_exception_handler_device.cc
        ${TOP_DIR}/hcomm/src/common/debug/profiling/plugin_runner_device.cc
        ${TOP_DIR}/hcomm/src/algorithm/impl/hccl_aiv_device.cc
        ${TOP_DIR}/hcomm/src/algorithm/impl/hccl_alg_device.cc 
    )
endif()

add_executable(hccl_stest_device  ${hccl_stest_device_src_list})

if(BUILD_OPEN_PROJECT)
    target_include_directories(hccl_stest_device  PRIVATE
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

    target_compile_definitions(hccl_stest_device    PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
        -DOPEN_HCCL_TEST
        OPEN_BUILD_PROJECT
    )
else()
    target_include_directories(hccl_stest_device  PRIVATE
        ${include_list}
    )

    target_compile_definitions(hccl_stest_device    PRIVATE
        HALF_T=float
        google=ascend_private
        CCL_KERNEL
        -DCCL_LLT
    )
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_options(hccl_stest_device   PRIVATE
    -O0
    -g
    -U_FORTIFY_SOURCE
)

if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_link_directories(hccl_stest_device PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    find_library(LIBRARY_ASCEND_HAL libascend_hal.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib/stub)
    target_link_libraries(hccl_stest_device PRIVATE
        framework_stub
        ${LIBRARY_ASCEND_HAL}
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
        ${LIBRARY_GTEST}
        ${LIBRARY_MOCKCPP}
    )
    add_dependencies(hccl_stest_device hccl hccl_alg hccl_plf framework_stub)
else() 
    target_link_libraries(hccl_stest_device  PRIVATE
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:runtime_headers>
        $<BUILD_INTERFACE:cce_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:kernel_tiling_headers> 
        c_sec_static
        ascend_protobuf
        hccl_llt
        hccl_llt_open
        mmpa
        -lrt
        -ldl
    )
    add_dependencies(hccl_stest_device hccl_llt hccl_llt_open)
    run_llt_test(
    TARGET  hccl_stest_device
    TASK_NUM 1
    )
endif()
