set(CTX_DIR ${SRC_DIR}/src/platform/hccp/rdma_service/ctx)
 
set(rs_ut_stub_list
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/init.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/device.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/mm.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/ssl.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/drv_usr_cfg.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/ccu_api.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/ub_api.c
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/stub/net_api.c
    stub/rs_ub_stub.c
    tc_ut_rs_ub.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/crypto/encrypt.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/crypto/kmc_crypt.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx/dl_net_function.c
    ${SRC_DIR}/src/platform/hccp/rdma_service/hccp_dl.c
    # ${SRC_DIR}/src/platform/hccp/common/dl_hal_function.c
    ${CTX_DIR}/dl_ccu_function.c
    ${CTX_DIR}/dl_urma_function.c
    ${CTX_DIR}/dl_net_function.c
    ${CTX_DIR}/rs_ccu.c
    main.cc
    ut_test.cc
    ${SRC_DIR}/test/ut/platform/hccp/stub/ut_dispatch.cc)
 
 
add_executable(rs_ut_stub ${rs_ut_stub_list})
set_property(TARGET rs_ut_stub PROPERTY C_STANDARD 99)
 
target_include_directories(rs_ut_stub PRIVATE
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service_normal/include
    ${SRC_DIR}/src/platform/hccp/external_depends/inc
    ${SRC_DIR}/src/platform/hccp/external_depends/inc/driver
    ${TOP_DIR}/drivers/ai_sdk/arc/linux/kernel_space/inc
    ${TOP_DIR}/abl/libc_sec/include
    ${SRC_DIR}/test/ut/platform/hccp/stub/include
    ${SRC_DIR}/test/ut/platform/hccp/ut_include
    ${SRC_DIR}/test/ut/platform/hccp/stub/
    ${SRC_DIR}/test/ut/platform/hccp/rdma_service/stub
    ${SRC_DIR}/test/ut/depends/tls_adp
    ${SRC_DIR}/src/platform/hccp/inc/private/network
    ${SRC_DIR}/src/platform/hccp/inc/network
    ${SRC_DIR}/src/platform/hccp/crypt/inc
    ${SRC_DIR}/src/platform/hccp/rdma_service/
    ${SRC_DIR}/src/platform/hccp/rdma_service/tlv
    ${SRC_DIR}/test/ut/platform/hccp/stub/roce_user
    ${SRC_DIR}/src/platform/hccp/common
    ${SRC_DIR}/src/platform/hccp/roce/roce_user
    ${SRC_DIR}/src/platform/hccp/tls_adp
    ${SRC_DIR}/test/ut/depends/include/tls_adp
    ${SRC_DIR}/src/platform/hccp/net_adapt/user/inc
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx/product
    ${SRC_DIR}/src/platform/hccp/inc/network
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx
    ${SRC_DIR}/src/platform/hccp/inc/private/network
    ${SRC_DIR}/src/platform/hccp/external_depends/ubengine
    ${SRC_DIR}/src/platform/hccp/external_depends/net_adapt
    ${SRC_DIR}/src/platform/hccp/external_depends/ccu
    ${UT_COMMON_INCLUDE_LIST}
)
 
add_definitions(-DCONFIG_CONTEXT -DTGID_VA -DCONFIG_V121)
target_include_directories(rs_ut_stub PRIVATE
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/lib/urma/include
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/include
    ${TOP_DIR}/ubengine/ssapi/kernelspace/include/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/include/external/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/include/udma
    ${TOP_DIR}/ubengine/ssapi/userspace/udma
    ${SRC_DIR}/src/platform/hccp/ccu/ccu_user
    ${SRC_DIR}/src/platform/hccp/ccu/ccu_user/product
    ${SRC_DIR}/src/platform/hccp/ccu/ccu_user/product/ascend910_95
    ${SRC_DIR}/src/platform/hccp/rdma_service/ctx
    ${TOP_DIR}/ace/csruntime/inc/tsch
)
 
target_compile_options(rs_ut_stub PRIVATE
    -U_FORTIFY_SOURCE -O0 -g
)
 
target_compile_definitions(rs_ut_stub PRIVATE
    CCU_CONFIG_LLT HNS_ROCE_LLT USE_DISPATCH_EXPECT CA_CONFIG_LLT DEVICE_TLS_CONFIG CONFIG_SSL CONFIG_RDMA RS_DEVICE_CONFIG CFG_BUILD_DEBUG CUSTOM_INTERFACE RDMA_LITE_STUB_TEST
)
 
target_link_libraries(rs_ut_stub PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    -lrt
    -lpthread
    -lgcov
    -ldl
)
 
run_llt_test(
    TARGET rs_ut_stub
    TASK_NUM 1
)