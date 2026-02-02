set(CTX_DIR ${SRC_DIR}/src/platform/hccp/rdma_service/ctx)

set(rs_ut_normal_list
    stub/cmd.c
    stub/init.c
    stub/device.c
    stub/mm.c
    stub/ssl.c
    stub/drv_usr_cfg.c
    stub/ccu_api.c
    stub/ub_api.c
    stub/net_api.c
    stub/net_co.c
    tc_ut_rs.c
    tc_ut_rs_ping.c
    tc_ut_rs_ping_urma.c
    tc_ut_rs_tlv.c
    tc_ut_rs_ctx.c
    tc_ut_rs_ub.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/crypto/encrypt.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/crypto/kmc_crypt.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_drv_rdma.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_ping.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_ping_roce.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_ping_inner.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_rdma.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_epoll.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_drv_socket.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/rs_socket.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/tlv/rs_tlv.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/tlv/rs_adp_nslb.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/dl_ibverbs_function.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/tlv/dl_netco_function.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/hccp_dl.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/roce_user/stub_hns_roce_u_cmd.c
    ${SRC_DIR}/src/platform/hccp/common/dl_hal_function.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx/dl_net_function.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/devdrv_runtime_api.c
    ${CTX_DIR}/dl_ccu_function.c
    ${CTX_DIR}/dl_urma_function.c
    ${CTX_DIR}/dl_net_function.c
    ${CTX_DIR}/rs_ccu.c
    ${CTX_DIR}/rs_ub.c
    ${CTX_DIR}/rs_ctx.c
    ${CTX_DIR}/rs_esched.c
    ${CTX_DIR}/rs_ping_urma.c
    ${CTX_DIR}/product/ascend910_96/rs_ub_jetty.c
    ${CTX_DIR}/product/ascend910_96/rs_ub_jfc.c
    main.cc
    ut_test.cc
    ${SRC_DIR}/test/ut/platform/hccp/stub/ut_dispatch.cc
)

add_executable(rs_ut_normal ${rs_ut_normal_list})

set_property(TARGET rs_ut_normal PROPERTY C_STANDARD 11)
target_include_directories(rs_ut_normal PRIVATE
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/include
    ${SRC_DIR}/src/platform/hccp/external_depends/inc
    ${SRC_DIR}/src/platform/hccp/external_depends/inc/driver
    ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/inc
    ${TOP_DIR}/abl/libc_sec/include
    ${SRC_DIR}/test/ut/depends/tls_adp
    ${SRC_DIR}/test/ut/depends/include
    ${SRC_DIR}/test/ut/platform/hccp/stub/include
    ${SRC_DIR}/test/ut/platform/hccp/ut_include
    ${SRC_DIR}/test/ut/platform/hccp/stub/
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub
    ${SRC_DIR}/test/ut/platform/hccp/stub/roce_user
    ${SRC_DIR}/src/platform/hccp/inc/private/network
    ${SRC_DIR}/src/platform/hccp/crypt/inc
    ${SRC_DIR}/src/platform/hccp/rdma_service/
    ${SRC_DIR}/src/platform/hccp/rdma_service/tlv
    ${SRC_DIR}/src/platform/hccp/common
    ${SRC_DIR}/src/platform/hccp/roce/roce_user
    ${SRC_DIR}/src/platform/hccp/tls_adp
    ${SRC_DIR}/test/ut/depends/include/tls_adp
    ${SRC_DIR}/src/platform/hccp/net_adapt/user/inc
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx/product
    ${SRC_DIR}/src/platform/hccp/inc/network
    ${SRC_DIR}/src/platform/hccp/inc/network
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx
    ${SRC_DIR}/src/platform/hccp/inc/private/network
    ${SRC_DIR}/src/platform/hccp/external_depends/ubengine
    ${SRC_DIR}/src/platform/hccp/external_depends/net_adapt
    ${SRC_DIR}/src/platform/hccp/external_depends/ccu
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx/product
    ${UT_COMMON_INCLUDE_LIST}
)

add_definitions(-DCONFIG_CONTEXT -DTGID_VA -DCONFIG_V121)
target_include_directories(rs_ut_normal PRIVATE
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/lib/urma/include
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/include
    ${TOP_DIR}/ubengine/ssapi/kernel/include/ub/urma/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/include/external/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/include/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/udma
    ${SRC_DIR}/src/platform/hccp/ccu/ccu_user
    ${SRC_DIR}/src/platform/hccp/ccu/ccu_user/product
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx
    ${TOP_DIR}/ace/csruntime/inc/tsch
)

target_compile_options(rs_ut_normal PRIVATE
    -U_FORTIFY_SOURCE -O0 -g
)

target_compile_definitions(rs_ut_normal PRIVATE
    HNS_ROCE_LLT USE_DISPATCH_EXPECT CA_CONFIG_LLT DEVICE_TLS_CONFIG CONFIG_SSL CONFIG_RDMA RS_DEVICE_CONFIG CUSTOM_INTERFACE RDMA_LITE_STUB_TEST CCU_CONFIG_LLT
)

target_link_libraries(rs_ut_normal PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    -lrt
    -lpthread
    -lgcov
    -ldl
)

run_llt_test(
    TARGET rs_ut_normal
    TASK_NUM 1
)
