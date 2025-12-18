
add_subdirectory(rdma_service)
add_subdirectory(rdma_agent)

set(BASE_DIR "${TOP_DIR}/drivers/network/hccp")

add_executable(hccp_service.bin  hccp_service/main.c hccp_service/param.c
${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    )

#头文件搜索路径
target_include_directories(hccp_service.bin PRIVATE
    ${TOP_DIR}/drivers/network/hccp
    ${TOP_DIR}/drivers/network/hccp/hccp_service
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/inc/aicpu/tsd
    ${BASE_DIR}/rdma_service
    ${BASE_DIR}/rdma_agent/adapter
    ${BASE_DIR}/rdma_agent/adapter/async
    ${BASE_DIR}/rdma_agent/hdc
    ${BASE_DIR}/rdma_agent/inc
    ${TOP_DIR}/drivers/network/roce/roce_user
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/host_lite
)

if ((PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST))
add_definitions(-DCONFIG_CONTEXT)

target_link_libraries(hccp_service.bin PRIVATE
    # avoid libascend_hal linking urma symbol not found issue
    urma
    urma_common
)

target_link_directories(hccp_service.bin PRIVATE
    # avoid libascend_hal linking urma symbol not found issue
    ${CMAKE_INSTALL_PREFIX}/lib
)

target_include_directories(hccp_service.bin PRIVATE
    ${TOP_DIR}/drivers/network/inc/private/network/log/v2
)
else()
target_include_directories(hccp_service.bin PRIVATE
    ${TOP_DIR}/drivers/network/inc/private/network/log/v1
)
endif()

target_compile_options(hccp_service.bin PRIVATE
    -Werror -Wall -fPIC -rdynamic -fvisibility=hidden -fno-common -fsigned-char -fno-strict-aliasing -std=gnu11
)

target_link_options(hccp_service.bin PRIVATE
    -Wl,-Bsymbolic -Wl,--no-undefined -rdynamic
)

target_compile_definitions(hccp_service.bin PRIVATE
    google=ascend_private
    $<$<AND:$<STREQUAL:${PRODUCT},ascend910>,$<STREQUAL:${PRODUCT_SIDE},device>>:CONFIG_CGROUP>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend310p>,$<STREQUAL:${PRODUCT_SIDE},device>>:CONFIG_CGROUP>
)

target_link_libraries(hccp_service.bin PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:network_headers>
    c_sec
    slog
    ra_adp
    rs
    tsd_eventclient
    ascend_protobuf
    -ldl
    ofed_headers
)

install(TARGETS  hccp_service.bin
    RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}  OPTIONAL
)