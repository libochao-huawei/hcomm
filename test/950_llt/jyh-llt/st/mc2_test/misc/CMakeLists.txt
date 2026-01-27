set(hccl_stest_mc2_misc_list
    main.cc
    st_mc2_sqe_context_utest
    st_hccl_retry_utest.cc
    st_cann_error_report.cc
)
add_executable(hccl_stest_mc2_misc  ${hccl_stest_mc2_misc_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_mc2_misc  PRIVATE
    ${include_list}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_mc2_misc    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_mc2_misc PRIVATE
    -O0
    -g
)

target_link_libraries(hccl_stest_mc2_misc  PRIVATE
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
)

add_dependencies(hccl_stest_mc2_misc hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_mc2_misc
    TASK_NUM 1
)
