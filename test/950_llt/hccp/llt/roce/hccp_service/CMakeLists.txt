

set(hccp_utest_list
    main.cc
    stub/tsd.c stub/mm.c stub/hccp_pub.c
    ${TOP_DIR}/llt/network/ut/stub/ut_dispatch.cc
    ${TOP_DIR}/drivers/network/hccp/hccp_service/main.c
    ${TOP_DIR}/drivers/network/hccp/hccp_service/param.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
    st_test.cc tc_hccp.c)


add_executable(hccp_utest ${hccp_utest_list})

set_property(TARGET hccp_utest PROPERTY C_STANDARD 99)
target_include_directories(hccp_utest PRIVATE
    stub
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/inc
    ${TOP_DIR}/llt/network/ut/stub/include
    ${TOP_DIR}/llt/network/ut/ut_include
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/inc/aicpu/tsd
    ${TOP_DIR}/inc/aicpu
    ${TOP_DIR}/drivers/network/hccp/hccp_service
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_service
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/llt/network/ut/roce/rdma_service_normal/include
    ${TOP_DIR}/drivers/network/roce/roce_user
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/host_lite
)

target_compile_options(hccp_utest PRIVATE
    -U_FORTIFY_SOURCE -O0
)

target_compile_definitions(hccp_utest PRIVATE
    HNS_ROCE_LLT USE_DISPATCH_EXPECT CONFIG_HCCP_LLT
    CONFIG_CGROUP
)

target_link_libraries(hccp_utest PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:network_headers>
    -lrt
)

run_llt_test(
    TARGET hccp_utest
    TASK_NUM 1
)
