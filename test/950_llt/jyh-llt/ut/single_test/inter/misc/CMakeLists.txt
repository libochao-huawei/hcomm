set(hccl_utest_inter_misc_list
    main.cc
    ut_hcom_graph_mc2.cc
    ut_mem_host.cc
    ut_mem_device.cc
    ut_stream.cc
    ut_op_base_stream_manager.cc
    ut_offload_stream_manager.cc
    ut_workspace_mem.cc
    ut_notify_pool.cc
    ut_transport_virtural.cc
    ut_hccl_transport_heterog_p2p.cc
    ut_hccl_transport_p2p.cc
    ut_hccl_transport_tcp.cc
    ut_hccl_inter_new.cc
    ut_nonuniform_hierarchical_ring_v1_base.cc
    ut_reducescatter_common.cc
    ut_hvd_adapter.cc
    ut_threadManage.cc
    ut_hccl_kernel_builder.cc
    ut_hvd_kernel_builder.cc
    ut_heterog_send_recv.cc
    ut_stream_active_manager.cc
    ut_rank_table.cc
    ut_hvd_graph_optimizer.cc
    ut_hvd_ops_kernel_info_store.cc
    ut_prepare_slice.cc
    ut_coll_executor_broadcast.cc
    ut_coll_executor_reduce.cc
    ut_op_counter.cc
    ut_hccl_comm_aicpuunfold.cc
    ut_coll_executor_muli_qp.cc
    ut_hccl_transport_ibv_exp.cc
)
add_executable(hccl_utest_inter_misc  ${hccl_utest_inter_misc_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_inter_misc  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_inter_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_inter_misc   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1
)

target_link_libraries(hccl_utest_inter_misc  PRIVATE
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

add_dependencies(hccl_utest_inter_misc hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_inter_misc
   TASK_NUM 1
)
