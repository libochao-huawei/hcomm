set(hccl_utest_platform_aicpu_list
    main.cc
    ut_aicpu_notify_utest.cc
    ut_aicpu_transport_device_p2p_utest.cc
    ut_aicpu_dispatcher_utest.cc
    ut_aicpu_trace_utest.cc
)
add_executable(hccl_utest_platform_aicpu  ${hccl_utest_platform_aicpu_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_platform_aicpu  PRIVATE
    ${include_list}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_utest_platform_aicpu    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_platform_aicpu PRIVATE
    -O0
    -g
)

target_link_libraries(hccl_utest_platform_aicpu  PRIVATE
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

add_dependencies(hccl_utest_platform_aicpu hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_utest_platform_aicpu
    TASK_NUM 1
)
