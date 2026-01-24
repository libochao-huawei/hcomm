
set(BASE_DIR "${TOP_DIR}/drivers/network/hccp/rdma_agent/comm")
set(ASYNC_DIR "${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async")

set(ra_hdc_list ra_hdc.c
    ${BASE_DIR}/ra_comm.c
    ${BASE_DIR}/ctx/ra_ctx_comm.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
    ra_hdc_ctx.c
    ra_rdma_lite.c
    ra_hdc_lite.c
    ra_hdc_ping.c
    ra_hdc_socket.c
    ra_hdc_rdma.c
    ra_hdc_tlv.c
    ${ASYNC_DIR}/ra_hdc_async.c
    ${ASYNC_DIR}/ra_hdc_async_ctx.c
    ${ASYNC_DIR}/ra_hdc_async_socket.c
)

add_library(ra_hdc SHARED ${ra_hdc_list})

target_include_directories(ra_hdc PRIVATE
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_service
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/inc/private/network/log/v2
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm/ctx
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async
    ${TOP_DIR}/drivers/network/common/
    ${TOP_DIR}/drivers/network/roce/host_lite
    ${TOP_DIR}/drivers/network/roce/roce_user
)

target_compile_options(ra_hdc PRIVATE
    -Werror -fno-common -fsigned-char -fno-strict-aliasing -std=gnu11 -Wextra
)

target_compile_definitions(ra_hdc PRIVATE

)

target_link_libraries(ra_hdc PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:network_headers>
    -Wl,--no-as-needed
    c_sec
    slog
    -Wl,--as-needed
    -lrt
    -ldl
    ofed_headers
)

target_compile_definitions(ra_hdc PRIVATE
    $<${USE_ALOG}:LOG_HOST>
)

install(TARGETS  ra_hdc
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

install_package(
    PACKAGE ra_hdc
    TARGETS ra_hdc
)