set(hccl_utest_device_src_list
    main.cc
    ut_aiv_device_utest.cc
    ut_stream_device_utest.cc
    ut_communicator_device_utest.cc
    ut_zero_copy_device_utest.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/common/stream/stream_utils_aicpu.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/algorithm/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_aicpu.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/framework/communicator/hccl_comm_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/framework/communicator/impl/hccl_communicator_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/framework/communicator/impl/hccl_communicator_attrs_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/framework/communicator/impl/zero_copy/zero_copy_address_mgr_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/framework/common/src/config/env_config_aicpu.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/framework/op_base/src/op_base_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/common/debug/profiling/task_exception_handler_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/common/debug/profiling/plugin_runner_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/algorithm/impl/hccl_aiv_device.cc
    ${TOP_DIR}/ace/comop/hccl/open_source/src/domain/collective_communication/algorithm/impl/hccl_alg_device.cc
)

add_executable(hccl_utest_device  ${hccl_utest_device_src_list})
set_legacy_protobuf()
#头文件搜索路径
target_include_directories(hccl_utest_device  PRIVATE
    ${include_list}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

target_compile_definitions(hccl_utest_device    PRIVATE
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DCCL_LLT
)

target_compile_options(hccl_utest_device PRIVATE
    -O0
    -g
)

target_link_libraries(hccl_utest_device  PRIVATE
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
    mmpa
    -lrt
    -ldl
)

add_dependencies(hccl_utest_device hccl_llt hccl_llt_open)

run_llt_test(
    TARGET hccl_utest_device
    TASK_NUM 1
)
