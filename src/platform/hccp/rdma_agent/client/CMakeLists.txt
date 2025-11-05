
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/dl_hal_function.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/ascend_hal_dl.c
)

add_library(ra  SHARED ${ra_list})

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(ra PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP)
    
    target_include_directories(ra PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../hdc
        ${CMAKE_CURRENT_SOURCE_DIR}/../hdc/async
        ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/network
        ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/private/network
        ${CMAKE_CURRENT_SOURCE_DIR}/../comm
        ${CMAKE_CURRENT_SOURCE_DIR}/../peer
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../roce/host_lite
#临时依赖位置
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/abl/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/rdma-core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/inc/log
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../externel_depends/driver/

        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog/toolchain/
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/ascend_hal/driver/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include

        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc

        # 缺runtime包临时引用放置在extenel_depends中的头文件
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../externel_depends/c_sec/include
    )

    target_link_directories(ra PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    if (DEVICE_MODE)
        target_link_directories(ra PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/${SYSTEM_ARCH}-linux/lib64/device/lib64/ # c_sec、mmpa、unified_dlog动态库搜索路径
        )
    endif()
else()
    target_include_directories(ra PRIVATE
        ${TOP_DIR}/hccl/src/platform/hccp/inc/network
        ${TOP_DIR}/hccl/src/platform/hccp/inc/private/network
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/inc
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/hdc
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/hdc/async
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/comm
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/peer
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/client
        ${TOP_DIR}/hccl/src/platform/hccp/common
        ${TOP_DIR}/hccl/src/platform/hccp/roce/host_lite
    )
endif()

target_compile_options(ra PRIVATE
    -Werror -fvisibility=hidden -fno-strict-aliasing -fsigned-char -fno-common -std=gnu11 -Wextra
)

target_compile_definitions(ra PRIVATE
)

if(BUILD_OPEN_PROJECT)
    target_link_libraries(ra PRIVATE
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        ra_hdc
        ra_peer
        -Wl,--as-needed
        -ldl -lrt
    )
else()
    target_link_libraries(ra PRIVATE
        $<BUILD_INTERFACE:intf_pub>
        $<BUILD_INTERFACE:slog_headers>
        -Wl,--no-as-needed
        c_sec
        slog
        ra_hdc
        ra_peer
        -Wl,--as-needed
        -ldl -lrt
        ofed_headers
    )
endif()

if((("${USE_ALOG}" STREQUAL "1")) OR ("${BUILD_OPEN_PROJECT}" STREQUAL "ON"))
target_compile_definitions(ra PRIVATE
    LOG_HOST
)
endif()

install(TARGETS  ra
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

# install_package(
#     PACKAGE ra
#     TARGETS ra
# )