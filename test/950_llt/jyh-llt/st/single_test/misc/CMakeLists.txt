set(hccl_stest_misc_list
    main.cc
    st_sal.cc
    st_gradient_split.cc
    st_send_receive.cc
    st_wrapper.cc
    st_external_input.cc
    st_prepare_slice.cc
    st_reducescatter_common.cc
    st_hvd_kernel_builder.cc
    st_gradient_split_tune.cc
    st_opretry_conn.cc
    st_op_base.cc
    st_op_base_multithread.cc
    st_heterog_send_recv.cc
    st_rankconsistent.cc
    st_task_exception.cc
    # st_mpi_tcp_sendrecv.cc
    st_tbe_vector_reduce.cc
    st_heartbeat.cc
    st_transport_tcp.cc
    st_transport_heterog_roce.cc
    st_transport_roce.cc
    st_tcp_send_thread_pool.cc
    st_mpi_recv_task.cc
    st_p2p_mgmt.cc
    st_gradient_segment.cc
    st_stream_active_manager.cc
    st_hvd_graph_optimizer.cc
    st_hvd_ops_kernel_info_store.cc
    st_rank_table.cc
    st_transport_virtural.cc
    st_socket_manager.cc
    st_op_base_stream_manager.cc
    st_offload_stream_manager.cc
    st_stream.cc
    st_topoinfo_exchange_dispatcher.cc
    st_op_counter.cc
    st_hccl_hdc.cc
    st_opretry.cc
    st_peterson_lock.cc
    st_task_abort_handler.cc
    st_hccl_comm_aicpuunfold.cc
    st_hccl_transport_p2p.cc
    st_onesided_service.cc
    st_transport_mem.cc
    st_typical_mem.cc
    st_typical_qp_manager.cc
    st_typical_sendrecv.cc
    st_typical_interface.cc
    st_load_backup_ip.cc
    st_backup_link.cc
    st_adapter.cc
    st_switch_nic.cc
    st_switch_nic_fsm.cc
    st_interface_multithread.cc
    st_topoinfo_exchange_agent.cc
)
add_executable(hccl_stest_misc  ${hccl_stest_misc_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_misc  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_misc PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_misc  PRIVATE
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
)

add_dependencies(hccl_stest_misc hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_misc
    TASK_NUM 1
)
