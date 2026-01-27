set (hccl_mpi_utest_misc_list
    main.cc
    ut_mpi_send_receive.cc
    ut_mpi_topo_detect.cc
    ut_mpi_rma_buffer.cc
    ut_mpi_raw.cc
    ut_mpi_transport_shm_event.cc
    ut_mpi_transport_p2p.cc
    # ut_mpi_transport_ibv_exp.cc
    ut_mpi_sal.cc
    ut_mpi_hccl_aicpu_kernel.cc
    ut_mpi_transport_device_ibverbs.cc
)

add_executable(hccl_mpi_utest_misc ${hccl_mpi_utest_misc_list})

set_legacy_protobuf()
target_compile_options(hccl_mpi_utest_misc PRIVATE
     -U_FORTIFY_SOURCE -g
)

#头文件搜索路径
target_include_directories(hccl_mpi_utest_misc PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_mpi_utest_misc  PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_link_directories(hccl_mpi_utest_misc  PRIVATE
    ${TOP_DIR}/llt/third_party/mpi/lib
)
target_link_libraries(hccl_mpi_utest_misc  PRIVATE
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
#    exe_graph
)

add_dependencies(hccl_mpi_utest_misc hccl_llt hccl_llt_open)

run_llt_test(
    TARGET  hccl_mpi_utest_misc
    TASK_NUM 2
)
