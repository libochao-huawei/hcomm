# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if(BUILD_OPEN_PROJECT)
    add_subdirectory(stub_kms)
    if (DEVICE_MODE)
        add_subdirectory(stub_tsd)
    endif()
endif()
add_subdirectory(rdma_agent)
add_subdirectory(rdma_service)
add_subdirectory(tls_adp)

if (NOT DEVICE_MODE)
    return()
endif()

add_executable(hccp_service.bin  hccp_service/main.c hccp_service/param.c
    ${CMAKE_CURRENT_SOURCE_DIR}/common/ascend_hal_dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/common/dl_hal_function.c
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hccp_service.bin PRIVATE OPEN_BUILD_PROJECT LOG_CPP)

    target_include_directories(hccp_service.bin PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/hccp_service
        ${CMAKE_CURRENT_SOURCE_DIR}/external_depends/inc/driver
        ${CMAKE_CURRENT_SOURCE_DIR}/external_depends/inc/log
        ${CMAKE_CURRENT_SOURCE_DIR}/external_depends/rdma-core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/network
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/private/network
        ${CMAKE_CURRENT_SOURCE_DIR}/rdma_service
        ${CMAKE_CURRENT_SOURCE_DIR}/rdma_agent/adapter
        ${CMAKE_CURRENT_SOURCE_DIR}/rdma_agent/adapter/async
        ${CMAKE_CURRENT_SOURCE_DIR}/rdma_agent/hdc
        ${CMAKE_CURRENT_SOURCE_DIR}/rdma_agent/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/roce/roce_user
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/roce/host_lite

        # 缺runtime包临时引用放置在extenel_depends中的头文件
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../externel_depends/c_sec/include
        ${CMAKE_CURRENT_SOURCE_DIR}/stub_tsd/
    )

    target_link_directories(hccp_service.bin PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/${SYSTEM_ARCH}-linux/lib64/device/lib64/ # c_sec、mmpa、unified_dlog动态库搜索路径
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

if(BUILD_OPEN_PROJECT)
    target_link_libraries(hccp_service.bin PRIVATE
        # $<BUILD_INTERFACE:intf_pub>
        # $<BUILD_INTERFACE:slog_headers>
        # $<BUILD_INTERFACE:network_headers>
        c_sec
        unified_dlog
        ra_adp
        rs
        mmpa
        tsd_eventclient
        # ascend_protobuf
        # crypto_static
        -ldl
        # ofed_headers
    )
endif()

install(TARGETS  hccp_service.bin
    RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}  OPTIONAL
)

add_dependencies(hccp_service.bin rs)
if(DEVICE_MODE)
    set(HCCP_TAR_DIR ${HCCL_BASE_DIR}/../build_device/tar_pkg/)
    add_custom_command(
        TARGET hccp_service.bin
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HCCP_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rs> ${HCCP_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:stub_kms> ${HCCP_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ra_adp> ${HCCP_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:hccp_service.bin> ${HCCP_TAR_DIR}
        COMMENT "Copying librs.so to ${HCCP_TAR_DIR}"
    )
endif()