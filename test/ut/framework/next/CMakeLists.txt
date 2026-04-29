# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 注意a3与a5测试用例拆分不同可执行文件
set(src_to_test_list
    main.cc
    hccl_api_base_test.cc
    ut_HcclRankGraph_API.cc
    ut_HcomInitCollComm.cc
    comms/aiv/ut_AivUbMemTransport_API_test.cc
    comms/ccu/ut_CcuTransport_API_test.cc
    comms/comm_engine_res/ccu/ut_ccu_res_container.cc
    comms/common/ut_orion_adpt_utils.cc
    comms/comm_engine_res/threads/test_aicpu_ts_thread_class.cc
    comms/comm_engine_res/threads/test_cpu_ts_thread_class.cc
    comms/comm_engine_res/threads/test_iaicpu_ts_thread_interface.cc
    comms/comm_engine_res/threads/test_thread_api.cc
    comms/endpoint_pairs/ut_endpoint_pair.cc
    comms/api_c_adpt/ut_hcomm_c_adpt.cc
    coll_comms/rank/ut_my_rank.cc
    coll_comms/rank/comm_mems/test_getHcclBuffer_api.cc
    coll_comms/rank/ut_my_rank_tls.cc
    comms/api_c_adpt/ut_hcomm_res_mgr_test.cc
    comms/api_c_adpt/ut_hcomm_endpoint_test.cc
    comms/api_c_adpt/ut_hcomm_endpoint_uboe_test.cc
    comms/api_c_adpt/ut_hcomm_mem_test.cc
    comms/api_c_adpt/ut_hcomm_channel_test.cc
    comms/api_c_adpt/ut_hcomm_engine_ctx_test.cc
    comms/api_c_adpt/ut_hcomm_dfx_test.cc
    coll_comms/communicator/ut_coll_comm_test.cc
    comms/endpoint_pairs/channels/ut_channel_process_test.cc
    comms/endpoint_pairs/channels/ut_channel_roce_factory.cc
    coll_comms/communicator/aicpu/ut_aicpu_indop_process_test.cc
    host/ut_cpu_roce_endpoint.cc
    host/ut_host_cpu_roce_channel.cc
    host/ut_host_rdma_connection.cc
    host/ut_server_socket_manager.cc
    comms/endpoints/ut_ub_mem_endpoint.cc
    comms/endpoints/reged_mems/ut_roce_mem_test.cc
    comms/endpoints/reged_mems/ut_urma_mem_test.cc
    coll_comms/api_c_adpt/ut_coll_comm_res_c_adpt.cc
    comms/endpoints/reged_mems/ut_aicpu_ts_roce_mem.cc
    # Ns recovery相关测试用例
    comms/endpoint_pairs/channels/aicpu/ut_aicpu_ts_urma_channel.cc
    comms/endpoint_pairs/channels/aicpu/ut_aicpu_ts_uboe_channel.cc
    comms/endpoint_pairs/channels/aicpu/ut_aicpu_ts_roce_channel.cc
    comms/endpoint_pairs/channels/aicpu/device/ut_aicpu_channel_process.cc
    comms/endpoints/ut_aicpu_ts_roce_endpoint.cc
    comms/endpoint_pairs/channels/ccu/ut_ccu_urma_channel.cc
    comms/endpoint_pairs/channels/aiv/ut_aiv_ub_mem_channel.cc
    comms/endpoint_pairs/ut_endpoint_pair_mgr.cc
    comms/ccu/ccu_device/ccu_comp/ut_ccu_comp_pub.cc
    coll_comms/communicator/ns_recovery/aicpu/ut_ns_recovery_func_lite.cc
    coll_comms/communicator/ns_recovery/aicpu/ut_ns_recovery_lite.cc
    coll_comms/rank_pairs/ut_rank_pair.cc
    coll_comms/communicator/ns_recovery/ut_hccl_task_abort_handler.cc
    coll_comms/communicator/aicpu/ut_coll_comm_aicpu.cc
    coll_comms/communicator/ns_recovery/ut_ns_recovery.cc
)

add_executable(hccl_utest_test_next_resource ${src_to_test_list})

#头文件搜索路径
target_include_directories(hccl_utest_test_next_resource PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/channels/
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

enable_testing()
add_test(NAME hccl_utest_test_next_resource COMMAND hccl_utest_test_next_resource)

set(src_to_a3_test_list
    main.cc
    hccl_api_base_test.cc
    coll_comms/rank/comm_mems/test_getHcclBuffer_a3_api.cc
)

add_executable(hccl_utest_test_next_resource_a3 ${src_to_a3_test_list})

#头文件搜索路径
target_include_directories(hccl_utest_test_next_resource_a3 PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
)
 
target_compile_definitions(hccl_utest_test_next_resource_a3 PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    ORION_MODE
    OPEN_BUILD_PROJECT
)
 
target_compile_options(hccl_utest_test_next_resource_a3 PRIVATE
    -U_FORTIFY_SOURCE
    -fno-var-tracking-assignments
    -Os
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)
 
target_link_directories(hccl_utest_test_next_resource_a3 PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)

target_link_libraries(hccl_utest_test_next_resource_a3 PRIVATE
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

add_dependencies(hccl_utest_test_next_resource_a3 hccl_llt)

enable_testing()
add_test(NAME hccl_utest_test_next_resource_a3 COMMAND hccl_utest_test_next_resource_a3)
