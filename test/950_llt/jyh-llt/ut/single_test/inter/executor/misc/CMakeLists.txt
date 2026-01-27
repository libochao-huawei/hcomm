set(hccl_utest_inter_executor_misc_list
    main.cc
    ut_executor_allgather_ring_concurrent_direct.cc
    ut_executor_allgather_ring_direct.cc
    ut_excutor_alltoallv_direct_fullmesh.cc
    ut_executor_aligned_allgather_double_ring.cc
    ut_executor_alltoallv_pairwise.cc
    ut_executor_base.cc
    ut_executor_reduce_scatter_ring_concurrent_direct.cc
    ut_executor_aligned_reduce_scatter_double_ring_with_serial_local_copy.cc
    ut_executor_aligned_reduce_scatter_double_ring.cc
    ut_executor_scatter_ring_concurrent_direct.cc
    ut_executor_broadcast_oneshot.cc
    ut_executor_allreduce_hd_optim.cc
    ut_executor_scatter_ring_direct.cc
)
add_executable(hccl_utest_inter_executor_misc  ${hccl_utest_inter_executor_misc_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_inter_executor_misc  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_inter_executor_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_inter_executor_misc   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1
)

target_link_libraries(hccl_utest_inter_executor_misc  PRIVATE
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

add_dependencies(hccl_utest_inter_executor_misc hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_inter_executor_misc
   TASK_NUM 1
)
