set (hccl_mpi_utest_comm_list
    main.cc
    ut_mpi_all_reduce_aivRdma.cc
    ut_mpi_all_reduce_pipeline.cc
    ut_mpi_reduce_scatter_pipeline.cc
    ut_mpi_reduce_scatter_aivRdma.cc
    ut_mpi_allgather_pipeline_opbase.cc
    ut_mpi_allgather_aivRdma.cc
    ut_mpi_alltoall_pipeline_opbase.cc
    ut_mpi_comm_factory.cc
    ut_mpi_comm_halving_doubling.cc
    ut_mpi_comm_ring.cc
    # ut_mpi_hostnic.cc
)

add_executable(hccl_mpi_utest_comm ${hccl_mpi_utest_comm_list})

set_legacy_protobuf()
target_compile_options(hccl_mpi_utest_comm PRIVATE
     -U_FORTIFY_SOURCE -g
)

#头文件搜索路径
target_include_directories(hccl_mpi_utest_comm PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_mpi_utest_comm  PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_link_directories(hccl_mpi_utest_comm  PRIVATE
    ${TOP_DIR}/llt/third_party/mpi/lib
)
target_link_libraries(hccl_mpi_utest_comm  PRIVATE
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

add_dependencies(hccl_mpi_utest_comm  hccl_llt hccl_llt_open)

run_llt_test(
    TARGET  hccl_mpi_utest_comm
    TASK_NUM 2
)
