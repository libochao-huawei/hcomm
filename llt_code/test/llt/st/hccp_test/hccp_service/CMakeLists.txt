

set(hccp_stest_list
    main.cc
    stub/tsd.c stub/mm.c stub/hccp_pub.c
    ${TOP_DIR}/llt/network/st/stub/ut_dispatch.cc
    ${TOP_DIR}/drivers/network/hccp/hccp_service/main.c
    ${TOP_DIR}/drivers/network/hccp/hccp_service/param.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
    # ${TOP_DIR}/llt/network/st/stub/devdrv_runtime_api.c

    st_test.cc tc_hccp.c)


add_executable(hccp_stest ${hccp_stest_list})


target_include_directories(hccp_stest PRIVATE
    stub
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/llt/network/st/stub/include
    ${TOP_DIR}/llt/network/st/ut_include
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
    ${TOP_DIR}/llt/network/st/roce/rdma_service/include
    ${TOP_DIR}/drivers/network/roce/roce_user
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/host_lite
)

target_compile_options(hccp_stest PRIVATE
    -U_FORTIFY_SOURCE -O0
)

target_compile_definitions(hccp_stest PRIVATE
    HNS_ROCE_LLT USE_DISPATCH_EXPECT CONFIG_HCCP_LLT
    CONFIG_CGROUP
)

target_link_libraries(hccp_stest PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:network_headers>
    -lrt
)

run_llt_test(
    TARGET hccp_stest
    TASK_NUM 1
)
