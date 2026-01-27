set(hccl_utest_hcom_list
    main.cc
    ut_detect_connect_anomalies.cc
    ut_backup_link.cc
    ut_cann_error_report.cc
    ut_coll_scatter_test.cc
    ut_comm_config.cc
    ut_comm_configer.cc
    ut_comm_star.cc
    ut_command_handle.cc
    ut_hccl_hdc.cc
    ut_hccl_transport_roce.cc
    ut_hcom_executor.cc
    ut_hcom_lowering.cc
    ut_hcom_param_check.cc
    ut_hcom_plugin.cc
    ut_heartbeat.cc
    ut_load_backup_ip.cc
    ut_multirootscatterring.cc
    ut_onesided_service.cc
    ut_op_base.cc
    ut_opretry.cc
    ut_nslbdp.cc
    ut_hcom_launch_kernel.cc
    ut_superFastKernel.cc
    ut_hccl_ffts_graph_v2.cc
    ut_switch_nic.cc
    ut_switch_nic_fsm.cc
    ut_hccl_error_manager.cc
)
add_executable(hccl_utest_hcom  ${hccl_utest_hcom_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_hcom  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_hcom    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_hcom   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(hccl_utest_hcom  PRIVATE
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
#    exe_graph
)

add_dependencies(hccl_utest_hcom hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_hcom
   TASK_NUM 1
)
