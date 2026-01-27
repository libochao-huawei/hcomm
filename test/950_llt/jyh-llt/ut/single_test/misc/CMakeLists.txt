set(hccl_utest_misc_list
    main.cc
    ut_send_receive.cc
    ut_opretry_conn.cc
    ut_task_profiling.cc
    ut_task_exception.cc
    ut_tcp_send_thread_pool.cc
    ut_transport_tcp.cc
    ut_transport_heterog_roce.cc
    ut_tbe_vector_reduce.cc
    ut_topoinfo_exchange_dispatcher.cc
    ut_reduce_scatter_opbase.cc
    ut_sal.cc
    ut_stub.cc
    ut_transport_roce.cc
    ut_socket_manager.cc
    ut_peterson_lock.cc
    ut_task_abort_handler.cc
    ut_transport_mem.cc
    ut_typical_sendrecv.cc
    ut_typical_mem.cc
    ut_typical_qp_manager.cc
    ut_typical_interface.cc
    ut_rankconsistent.cc
    ut_comm_init.cc
    ut_interface_multithread.cc
)
add_executable(hccl_utest_misc  ${hccl_utest_misc_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_misc  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_misc   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(hccl_utest_misc  PRIVATE
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

add_dependencies(hccl_utest_misc hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_misc
   TASK_NUM 1
)
