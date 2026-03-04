# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set(hccl_utest_device_src_list
    main.cc
    ut_aiv_device_utest.cc
    ut_stream_device_utest.cc
    ut_communicator_device_utest.cc
    ut_zero_copy_device_utest.cc

    ${HCCL_CODE_ROOT}/src/common/stream/stream_utils_aicpu.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/task_exception_handler_device.cc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/plugin_runner_device.cc
    ${HCCL_CODE_ROOT}/src/framework/common/src/config/env_config_aicpu.cc
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/op_base_device.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/hccl_comm_device.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/hccl_communicator_device.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/hccl_communicator_attrs_device.cc
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/zero_copy/zero_copy_address_mgr_device.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/hccl_aiv_device.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/hccl_alg_device.cc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_aicpu.cc
)

add_executable(hccl_utest_device ${hccl_utest_device_src_list})

# 头文件搜索路径
target_include_directories(hccl_utest_device PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
)
target_compile_definitions(hccl_utest_device PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    CCL_LLT
    OPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)
target_compile_options(hccl_utest_device   PRIVATE
    -U_FORTIFY_SOURCE
    -fno-var-tracking-assignments
    -Os
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)
target_link_directories(hccl_utest_device PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)
target_link_libraries(hccl_utest_device PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    -pthread
    hccl_llt
    -Wl,--whole-archive
    -Wl,--no-whole-archi
    -lgcov
    -lrt
    -ldl
)
add_dependencies(hccl_utest_device hccl_llt)
