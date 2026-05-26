# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
include_guard(GLOBAL)

unset(rdma_core_FOUND CACHE)
unset(RDMA_CORE_INCLUDE CACHE)

if(NOT CANN_3RD_LIB_PATH)
    set(CANN_3RD_LIB_PATH ${CMAKE_SOURCE_DIR}/third_party)
endif()

set(RDMA_CORE_FILE "rdma-core-42.7.tar.gz")
set(RDMA_CORE_PATCH_FILE "rdma-core-42.7.patch")
set(RDMA_CORE_URL "https://cann-3rd.obs.cn-north-4.myhuaweicloud.com/rdma-core/${RDMA_CORE_FILE}")
set(RDMA_CORE_PATCH_URL "https://cann-3rd.obs.cn-north-4.myhuaweicloud.com/rdma-core/${RDMA_CORE_PATCH_FILE}")
set(RDMA_CORE_SOURCE_PATH "${CANN_3RD_LIB_PATH}/rdma-core")
set(RDMA_CORE_BUILD_PATH "${CANN_3RD_LIB_PATH}/rdma-core-build")

# 查找目录下是否已经安装，避免重复编译安装
find_path(RDMA_CORE_INCLUDE
    NAMES rdma/rdma_user_cm.h
    PATHS ${RDMA_CORE_BUILD_PATH}/include
    NO_DEFAULT_PATH
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(rdma_core
    FOUND_VAR rdma_core_FOUND
    REQUIRED_VARS
    RDMA_CORE_INCLUDE
)
message(STATUS "[ThirdPartyLib][rdma-core] Found rdma-core: ${rdma_core_FOUND}, PRODUCT_SIDE: ${PRODUCT_SIDE}")

if(rdma_core_FOUND AND NOT FORCE_REBUILD_CANN_3RD)
    message(STATUS "[ThirdPartyLib][rdma-core] rdma-core found in ${RDMA_CORE_BUILD_PATH}, and not force rebuild cann third_party")
elseif(PRODUCT_SIDE STREQUAL "host")
    # Host 侧编译时下载 hcomm_utils 包，Device 侧编译时进行复用
    if(EXISTS "${CANN_3RD_LIB_PATH}/${RDMA_CORE_FILE}")
        message(STATUS "[ThirdPartyLib][rdma-core] use local rdma-core cache.")
        set(RDMA_CORE_PROJECT_URL "${CANN_3RD_LIB_PATH}/${RDMA_CORE_FILE}")
    elseif(EXISTS "${CANN_3RD_LIB_PATH}/rdma-core/${RDMA_CORE_FILE}")
        message(STATUS "[ThirdPartyLib][rdma-core] pipeline use rdma-core cache.")
        set(RDMA_CORE_PROJECT_URL "${CANN_3RD_LIB_PATH}/rdma-core/${RDMA_CORE_FILE}")
    else()
        message(STATUS "[ThirdPartyLib][rdma-core] not use cache, download rdma-core source.")
        set(RDMA_CORE_PROJECT_URL "${RDMA_CORE_URL}")
    endif()

    # rdma-core 补丁
    if(EXISTS "${CANN_3RD_LIB_PATH}/${RDMA_CORE_PATCH_FILE}")
        message(STATUS "[ThirdPartyLib][rdma-core] use local rdma-core patch.")
        set(RDMA_CORE_PATCH_PROJECT_URL "${CANN_3RD_LIB_PATH}/${RDMA_CORE_PATCH_FILE}")
    elseif(EXISTS "${CANN_3RD_LIB_PATH}/rdma-core/${RDMA_CORE_PATCH_FILE}")
        message(STATUS "[ThirdPartyLib][rdma-core] pipeline use rdma-core patch.")
        set(RDMA_CORE_PATCH_PROJECT_URL "${CANN_3RD_LIB_PATH}/rdma-core/${RDMA_CORE_PATCH_FILE}")
    else()
        message(STATUS "[ThirdPartyLib][rdma-core] not use cache, download rdma-core patch.")
        set(RDMA_CORE_PATCH_PROJECT_URL "${RDMA_CORE_PATCH_URL}")
    endif()

    # 避免重复下载
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()

    # 下载 patch
    include(ExternalProject)
    ExternalProject_Add(third_party_rdma_core_patch
        URL ${RDMA_CORE_PATCH_PROJECT_URL}
        URL_HASH SHA256=54ca56b3b68bc465a78dd5839cd7110610745c7152a1dc3a72b265deeebb905f
        TLS_VERIFY OFF
        DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}/pkg
        DOWNLOAD_NAME ${RDMA_CORE_PATCH_FILE}
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_NO_PROGRESS TRUE
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        UPDATE_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
    )

    # 编译安装 rdma-core
    ExternalProject_Add(third_party_rdma_core
        URL ${RDMA_CORE_PROJECT_URL}
        URL_HASH SHA256=aa935de1fcd07c42f7237b0284b5697b1ace2a64f2bcfca3893185bc91b8c74d
        TLS_VERIFY OFF
        DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}/pkg
        DOWNLOAD_NO_PROGRESS TRUE
        SOURCE_DIR ${RDMA_CORE_SOURCE_PATH}
        BINARY_DIR ${RDMA_CORE_BUILD_PATH}
        PATCH_COMMAND patch -p1 -i "${CANN_3RD_LIB_PATH}/pkg/${RDMA_CORE_PATCH_FILE}"
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
            -DNO_MAN_PAGES=1
            -DENABLE_RESOLVE_NEIGH=0
            -DCMAKE_SKIP_RPATH=True
            -DNO_PYVERBS=1
            -S <SOURCE_DIR>
            -B <BINARY_DIR>
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target kern-abi
        INSTALL_COMMAND ""
        UPDATE_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
        DEPENDS third_party_rdma_core_patch
    )
endif()

set(RDMA_CORE_INCLUDE_DIR "${RDMA_CORE_BUILD_PATH}/include")
if(NOT EXISTS ${RDMA_CORE_INCLUDE_DIR})
    file(MAKE_DIRECTORY "${RDMA_CORE_INCLUDE_DIR}")
endif()

# 创建导入目标
add_library(rdma_core INTERFACE)
add_dependencies(rdma_core third_party_rdma_core)
target_include_directories(rdma_core INTERFACE
    ${RDMA_CORE_INCLUDE_DIR}
)
