# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set(ra_list
    async/ra_comm.c
    async/ra_socket.c
    async/ra_ctx.c
    ra_nda.c
    ra_ctx.c
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

    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/include/toolchain/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc

    # runtime头文件
    ${ASCEND_CANN_PACKAGE_PATH}/include/

    # rdma头文件
    ${RDMA_CORE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../external_depends/host_lite
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/

    # HCCP Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/async
)

if(NOT BUILD_OPEN_PROJECT)
    target_include_directories(ra PRIVATE
        ${TOP_DIR}/runtime/pkg_inc/base
        ${TOP_DIR}/driver/driver/pkg_inc/
    )
endif()

target_link_directories(ra PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)
if (DEVICE_MODE)
    target_link_directories(ra PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径
    )
endif()

target_compile_options(ra PRIVATE
    -Werror -fvisibility=hidden -fno-strict-aliasing -fsigned-char -fno-common -std=gnu11 -Wextra
    -fstack-protector-strong
    $<$<CONFIG:Debug>:-g>
)

target_link_options(ra PRIVATE
    -Wl,-z,now
    $<$<CONFIG:Release>:-s>
)

target_link_libraries(ra PRIVATE
    -Wl,--no-as-needed
    c_sec
    $<$<STREQUAL:${PRODUCT_SIDE},host>:unified_dlog>
    ra_hdc
    ra_peer
    -Wl,--as-needed
    -ldl -lrt
)

target_compile_definitions(ra PRIVATE
    LOG_HOST
    LOG_CPP
)

install(TARGETS  ra
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)
