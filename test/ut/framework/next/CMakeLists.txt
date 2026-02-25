# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(src_to_test_list
    main.cc
    hccl_api_base_test.cc
    host/ut_cpu_roce_endpoint.cc
    host/ut_host_cpu_roce_channel.cc
    host/ut_host_rdma_connection.cc
    ut_HcclRankGraph_API.cc
    comms/comm_engine_res/threads/test_aicpu_ts_thread_class.cc
    comms/comm_engine_res/threads/test_cpu_ts_thread_class.cc
    comms/comm_engine_res/threads/test_thread_api.cc
    coll_comms/rank/comm_mems/test_getHcclBuffer_api.cc
)

add_executable(hccl_utest_test_next_resource ${src_to_test_list})

#头文件搜索路径
target_include_directories(hccl_utest_test_next_resource PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
)
 
target_compile_definitions(hccl_utest_test_next_resource PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    ORION_MODE
    OPEN_BUILD_PROJECT
)
 
target_compile_options(hccl_utest_test_next_resource PRIVATE
    -U_FORTIFY_SOURCE
    -fno-var-tracking-assignments
    -Os
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)
 
target_link_directories(hccl_utest_test_next_resource PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)
target_link_libraries(hccl_utest_test_next_resource PRIVATE
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
add_dependencies(hccl_utest_test_next_resource hccl_llt)

run_llt_test(
   TARGET  hccl_utest_test_next_resource
   TASK_NUM 1
)