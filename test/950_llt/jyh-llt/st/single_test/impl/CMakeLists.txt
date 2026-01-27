set(hccl_stest_impl_list
    main.cc
    st_hccl_impl_common_new.cc
    st_hccl_impl_common.cc
    st_hccl_impl.cc
    st_zero_copy_memory_agent.cc
    st_preempt_port_manager.cc
    st_template_base.cc
    st_netdevice.cc
    st_global_mem_mgr.cc
    st_local_dispatcher_ctx.cc
    st_multi_qp_info.cc
)
add_executable(hccl_stest_impl  ${hccl_stest_impl_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_impl  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")


target_compile_definitions(hccl_stest_impl    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_impl PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_impl  PRIVATE
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

add_dependencies(hccl_stest_impl hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_impl
    TASK_NUM 1
)
