set(hccl_stest_algorithms_8pring_list
    main.cc
    st_hccl_8pring_test.cc
)
add_executable(hccl_stest_algorithms_8pring  ${hccl_stest_algorithms_8pring_list})

set_legacy_protobuf()

#头文件搜索路径
target_include_directories(hccl_stest_algorithms_8pring  PRIVATE
    ${include_list}
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_stest_algorithms_8pring    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_stest_algorithms_8pring PRIVATE
    -Os
    -g
)

target_link_libraries(hccl_stest_algorithms_8pring  PRIVATE
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

add_dependencies(hccl_stest_algorithms_8pring hccl_llt hccl_llt_open)
run_llt_test(
    TARGET hccl_stest_algorithms_8pring
    TASK_NUM 1
)