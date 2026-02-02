set(CTX_DIR ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx)

set(rs_st_v2_list
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/init.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/device.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/dummy_ops.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/mm.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/ssl.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/drv_usr_cfg.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/ccu_api.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/ub_api.c
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub/net_api.c
    stub/rs_ub_stub.c
    tc_ut_rs_ub.c
    ${TOP_DIR}/llt/network/st/stub/crypto/encrypt.c
    ${TOP_DIR}/llt/network/st/stub/crypto/kmc_crypt.c
    ${TOP_DIR}/drivers/network/tls_adp/rs_tls.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx/dl_net_function.c
    ${TOP_DIR}/drivers/network/hccp/rdma_service/hccp_dl.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/llt/network/st/stub/devdrv_runtime_api.c
    ${TOP_DIR}/drivers/network/common/file_opt.c
    ${CTX_DIR}/dl_ccu_function.c
    ${CTX_DIR}/dl_urma_function.c
    ${CTX_DIR}/dl_net_function.c
    ${CTX_DIR}/rs_ccu.c
    ${CTX_DIR}/product/ascend910_95/rs_ub_jfc.c
    ${CTX_DIR}/product/ascend910_95/rs_ub_jetty.c
    main.cc
    st_test.cc
    ${TOP_DIR}/llt/network/st/stub/ut_dispatch.cc)


add_executable(rs_st_v2 ${rs_st_v2_list})
set_property(TARGET rs_st_v2 PROPERTY C_STANDARD 99)

target_include_directories(rs_st_v2 PRIVATE
    ${TOP_DIR}/llt/network/st/roce/rdma_service/include
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/inc
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/llt/network/st/stub/include
    ${TOP_DIR}/llt/network/st/ut_include
    ${TOP_DIR}/llt/network/st/roce/stub/
    ${TOP_DIR}/llt/network/st/roce/rdma_service/stub
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/inc/network
    ${TOP_DIR}/drivers/network/crypt/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_service/
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv
    ${TOP_DIR}/llt/network/ut/stub/roce_user
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/roce_user
    ${TOP_DIR}/drivers/network/tls_adp
    ${TOP_DIR}/drivers/network/net_adapt/user/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx/product
)

add_definitions(-DCONFIG_CONTEXT -DTGID_VA -DCONFIG_V121)
target_include_directories(rs_st_v2 PRIVATE
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

target_compile_options(rs_st_v2 PRIVATE
    -U_FORTIFY_SOURCE -O0 -g
)

target_compile_definitions(rs_st_v2 PRIVATE
    CCU_CONFIG_LLT HNS_ROCE_LLT USE_DISPATCH_EXPECT CA_CONFIG_LLT DEVICE_TLS_CONFIG CONFIG_SSL CONFIG_RDMA RS_DEVICE_CONFIG CFG_BUILD_DEBUG CUSTOM_INTERFACE RDMA_LITE_STUB_TEST
)

target_link_libraries(rs_st_v2 PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    $<BUILD_INTERFACE:network_headers>
    -lrt
)

run_llt_test(
    TARGET rs_st_v2
    TASK_NUM 1
)
