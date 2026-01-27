set(hccl_stest_hcom_list
    main.cc
    st_nslbdp.cc
    st_hccl_inter_new.cc
    st_hccl_inter_reduce_ffts.cc
    st_hccl_kernel_builder.cc
    st_hccl_kernel_info.cc
    st_hccl_transport_ibv.cc
    st_hcom_kernel_builder.cc
    st_hcom_lowering.cc
    st_hcom.cc
    st_hcom_graph_mc2.cc
    st_hcom_launch_kernel.cc
    st_superFastKernel.cc
    st_hccl_ffts_graph_v2.cc
    st_comm_config.cc
    st_comm_configer.cc
    st_detect_connect_anomalies.cc
    st_hccl_error_manager.cc
)
add_executable(hccl_stest_hcom  ${hccl_stest_hcom_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_hcom  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")


target_compile_definitions(hccl_stest_hcom    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_hcom PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_hcom  PRIVATE
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

add_dependencies(hccl_stest_hcom hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_hcom
    TASK_NUM 1
)