set(hccl_stest_coll_list
    main.cc
    st_coll_batch_send_recv_executor.cc
    st_coll_batch_send_recv_retry_executor.cc
    st_coll_send_recv_executor.cc
    st_coll_executor_allreduce.cc
    st_coll_executor_allgather.cc
    st_coll_executor_reduce_scatter.cc
    st_coll_scatter_test.cc
    st_coll_broadcast_test.cc
    st_coll_reduce_test.cc
    st_coll_executor_muli_qp.cc
    st_mpi_isendimrecv_new.cc
)
add_executable(hccl_stest_coll  ${hccl_stest_coll_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_coll  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_coll    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_coll PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_coll  PRIVATE
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

add_dependencies(hccl_stest_coll hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_coll
    TASK_NUM 1
)
