

set(rdma_agent_stest_list
    main.cc
    stub/hdc.c stub/rs.c stub/rs_ping.c stub/sec.c stub/devmm_api.c stub/dl.c stub/devdrv_runtime_api.c
    stub/rs_tlv.c
    stub/rs_ctx.c
    ${TOP_DIR}/llt/network/st/stub/ut_dispatch.cc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_host.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_common.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_ping.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_ctx.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_rdma.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_init.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/ra_tlv.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/async/ra_comm.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/async/ra_ctx.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client/async/ra_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/ra_adp.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/tlv/ra_adp_tlv.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async/ra_adp_pool.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async/ra_adp_async.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/ctx/ra_adp_ctx.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc_ping.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc_ctx.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc_rdma.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc_tlv.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async/ra_hdc_async.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async/ra_hdc_async_ctx.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async/ra_hdc_async_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/peer/ra_peer.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/peer/ra_peer_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm/ra_comm.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/llt/network/st/stub/devdrv_runtime_api.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_hdc_lite.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ra_rdma_lite.c
    st_test.cc tc_rdma_agent.c tc_ra_ping.c tc_ra_ctx.c tc_ra_async.c tc_ra_tlv.c tc_ra_adp_tlv.c)


add_executable(rdma_agent_stest ${rdma_agent_stest_list})


target_include_directories(rdma_agent_stest PRIVATE
    stub
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/tlv
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/peer
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/llt/network/st/stub/include
    ${TOP_DIR}/llt/network/st/ut_include
    ${TOP_DIR}/llt/network/st/roce/rdma_service/include
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm
    ${TOP_DIR}/drivers/network/hccp/rdma_service
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/crypt/inc
    ${TOP_DIR}/abl/libkmc/include
    ${TOP_DIR}/abl/libkmc/src/sdp
    ${TOP_DIR}/abl/libkmc/src/common
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/air/third_party/inc/toolchain
    ${TOP_DIR}/abl/slog/inc/toolchain
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/host_lite
)

add_definitions(-DCONFIG_CONTEXT)
target_include_directories(rdma_agent_stest PRIVATE
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/ctx
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx
)

target_compile_options(rdma_agent_stest PRIVATE
    -U_FORTIFY_SOURCE -O0 -g
)

target_compile_definitions(rdma_agent_stest PRIVATE
    HNS_ROCE_LLT USE_DISPATCH_EXPECT CONFIG_SSL
)

target_link_libraries(rdma_agent_stest PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:network_headers>
    -lrt
)

run_llt_test(
    TARGET rdma_agent_stest
    TASK_NUM 1
)
