set(hccl_stest_executor_misc_list
    main.cc
    st_excutor_alltoallv_direct_fullmesh.cc
    st_executor_aligned_reduce_scatter_double_ring_with_serial_local_copy.cc
    st_executor_aligned_reduce_scatter_double_ring.cc
    st_executor_reduce_scatter_ring_concurrent_direct.cc
    st_executor_alltoallv_pairwise.cc
    st_executor_scatter_ring_concurrent_direct.cc
    st_executor_allgather_ring_concurrent_direct.cc
    st_executor_aligned_allgather_double_ring.cc
    st_executor.cc
    st_executor_broadcast_oneshot.cc
    st_executor_allreduce_hd_optim.cc
    st_executor_scatter_ring_direct.cc
)
add_executable(hccl_stest_executor_misc  ${hccl_stest_executor_misc_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_executor_misc  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_executor_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_executor_misc PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_executor_misc  PRIVATE
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

add_dependencies(hccl_stest_executor_misc hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_executor_misc
    TASK_NUM 1
)