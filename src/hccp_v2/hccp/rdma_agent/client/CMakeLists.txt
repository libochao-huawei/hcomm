set(CRYP_DIR ${TOP_DIR}/drivers/network/crypt)

set(ra_list
    ra_ctx.c
    async/ra_comm.c
    async/ra_ctx.c
    async/ra_socket.c
    ra_host.c
    ra_common.c
    ra_ping.c
    ra_rdma.c
    ra_init.c
    ra_socket.c
    ra_tlv.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
)

add_library(ra  SHARED ${ra_list})

target_include_directories(ra PRIVATE
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/inc/private/network/log/v2
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/peer
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/client
    ${TOP_DIR}/drivers/network/common/
    ${TOP_DIR}/drivers/network/roce/host_lite
)

target_compile_options(ra PRIVATE
    -Werror -fvisibility=hidden -fno-strict-aliasing -fsigned-char -fno-common -std=gnu11 -Wextra
)

target_compile_definitions(ra PRIVATE
)

target_link_libraries(ra PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:network_headers>
    -Wl,--no-as-needed
    c_sec
    slog
    ra_hdc
    ra_peer

    -Wl,--as-needed
    -ldl -lrt
    ofed_headers
)

target_compile_definitions(ra PRIVATE
    $<${USE_ALOG}:LOG_HOST>
)

install(TARGETS  ra
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

install_package(
    PACKAGE ra
    TARGETS ra
)