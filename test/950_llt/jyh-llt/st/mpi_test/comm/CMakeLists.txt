set(hccl_mpi_stest_comm_list
    main.cc
    st_mpi_reduce_scatter_pipeline.cc
    st_mpi_all_reduce_pipeline.cc
    st_mpi_hostnic.cc
    st_mpi_sal.cc
    st_mpi_transport_p2p.cc
    st_mpi_transport_inverbs.cc
    st_mpi_transport_ibv_exp.cc
    st_mpi_transport_shm_event.cc
    st_mpi_comm_factory.cc
    st_mpi_comm_inner.cc
    st_mpi_comm_ring.cc
    st_mpi_comm_halving_doubling.cc
    st_mpi_allgather_pipeline_opbase.cc
    st_mpi_raw.cc
    st_mpi_allgather_mix.cc
)

add_executable(hccl_mpi_stest_comm  ${hccl_mpi_stest_comm_list} ${hccl_src_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_mpi_stest_comm PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_mpi_stest_comm  PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_link_directories(hccl_mpi_stest_comm  PRIVATE
    ${TOP_DIR}/llt/third_party/mpi/lib
)

target_link_libraries(hccl_mpi_stest_comm  PRIVATE
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

add_dependencies(hccl_mpi_stest_comm hccl_llt hccl_llt_open)
run_llt_test(
    TARGET  hccl_mpi_stest_comm
    TASK_NUM 2
)
