set(hccl_stest_comm_list
    main.cc
    st_adapter.cc
    st_alg_registry.cc
    st_coll_alg_operator.cc
    st_allgather_common.cc
    st_allreduce_mesh_opbase.cc
    st_alltoall_aiv.cc
    st_alltoallv_staged.cc
    st_autotuning_hccl_kernel_builder.cc
    st_callback_thread_manager.cc
    st_cltm.cc
    st_comm_base.cc
    st_comm_factory.cc
    st_comm_halving_doubling.cc
    st_comm_inner.cc
    st_comm_mesh.cc
    st_comm_p2p.cc
    st_comm_ring.cc
    st_command_handle.cc
    st_component_reducer.cc
    st_dispatcher_ffts.cc
    st_eletwise.cc
    st_evaluator.cc
    st_hccl_comm_910B.cc
    st_hccl_graph_optimizer.cc
    st_hccl_impl_alg_double_ring.cc
    st_hccl_impl_alg_nhr.cc
    st_hccl_impl_alg_nb.cc
    st_hccl_impl_alg_ahc.cc
    st_hccl_impl_alg.cc
    st_hcom_executor.cc
    st_hccl_impl_alg_double_ring_concurrent.cc
    st_hccl_impl_alg_double_ring_concurrent_allreduce.cc
    st_allgather_mix_op_base.cc
    st_comm_init.cc
)
add_executable(hccl_stest_comm  ${hccl_stest_comm_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_comm  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_comm    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_comm PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_comm  PRIVATE
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
)

add_dependencies(hccl_stest_comm hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_comm
    TASK_NUM 1
)