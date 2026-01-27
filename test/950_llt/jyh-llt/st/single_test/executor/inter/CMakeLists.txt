set(hccl_stest_executor_inter_list
    main.cc
    st_executor_inter_all_reduce_doubling_direct.cc
    st_executor_inter_all_reduce_doubling.cc
    st_executor_inter_all_reduce_nb.cc
    st_executor_inter_all_reduce_nhr_v1.cc
    st_executor_inter_all_reduce_nhr.cc
    st_executor_inter_all_reduce_recursive_hd.cc
    st_executor_inter_alltoallv_staged_ffts.cc
    st_executor_inter_bcast_recursive_hd.cc
    st_executor_inter_broadcast_nb.cc
    st_executor_inter_broadcast_nb_binary.cc
    st_executor_inter_broadcast_nhr_v1.cc
    st_executor_inter_broadcast_nhr.cc
    st_executor_inter_reduce_recursive_hd.cc
    st_executor_inter_reduce_scatter_nhr_v1.cc
    st_executor_inter_reduce_scatter_nhr.cc
    st_executor_inter_reduce_scatter_recursive_hd.cc
    st_executor_inter_scatter_nb.cc
    st_executor_inter_scatter_ring.cc
    st_executor_inter_send_receive.cc
    st_executor_inter_scatter_double_ring_direct.cc

)
add_executable(hccl_stest_executor_inter  ${hccl_stest_executor_inter_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_executor_inter  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_executor_inter    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_executor_inter PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_executor_inter  PRIVATE
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

add_dependencies(hccl_stest_executor_inter hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_executor_inter
    TASK_NUM 1
)