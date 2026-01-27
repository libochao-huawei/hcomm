set(hccl_mpi_stest_misc_list
    main.cc
    st_mpi_send_receive.cc
    st_mpi_hccl_aicpu_kernel.cc
    st_mpi_tbe_aicore.cc
    st_mpi_rma_buffer.cc
    st_comm_star.cc
    st_mpi_topo_detect.cc
    st_mpi_transport_device_ibverbs.cc
)

add_executable(hccl_mpi_stest_misc  ${hccl_mpi_stest_misc_list} ${hccl_src_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_mpi_stest_misc PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_mpi_stest_misc  PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_link_directories(hccl_mpi_stest_misc  PRIVATE
    ${TOP_DIR}/llt/third_party/mpi/lib
)

target_link_libraries(hccl_mpi_stest_misc  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:msprof_headers>
    $<BUILD_INTERFACE:runtime_headers>
    $<BUILD_INTERFACE:cce_headers>
    $<BUILD_INTERFACE:network_headers>
    $<BUILD_INTERFACE:atrace_headers>
    c_sec_static
    libmpi.a
    ascend_protobuf
    hccl_llt
    hccl_llt_open
    -lrt
    -ldl
    -fno-inline
)

add_dependencies(hccl_mpi_stest_misc hccl_llt hccl_llt_open)
run_llt_test(
    TARGET  hccl_mpi_stest_misc
    TASK_NUM 2
)