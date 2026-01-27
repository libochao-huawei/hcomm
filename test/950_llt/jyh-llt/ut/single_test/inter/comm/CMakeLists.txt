set(hccl_utest_inter_comm_list
    main.cc
    ut_hccl_bcast.cc
    ut_alg_registry.cc
    ut_coll_alg_operator.cc
    ut_allgather_common.cc
    ut_autotuning_hccl_kernel_builder.cc
    ut_coll_batch_send_recv_executor.cc
    ut_coll_batch_send_recv_retry_executor.cc
    ut_coll_executor_allgather.cc
    ut_coll_executor_reduce_scatter.cc
    ut_coll_executor_send.cc
    ut_coll_executor_allreduce.cc
    ut_comm_base.cc
    ut_comm_factory.cc
    ut_comm_halving_doubling.cc
    ut_comm_mesh.cc  
    ut_comm_p2p.cc
    ut_comm_ring.cc
    ut_component_reducer.cc
    ut_component_sender.cc
    ut_evaluator.cc
    ut_eletwise.cc
    ut_gradient_segment.cc
    ut_gradient_split.cc
    ut_hccl_impl_common_new.cc
    ut_hccl_impl_common.cc
)
add_executable(hccl_utest_inter_comm  ${hccl_utest_inter_comm_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_inter_comm  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_inter_comm    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_inter_comm   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1
)

target_link_libraries(hccl_utest_inter_comm  PRIVATE
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

add_dependencies(hccl_utest_inter_comm hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_inter_comm
   TASK_NUM 1
)
