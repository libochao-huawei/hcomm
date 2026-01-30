set(TLV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tlv)

set(rs_list
    rs.c
    rs_epoll.c
    rs_ping.c
    rs_ping_inner.c
    rs_ping_roce.c
    rs_socket.c
    rs_rdma.c
    rs_drv_rdma.c
    rs_drv_socket.c
    dl_ibverbs_function.c
    hccp_dl.c
    ctx/dl_net_function.c
    ctx/dl_ccu_function.c
    ctx/dl_urma_function.c
    ctx/rs_ping_urma.c
    ctx/rs_ccu.c
    ctx/rs_ctx.c
    ctx/rs_esched.c
    ctx/rs_ub.c
    ctx/product/ascend910_95/rs_ub_jetty.c
    ctx/product/ascend910_95/rs_ub_jfc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/dl_hal_function.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/ascend_hal_dl.c
    $<$<NOT:$<STREQUAL:${PRODUCT},ascend310p>>:${CMAKE_CURRENT_SOURCE_DIR}/../common/file_opt.c>
    $<$<NOT:$<STREQUAL:${PRODUCT},ascend310p>>:${CMAKE_CURRENT_SOURCE_DIR}/../common/network_comm.c>
)

if((("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend")) AND
   ("${PRODUCT_SIDE}" STREQUAL "device") AND
   (NOT ${USE_ALOG}))
list(APPEND rs_list
    ${TLV_DIR}/dl_netco_function.c
    ${TLV_DIR}/rs_tlv.c
    ${TLV_DIR}/rs_adp_nslb.c
)
endif()

add_library(rs  SHARED ${rs_list})

target_compile_definitions(rs PRIVATE
    OPEN_BUILD_PROJECT
    LOG_CPP
    TGID_VA
    CONFIG_V121
    CONFIG_CONTEXT
)

target_include_directories(rs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc/private/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/

    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/include/toolchain/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
    # runtime头文件
    ${ASCEND_CANN_PACKAGE_PATH}/include/

    # 第三方库依赖路径
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/hcomm_utils/${PRODUCT_SIDE}/include/tls_adp
    ${CMAKE_BINARY_DIR}/hcomm_utils/${PRODUCT_SIDE}/include/kms

    # rdma头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/rdma-core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/host_lite
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/rdma-core/providers/hns
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/inc/driver

    # HCCP Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/ctx
    ${CMAKE_CURRENT_SOURCE_DIR}/ctx/product
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/ccu
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/net_adapt
    ${CMAKE_CURRENT_SOURCE_DIR}/../external_depends/ubengine
)

# c_sec、mmpa、unified_dlog动态库搜索路径
target_link_directories(rs PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/
)

if((("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend")) AND
   ("${PRODUCT_SIDE}" STREQUAL "device") AND (NOT ${USE_ALOG}))
    target_include_directories(rs PRIVATE
        ${TLV_DIR}
    )

    target_compile_definitions(rs PRIVATE
        CUSTOM_INTERFACE google=ascend_private
        CONFIG_TLV
    )
endif()

target_compile_options(rs PRIVATE
    -Werror -fvisibility=hidden -fno-strict-aliasing -fsigned-char -fno-common -Wfloat-equal
    -Wextra -Wno-unused-parameter -std=gnu11
    -fstack-protector-strong
    $<$<CONFIG:Debug>:-g>
)

target_link_options(rs PRIVATE
    -Wl,-Bsymbolic -Wl,--exclude-libs,ALL
    -Wl,-z,now
    $<$<CONFIG:Release>:-s>
)

target_compile_definitions(rs PRIVATE
    $<$<AND:$<STREQUAL:${PRODUCT},ascend310p>,$<STREQUAL:${PRODUCT_SIDE},device>>:NOT_INIT_IBVERBS>
)

if(("${USE_ALOG}" STREQUAL "1"))
target_compile_definitions(rs PRIVATE
    LOG_HOST
)
endif()

target_link_libraries(rs PRIVATE
    -Wl,--no-as-needed
    c_sec
    $<$<STREQUAL:${PRODUCT_SIDE},host>:unified_dlog>
    $<$<NOT:$<STREQUAL:${PRODUCT},ascend310p>>:tls_adp>
    -Wl,--as-needed
    -Wl,--whole-archive
    -Wl,--no-whole-archive
    -lrt
    -ldl
)

install(TARGETS  rs
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

add_dependencies(rs crypto_static)
