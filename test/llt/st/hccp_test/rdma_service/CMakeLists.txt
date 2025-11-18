set(CTX_DIR ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx)

set(rs_st_list
    stub/cmd.c
    stub/init.c
    stub/device.c
    stub/dummy_ops.c
    stub/mm.c
    stub/ssl.c
    stub/drv_usr_cfg.c
    stub/ccu_api.c
    stub/net_co.c
    stub/ub_api.c
    tc_rs.c
    tc_ut_rs_ping.c
    tc_ut_rs_ping_urma.c
    tc_ut_rs_ctx.c
    tc_ut_rs_ub.c
    tc_rs_tlv.c
    ${TOP_DIR}/llt/network/st/stub/crypto/encrypt.c
    ${TOP_DIR}/llt/network/st/stub/crypto/kmc_crypt.c
    ${TOP_DIR}/drivers/network/tls_adp/rs_tls.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_drv_rdma.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_rdma.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_ping.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_ping_roce.c
    ${CTX_DIR}/rs_ping_urma.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_ping_inner.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_epoll.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_drv_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/rs_socket.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv/rs_tlv.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv/rs_adp_nslb.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/dl_ibverbs_function.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv/dl_netco_function.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/hccp_dl.c
    ${TOP_DIR}/drivers/network/tls_adp/ssl_adp.c
    ${TOP_DIR}/llt/network/st/stub/roce_user/stub_hns_roce_u_cmd.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/llt/network/st/stub/devdrv_runtime_api.c
    ${TOP_DIR}/drivers/network/common/file_opt.c
    ${CTX_DIR}/dl_ccu_function.c
    ${CTX_DIR}/dl_urma_function.c
    ${CTX_DIR}/rs_ccu.c
    ${CTX_DIR}/rs_ub.c
    ${CTX_DIR}/rs_ctx.c
    ${CTX_DIR}/rs_esched.c
    main.cc
    st_test.cc
    ${TOP_DIR}/llt/network/st/stub/ut_dispatch.cc)


add_executable(rs_st ${rs_st_list})
set_property(TARGET rs_st PROPERTY C_STANDARD 99)

target_include_directories(rs_st PRIVATE
    ${TOP_DIR}/llt/network/st/roce/rdma_service/include
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/llt/network/st/stub/include
    ${TOP_DIR}/llt/network/st/ut_include
    ${TOP_DIR}/llt/network/st/roce/stub/
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/crypt/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_service/
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv
    ${TOP_DIR}/llt/network/ut/stub/roce_user
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/roce_user
    ${TOP_DIR}/drivers/network/tls_adp
)

add_definitions(-DCONFIG_CONTEXT -DTGID_VA)
target_include_directories(rs_st PRIVATE
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/lib/urma/include
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/include
    ${TOP_DIR}/ubengine/ssapi/kernelspace/include/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/include/external/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/include/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/udma
    ${TOP_DIR}/drivers/network/ccu/ccu_user
    ${TOP_DIR}/drivers/network/ccu/ccu_user/product
    ${TOP_DIR}/drivers/network/ccu/ccu_user/product/ascend910_95
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx
    ${TOP_DIR}/ace/csruntime/inc/tsch
)

target_compile_options(rs_st PRIVATE
    -U_FORTIFY_SOURCE -O0 -g
)

target_compile_definitions(rs_st PRIVATE
    CCU_CONFIG_LLT HNS_ROCE_LLT USE_DISPATCH_EXPECT CA_CONFIG_LLT DEVICE_TLS_CONFIG CONFIG_SSL CONFIG_RDMA RS_DEVICE_CONFIG CFG_BUILD_DEBUG CUSTOM_INTERFACE RDMA_LITE_STUB_TEST
)

target_link_libraries(rs_st PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:network_headers>
    -lrt
)

run_llt_test(
    TARGET rs_st
    TASK_NUM 1
)
