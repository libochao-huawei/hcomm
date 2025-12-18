set(TLV_DIR ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/tlv)
set(ra_adp_list
    ra_adp.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async/ra_adp_pool.c
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async/ra_adp_async.c
)

if((("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend") OR
    (PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST)) AND
   ("${PRODUCT_SIDE}" STREQUAL "device") AND
   (NOT ${USE_ALOG}))
list(APPEND ra_adp_list
    ${TLV_DIR}/ra_adp_tlv.c
)
endif()

if ((PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST))
add_definitions(-DCONFIG_CONTEXT)

list(APPEND ra_adp_list
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/ctx/ra_adp_ctx.c
)
endif()

#编译目标
add_library(ra_adp SHARED
    ${ra_adp_list}
    ${TOP_DIR}/drivers/network/common/dl_hal_function.c
    ${TOP_DIR}/drivers/network/common/ascend_hal_dl.c
)

#添加头文件搜索路径
target_include_directories(ra_adp PRIVATE
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/drivers/network/inc/private/network
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/inc
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/async/
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/comm
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/async/
    ${TOP_DIR}/drivers/network/hccp/rdma_service
    ${TOP_DIR}/drivers/network/common
    ${TOP_DIR}/drivers/network/roce/host_lite
)

if((("${PRODUCT}" STREQUAL "ascend910B") OR ("${PRODUCT}" STREQUAL "ascend") OR
    (PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST)) AND
   ("${PRODUCT_SIDE}" STREQUAL "device") AND
   (NOT ${USE_ALOG}))
target_include_directories(ra_adp PRIVATE
    ${TLV_DIR}
    ${TOP_DIR}/drivers/network/hccp/rdma_service/tlv
)

target_compile_definitions(ra_adp PRIVATE
    CONFIG_TLV
)
endif()

if ((PRODUCT IN_LIST ASCEND910_95_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_96_PRODUCT_LIST) OR
    (PRODUCT IN_LIST ASCEND910_55_PRODUCT_LIST))
target_include_directories(ra_adp PRIVATE
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/hdc/ctx/
    ${TOP_DIR}/drivers/network/hccp/rdma_agent/adapter/ctx/
    ${TOP_DIR}/drivers/network/hccp/rdma_service/ctx/
    ${TOP_DIR}/drivers/network/inc/private/network/log/v2
)
else()
target_include_directories(ra_adp PRIVATE
    ${TOP_DIR}/drivers/network/inc/private/network/log/v1
)
endif()

#编译选项
target_compile_options(ra_adp PRIVATE
    -Werror -fvisibility=hidden -fsigned-char -fno-common -fno-strict-aliasing -std=gnu11
)

target_link_options(ra_adp PRIVATE
    -Wl,-Bsymbolic
    -rdynamic
    -Wl,--no-undefined
)

#链接动态库
target_link_libraries(ra_adp PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:network_headers>
    -Wl,--no-as-needed
    rs
    c_sec
    slog
    -Wl,--as-needed
    -lrt
    -ldl
    ofed_headers
)

install(TARGETS  ra_adp
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)
