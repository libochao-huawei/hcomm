set(hccl_utest_API_list
    main.cc
    hccl_api_base_test.cc
    ut_HcclCommInitClusterInfo_API_test.cc
    ut_HcclCommInitClusterInfoConfig_API_test.cc
    ut_HcclCreateSubCommConfig_API_test.cc
    ut_HcclSetConfig_API_test.cc
    ut_HcclGetConfig_API_test.cc
    ut_HcclAllReduce_API_test.cc
    ut_HcclBroadcast_API_test.cc
    ut_HcclReduceScatter_API_test.cc
    ut_HcclReduceScatterV_API_test.cc
    ut_HcclScatter_API_test.cc
    ut_HcclAllGather_API_test.cc
    ut_HcclAllGatherV_API_test.cc
    ut_HcclGetRankSize_API_test.cc
    ut_HcclGetRankId_API_test.cc
    ut_HcclBarrier_API_test.cc
    ut_HcclRecv_API_test.cc
    ut_HcclSend_API_test.cc
    ut_HcclAlltoAllV_API_test.cc
    ut_HcclAlltoAll_API_test.cc
    ut_HcclReduce_API_test.cc
    ut_HcclCommDestroy_API_test.cc
    ut_HcclGetCommAsyncError_API_test.cc
    ut_HcclGetErrorString_API_test.cc
    ut_HcclBatchSendRecv_API_test.cc
    ut_HcclGetCommConfigCapability_API_test.cc
    ut_HcclCommConfigInit_API_test.cc
    ut_HcclCommSuspend_API_test.cc
    ut_HcclCommResume_API_test.cc
)

add_executable(ut_hccl_utest_api_single_thread  ${hccl_utest_API_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(ut_hccl_utest_api_single_thread  PRIVATE
    ${include_list}
)

target_compile_definitions(ut_hccl_utest_api_single_thread    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(ut_hccl_utest_api_single_thread   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O3 -g
)

target_link_libraries(ut_hccl_utest_api_single_thread  PRIVATE
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

add_dependencies(ut_hccl_utest_api_single_thread hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  ut_hccl_utest_api_single_thread
   TASK_NUM 1
)