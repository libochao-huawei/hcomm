set(hccl_utest_mc2_misc_list
    main.cc
    ut_mc2_sqe_context_utest.cc
    ut_hccl_retry_utest.cc
)
add_executable(hccl_utest_mc2_misc  ${hccl_utest_mc2_misc_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_mc2_misc  PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_utest_mc2_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_mc2_misc   PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O1 -g
)

target_link_libraries(hccl_utest_mc2_misc  PRIVATE
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
    hccl_llt
    hccl_llt_open
    -lrt
    -ldl
#    exe_graph
)

add_dependencies(hccl_utest_mc2_misc hccl_llt hccl_llt_open)
run_llt_test(
   TARGET  hccl_utest_mc2_misc
   TASK_NUM 1
)