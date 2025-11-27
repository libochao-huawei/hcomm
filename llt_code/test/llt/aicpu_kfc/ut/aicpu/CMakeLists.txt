set(aicpu_kfc_utest_aicpu_list
    main.cc
    ut_aicpu_kfc_interface_utest.cc
    ut_aicpu_kfc_trace_utest.cc
    ut_aicpu_kfc_unfold_utest.cc
)
add_executable(aicpu_kfc_utest_aicpu  ${aicpu_kfc_utest_aicpu_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(aicpu_kfc_utest_aicpu  PRIVATE
    ${include_list}
)

target_compile_definitions(aicpu_kfc_utest_aicpu    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(aicpu_kfc_utest_aicpu   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(aicpu_kfc_utest_aicpu  PRIVATE
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
    mmpa
    aicpu_kfc_llt
    aicpu_kfc_llt_open
    -lrt
    -ldl
#    exe_graph
)

add_dependencies(aicpu_kfc_utest_aicpu aicpu_kfc_llt aicpu_kfc_llt_open)
run_llt_test(
   TARGET  aicpu_kfc_utest_aicpu
   TASK_NUM 1
)