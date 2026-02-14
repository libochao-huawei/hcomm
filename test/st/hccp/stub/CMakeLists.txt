# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_library(hccp_stub SHARED
    hccp_stub.c
    sec_stub.c
    ssl_stub.c
    dlog_stub.c
)

target_include_directories(hccp_stub PRIVATE
    ${HCCP_DIR}/inc/network
    ${HCCP_DIR}/inc/private/network
    ${HCCP_DIR}/common/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/include/toolchain/
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${CANN_3RD_LIB_PATH}/hcomm_utils/device/include/tls_adp
    ${CANN_3RD_LIB_PATH}/hcomm_utils/device/include/kms
    ${OPENSSL_INCLUDE_DIR}
)

install(
    TARGETS hccp_stub
    LIBRARY DESTINATION ${HCCP_TEST_DIR}/common
    COMPONENT hccp_test
)

function(create_hal_library PRODUCT_SIDE)
    set(TARGET_NAME "ascend_hal_${PRODUCT_SIDE}_stub")
    add_library(${TARGET_NAME} SHARED
        ascend_hal_${PRODUCT_SIDE}_stub.c
    )

    target_compile_options(${TARGET_NAME} PRIVATE
        -fno-strict-aliasing -fsigned-char -fno-common -std=gnu11 -Wextra
        -g -O0
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "ascend_hal"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PRODUCT_SIDE}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    )

    install(
        TARGETS ${TARGET_NAME}
        LIBRARY DESTINATION ${HCCP_TEST_DIR}/${PRODUCT_SIDE}
        COMPONENT hccp_test
    )
endfunction()

create_hal_library(host)
create_hal_library(device)

add_library(urma SHARED
    urma.c
)

set_target_properties(urma PROPERTIES
    OUTPUT_NAME          "urma"
    SOVERSION            "0"
    VERSION              "0.0.0"
    PREFIX               "lib"
)

target_include_directories(urma PRIVATE
    ${URMA_INCLUDE_DIR}
)

install(
    TARGETS urma
    LIBRARY DESTINATION ${HCCP_TEST_DIR}/device
    COMPONENT hccp_test
)
