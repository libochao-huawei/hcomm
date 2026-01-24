
set(BASE_DIR "${TOP_DIR}/drivers/network/hccp/rdma_agent/comm")

set(ra_peer_list
    ra_peer.c
    ra_peer_socket.c
    ra_peer_ctx.c
    ${BASE_DIR}/ra_comm.c
    ${BASE_DIR}/ctx/ra_ctx_comm.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
)


#编译目标
add_library(ra_peer SHARED ${ra_peer_list})

#添加头文件搜索路径
target_include_directories(ra_peer PRIVATE
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/peer
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_service
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/inc/private/network/log/v2
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm/ctx
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/host_lite
)

target_compile_options(ra_peer PRIVATE
    -Werror -std=gnu11 -fno-common -Wfloat-equal -Wextra
)

#链接动态库
target_link_libraries(ra_peer PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:network_headers>
    -Wl,--no-as-needed
    rs
    c_sec
    slog
    -Wl,--as-needed
    -ldl
    ofed_headers
)

target_compile_definitions(ra_peer PRIVATE
    $<${USE_ALOG}:LOG_HOST>
)

install(TARGETS  ra_peer
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

install_package(
    PACKAGE ra_peer
    TARGETS ra_peer
)