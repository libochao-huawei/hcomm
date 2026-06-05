# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

unset(hcomm_utils_FOUND CACHE)
unset(TLS_ADP_LIBRARY CACHE)

set(HCOMM_UTILS_VERSION "9.1.0")
set(HCOMM_UTILS_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
set(HCOMM_UTILS_FILE "cann-hcomm-utils_${HCOMM_UTILS_VERSION}_linux-${HCOMM_UTILS_ARCH}.tar.gz")
set(HCOMM_UTILS_URL "https://ascend-cann.obs.cn-north-4.myhuaweicloud.com/CANN/20260527_newest/${HCOMM_UTILS_FILE}")
set(HCOMM_UTILS_PKG_PATH ${CANN_3RD_LIB_PATH}/${HCOMM_UTILS_FILE})
set(HCOMM_UTILS_INSTALL_PATH ${CANN_3RD_LIB_PATH}/hcomm_utils)

# 查找目录下是否已经安装，避免重复编译安装
message(STATUS "[ThirdParty] HCOMM_UTILS_INSTALL_PATH=${HCOMM_UTILS_INSTALL_PATH} PRODUCT_SIDE=${PRODUCT_SIDE}")
find_library(TLS_ADP_LIBRARY
    NAMES libtls_adp.so
    PATH_SUFFIXES lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    NO_CMAKE_PATH
    PATHS ${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}
)

# 是否找到 hcomm_utils 的库文件
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(hcomm_utils
    FOUND_VAR
    hcomm_utils_FOUND 
    REQUIRED_VARS
    TLS_ADP_LIBRARY
)
message(STATUS "[ThirdParty] Found hcomm_utils: ${hcomm_utils_FOUND}")

if(hcomm_utils_FOUND AND NOT FORCE_REBUILD_CANN_3RD)
    message(STATUS "[ThirdParty] hcomm_utils found in ${HCOMM_UTILS_INSTALL_PATH}, and not force rebuild cann third_party")
elseif(PRODUCT_SIDE STREQUAL "host")
    # Host 侧编译时下载 hcomm_utils 包，Device 侧编译时进行复用
    file(GLOB HCOMM_UTILS_GLOB_PKG
        ${CANN_3RD_LIB_PATH}/cann-hcomm-utils_*_linux-${HCOMM_UTILS_ARCH}.tar.gz
    )
    if(EXISTS ${HCOMM_UTILS_GLOB_PKG})
        # 离线编译场景，优先使用已下载的包（忽略版本号）
        message(STATUS "[ThirdParty] Found local hcomm_utils package (ignore version): ${HCOMM_UTILS_GLOB_PKG}")
        set(HCOMM_UTILS_PKG_PATH ${HCOMM_UTILS_GLOB_PKG})
        set(HCOMM_UTILS_PROJECT_URL ${HCOMM_UTILS_PKG_PATH})
    elseif(EXISTS ${HCOMM_UTILS_PKG_PATH})
        # 离线编译场景，优先使用已下载的包
        message(STATUS "[ThirdParty] Found local hcomm_utils package: ${HCOMM_UTILS_PKG_PATH}")
        set(HCOMM_UTILS_PROJECT_URL ${HCOMM_UTILS_PKG_PATH})
    else()
        # 下载并解压
        message(STATUS "[ThirdParty] Downloading hcomm_utils from ${HCOMM_UTILS_URL}")
        set(HCOMM_UTILS_PROJECT_URL ${HCOMM_UTILS_URL})
    endif()

    if(EXISTS ${HCOMM_UTILS_GLOB_PKG})
        set(HCOMM_UTILS_URL_HASH "")    # 忽略版本号，不校验哈希值
    elseif(HCOMM_UTILS_ARCH MATCHES "aarch64|ARM64|arm64")
        set(HCOMM_UTILS_URL_HASH "SHA256=bbd7b3c3c78c9ad12ba5109ce7b0d4cf37c73c9a3844f3abd601cd4b0db9eeb5")
    else()
        set(HCOMM_UTILS_URL_HASH "SHA256=0bcfc92635af8066224374686050a72d83e52b6c882be3c62ba415123f3f3afd")
    endif()

    include(ExternalProject)
    ExternalProject_Add(hcomm_utils
        URL ${HCOMM_UTILS_PROJECT_URL}
        URL_HASH ${HCOMM_UTILS_URL_HASH}
        TLS_VERIFY OFF
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_NO_PROGRESS TRUE
        DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}
        SOURCE_DIR ${HCOMM_UTILS_INSTALL_PATH}
        CONFIGURE_COMMAND tar -xf ${HCOMM_UTILS_PKG_PATH} --overwrite --strip-components=2 -C <SOURCE_DIR>
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )
endif()

