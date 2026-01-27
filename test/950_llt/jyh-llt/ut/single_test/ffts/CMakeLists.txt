set(hccl_utest_ffts_list
    main.cc
    ut_adapter.cc
    ut_dispatcher_ffts.cc
    ut_hccl_comm.cc
    ut_hccl_impl_alg_double_ring_concurrent.cc
    ut_zero_copy_memory_agent.cc
    ut_preempt_port_manager.cc
    ut_local_dispatcher_ctx.cc
)
add_executable(hccl_utest_ffts  ${hccl_utest_ffts_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_ffts  PRIVATE
    ${include_list}

)

target_compile_definitions(hccl_utest_ffts    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_ffts   PRIVATE
    -U_FORTIFY_SOURCE
)

target_link_libraries(hccl_utest_ffts  PRIVATE
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
add_dependencies(hccl_utest_ffts hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_ffts
   TASK_NUM 1
)
