if(BUILD_OPEN_PROJECT)
    set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../comm)
    set(ASYNC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/async)
else()
    set(BASE_DIR "${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/comm")
    set(ASYNC_DIR "${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/hdc/async")
endif()

set(ra_hdc_list ra_hdc.c
    ${BASE_DIR}/ra_comm.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/dl_hal_function.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/ascend_hal_dl.c
)

add_library(ra_hdc SHARED ${ra_hdc_list})

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(ra_hdc PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP)
    
    target_include_directories(ra_hdc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/network
        ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/private
        ${CMAKE_CURRENT_SOURCE_DIR}/../../rdma_service
        ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/private/network
        ${BASE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${ASYNC_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../roce/host_lite
        ${CMAKE_CURRENT_SOURCE_DIR}/../../roce/roce_user
#临时依赖位置
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/inc/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/abl/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/abl/toolchain
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

    target_link_directories(ra_hdc PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    if (DEVICE_MODE)
        target_link_directories(ra_hdc PRIVATE
            ${ASCEND_CANN_PACKAGE_PATH}/${SYSTEM_ARCH}-linux/lib64/device/lib64/ # c_sec、mmpa、unified_dlog动态库搜索路径
        )
    endif()
else()
    target_include_directories(ra_hdc PRIVATE
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/hccl/src/platform/hccp/inc/network
        ${TOP_DIR}/hccl/src/platform/hccp/inc/private/network
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/inc
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_service
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/comm
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/hdc
        ${TOP_DIR}/hccl/src/platform/hccp/rdma_agent/hdc/async
        ${TOP_DIR}/hccl/src/platform/hccp/common/
        ${TOP_DIR}/hccl/src/platform/hccp/roce/host_lite
        ${TOP_DIR}/hccl/src/platform/hccp/roce/roce_user
    )
endif()

target_compile_options(ra_hdc PRIVATE
    -Werror -fno-common -fsigned-char -fno-strict-aliasing -std=gnu11 -Wextra
)

target_compile_definitions(ra_hdc PRIVATE

)

if(BUILD_OPEN_PROJECT)
    target_link_libraries(ra_hdc PRIVATE
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        -Wl,--as-needed
        -lrt
        -ldl
    )
else()
    target_link_libraries(ra_hdc PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    -Wl,--no-as-needed
    c_sec
    slog
    -Wl,--as-needed
    -lrt
    -ldl
    ofed_headers
    )
endif()

if((("${USE_ALOG}" STREQUAL "1")) OR ("${BUILD_OPEN_PROJECT}" STREQUAL "ON"))
target_compile_definitions(ra_hdc PRIVATE
    LOG_HOST
)
endif()

install(TARGETS  ra_hdc
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)

# install_package(
#     PACKAGE ra_hdc
#     TARGETS ra_hdc
# )