# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../comm)
set(ra_peer_list
    ra_peer.c
    ra_peer_nda.c
    ra_peer_socket.c
    ra_peer_ctx.c
    ${BASE_DIR}/ra_comm.c
    ${BASE_DIR}/ctx/ra_ctx_comm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/dl_hal_function.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/ascend_hal_dl.c
)

#编译目标
add_library(ra_peer SHARED ${ra_peer_list})

target_include_directories(ra_peer PRIVATE
    ${BASE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../rdma_service
    ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/private/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/

    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/include/toolchain/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc

    # rdma头文件
    ${RDMA_CORE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/host_lite
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/

    # HCCP Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../rdma_agent/comm/ctx
    ${CMAKE_CURRENT_SOURCE_DIR}/../../rdma_service/ctx
)

if(NOT BUILD_OPEN_PROJECT)
    target_include_directories(ra_peer PRIVATE
        ${TOP_DIR}/runtime/pkg_inc/base
        ${TOP_DIR}/driver/driver/pkg_inc/
    )
endif()

target_link_directories(ra_peer PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)
if (DEVICE_MODE)
    target_link_directories(ra_peer PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径
    )
endif()


target_compile_options(ra_peer PRIVATE
    -Werror -std=gnu11 -fno-common -Wfloat-equal -Wextra
    -fstack-protector-strong
    $<$<CONFIG:Debug>:-g>
)

target_link_options(ra_peer PRIVATE
    -Wl,-z,now
    $<$<CONFIG:Release>:-s>
)

target_link_libraries(ra_peer PRIVATE
    -Wl,--no-as-needed
    rs
    c_sec
    $<$<STREQUAL:${PRODUCT_SIDE},host>:unified_dlog>
    -Wl,--as-needed
    -ldl
)

target_compile_definitions(ra_peer PRIVATE
    LOG_HOST
    LOG_CPP
)

install(TARGETS  ra_peer
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)
