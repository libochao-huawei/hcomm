set(BASE_DIR ${TOP_DIR}/third_party)
set(CRYP_DIR ${TOP_DIR}/drivers/network/crypt)
set(TLV_DIR ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv)
set(CTX_DIR ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx)

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
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/file_opt.c
    ${TOP_DIR}/drivers/network/common/network_comm.c
)

if((("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend") OR
    (PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST)) AND
    ("${PRODUCT_SIDE}" STREQUAL "device") AND
    (NOT ${USE_ALOG}))
list(APPEND rs_list
    ${TLV_DIR}/dl_netco_function.c
    ${CTX_DIR}/dl_ccu_function.c
    ${TLV_DIR}/rs_tlv.c
    ${TLV_DIR}/rs_adp_nslb.c
)
endif()

if ((PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST) OR
    ("hccl" IN_LIST PACKAGE AND "${PRODUCT_SIDE}" STREQUAL "host") OR # CANN-hccl Host
    ("${PRODUCT}" STREQUAL "ascend" AND "${PRODUCT_SIDE}" STREQUAL "device" AND ${USE_ALOG})) # CANN runtime Device
add_definitions(-DTGID_VA -DCONFIG_V121)
add_definitions(-DCONFIG_CONTEXT)
list(APPEND rs_list
    ${CTX_DIR}/dl_ccu_function.c
    ${CTX_DIR}/dl_urma_function.c
    ${CTX_DIR}/rs_ccu.c
    ${CTX_DIR}/rs_ub.c
    ${CTX_DIR}/rs_ctx.c
    ${CTX_DIR}/rs_esched.c
    ${CTX_DIR}/rs_ping_urma.c
)
endif()

if ((PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST))
add_definitions(-DCUSTOM_INTERFACE)
endif()

add_library(rs  SHARED ${rs_list})

#头文件搜索路径
target_include_directories(rs PRIVATE
    ${TOP_DIR}/drivers/network/roce/roce_user
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/crypt/inc
    ${TOP_DIR}/drivers/network/tls_adp
    ${TOP_DIR}/abl/libkmc/include
    ${TOP_DIR}/abl/libkmc/src/sdp
    ${TOP_DIR}/abl/libkmc/src/common
    ${TOP_DIR}/drivers/network/hccp/rdma_service
    ${OPENSSL_INCLUDE_DIR}
)

if((("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend") OR
    (PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST)) AND
    ("${PRODUCT_SIDE}" STREQUAL "device") AND
    (NOT ${USE_ALOG}))
target_include_directories(rs PRIVATE
    ${TLV_DIR}
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx/
    ${TOP_DIR}/drivers/network/ccu/ccu_user
)

target_compile_definitions(rs PRIVATE
    CUSTOM_INTERFACE google=ascend_private
    CONFIG_TLV
)
endif()

if ((PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST) OR
    ("hccl" IN_LIST PACKAGE AND "${PRODUCT_SIDE}" STREQUAL "host") OR # CANN-hccl Host
    ("${PRODUCT}" STREQUAL "ascend" AND "${PRODUCT_SIDE}" STREQUAL "device" AND ${USE_ALOG})) # CANN runtime Device
target_include_directories(rs PRIVATE
    ${TOP_DIR}/drivers/network/inc/private/network/log/v2
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/lib/urma/include
    ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/include
    ${TOP_DIR}/ubengine/ssapi/userspace/include/external/udma
    ${TOP_DIR}/drivers/network/ccu/ccu_user
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx
    ${TOP_DIR}/ace/csruntime/inc/tsch
)
else()
target_include_directories(rs PRIVATE
    ${TOP_DIR}/drivers/network/inc/private/network/log/v1
)
endif()

target_compile_options(rs PRIVATE
    -Werror -fvisibility=hidden -fno-strict-aliasing -fsigned-char -fno-common -Wfloat-equal
    -Wextra -Wno-unused-parameter -std=gnu11
)

target_link_options(rs PRIVATE
    -Wl,-Bsymbolic -Wl,--exclude-libs,ALL
    # -rdynamic -Wl,--no-undefined
)

if(NOT (PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) AND
   NOT (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) AND
   NOT (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST) AND
   NOT (PRODUCT STREQUAL "ascend310p"))
target_compile_definitions(rs PRIVATE
    CONFIG_SSL
)
endif()

target_compile_definitions(rs PRIVATE
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910>,$<STREQUAL:${PRODUCT_SIDE},device>>:CUSTOM_INTERFACE google=ascend_private>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910B>,$<STREQUAL:${PRODUCT_SIDE},device>>:CUSTOM_INTERFACE google=ascend_private>
    $<${USE_ALOG}:LOG_HOST>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend310p>,$<STREQUAL:${PRODUCT_SIDE},device>>:NOT_INIT_IBVERBS>
)

target_link_libraries(rs PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:mmpa_headers>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:network_headers>
    -Wl,--no-as-needed
    c_sec
    slog
    mmpa
    -Wl,--as-needed
    $<$<NOT:$<STREQUAL:${PRODUCT},ascend310p>>:tls_adp>
    -Wl,--whole-archive
    $<$<STREQUAL:${PRODUCT_SIDE},device>:ascend_protobuf_static>
    -Wl,--no-whole-archive
    -lrt
    -ldl
    ofed_headers
)

install(TARGETS  rs
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

install_package(
    PACKAGE rs
    TARGETS rs
)