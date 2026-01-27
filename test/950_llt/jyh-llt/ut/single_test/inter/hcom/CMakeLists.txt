set(hccl_utest_inter_hcom_list
    main.cc
    ut_hccl_comm_910B_allreduce.cc
    #ut_hccl_comm_910B_nb.cc
    ut_hccl_comm_910B_reducescatter.cc
    ut_hccl_comm_910B.cc
    ut_hccl_comm.cc
    ut_hcom.cc
    ut_hccl_kernel_info.cc
    ut_hccl_graph_optimizer.cc
)
add_executable(hccl_utest_inter_hcom  ${hccl_utest_inter_hcom_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_inter_hcom  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_inter_hcom    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_inter_hcom   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1
)

target_link_libraries(hccl_utest_inter_hcom  PRIVATE
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

add_dependencies(hccl_utest_inter_hcom hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_inter_hcom
   TASK_NUM 1
)
