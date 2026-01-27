set(hccl_utest_inter_executor_inter_list
    main.cc
    ut_executor_inter_bcast_halvingdoubling.cc
    ut_executor_inter_bcast_recursive_hd.cc
    ut_executor_inter_broadcast_nb.cc
    ut_executor_inter_broadcast_nb_binary.cc
    ut_executor_inter_broadcast_nhr_v1.cc
    ut_executor_inter_broadcast_nhr.cc
    ut_executor_inter_broadcast.cc
    ut_executor_inter_gather_mesh.cc
    ut_executor_inter_reduce_recursive_hd.cc
    ut_executor_inter_gather_ring.cc
    ut_executor_inter_reduce_scatter_halvingdoubling.cc
    ut_executor_inter_reduce_scatter_mesh.cc
    ut_executor_inter_reduce_scatter_nhr_v1.cc
    ut_executor_inter_reduce_scatter_nhr.cc
    ut_executor_inter_reduce_scatter_recursive_hd.cc
    ut_executor_inter_reduce_scatter.cc
    ut_executor_inter_reduce.cc
    ut_executor_inter_scatter_mesh.cc
    ut_executor_inter_scatter_nb.cc
    ut_executor_inter_send_receive.cc
    ut_executor_inter_scatter_double_ring_direct.cc
    ut_executor_inter_alltoallvc_staged.cc
    
)
add_executable(hccl_utest_inter_executor_inter  ${hccl_utest_inter_executor_inter_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_inter_executor_inter  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_inter_executor_inter    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_inter_executor_inter   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1
)

target_link_libraries(hccl_utest_inter_executor_inter  PRIVATE
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

add_dependencies(hccl_utest_inter_executor_inter hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_inter_executor_inter
   TASK_NUM 1
)