if(NOT EXISTS "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include")
    file(MAKE_DIRECTORY "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/legacy")
    file(MAKE_DIRECTORY "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/tls_adp")
    file(MAKE_DIRECTORY "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/kms")
endif()

# 创建 headers 目标
add_library(hcomm_legacy_headers INTERFACE IMPORTED)
add_dependencies(hcomm_legacy_headers hcomm_utils)
set_target_properties(hcomm_legacy_headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/legacy"
)

add_library(ascend_kms_headers INTERFACE IMPORTED)
add_dependencies(ascend_kms_headers hcomm_utils)
set_target_properties(ascend_kms_headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/kms"
)

# 创建链接库目标
add_library(ascend_kms SHARED IMPORTED)
add_dependencies(ascend_kms hcomm_utils)
set_target_properties(ascend_kms PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/kms"
    IMPORTED_LOCATION "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/lib/libascend_kms.so"
)

add_library(tls_adp SHARED IMPORTED)
add_dependencies(tls_adp hcomm_utils)
set_target_properties(tls_adp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/tls_adp"
    IMPORTED_LOCATION "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/lib/libtls_adp.so"
)

if(PRODUCT_SIDE STREQUAL "host")
    add_library(hccl_legacy SHARED IMPORTED)
    add_dependencies(hccl_legacy hcomm_utils)
    set_target_properties(hccl_legacy PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/include/legacy"
        IMPORTED_LOCATION "${HCOMM_UTILS_INSTALL_PATH}/${PRODUCT_SIDE}/lib/libhccl_legacy.so"
    )
endif()

# 安装 host 侧需要的库
if(PRODUCT_SIDE STREQUAL "host")
    set(HCOMM_UTILS_HOST_LIB_FILES
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/libtls_adp.so
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/libhccl_legacy.so
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/MemSet_dynamic_AtomicAddrClean_1_ascend310p3.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/MemSet_dynamic_AtomicAddrClean_1_ascend910.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/MemSet_dynamic_AtomicAddrClean_1_ascend910b.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_float16_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_float16_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_float32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_int32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_int32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_int64_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_int64_v81.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_int8_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_add_int8_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_float16_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_float16_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_float32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_float32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_int32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_int32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_int64_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_int64_v81.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_int8_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_maximum_int8_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_float16_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_float16_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_float32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_float32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_int32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_int32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_int64_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_int64_v81.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_int8_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_minimum_int8_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float16_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float16_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float16_v81_910B1.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float16_v81_910B2.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float16_v81_910B3.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float16_v81_910B4.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float32_v81_910B1.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float32_v81_910B2.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float32_v81_910B3.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_float32_v81_910B4.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int32_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int32_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int32_v81_910B1.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int32_v81_910B2.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int32_v81_910B3.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int32_v81_910B4.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int64_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int64_v81.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int8_v51.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int8_v80.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int8_v81_910B1.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int8_v81_910B2.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int8_v81_910B3.o
        ${HCOMM_UTILS_INSTALL_PATH}/host/lib/dynamic_mul_int8_v81_910B4.o
    )

    install(FILES ${HCOMM_UTILS_HOST_LIB_FILES}
        DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
endif()
