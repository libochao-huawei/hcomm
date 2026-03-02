# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

message("Build third party library rdma-core")
set(RDMA_CORE_NAME "rdma-core")
set(ROOT_BUILD_PATH "${CMAKE_BINARY_DIR}")
set(RDMA_CORE_SEARCH_PATHS "${CANN_3RD_LIB_PATH}/${RDMA_CORE_NAME}")
set(RDMA_CORE_ROOT_DIR ${ROOT_BUILD_PATH}/rdma-core)
set(RDMA_CORE_SRC_DIR ${ROOT_BUILD_PATH}/rdma-core/src)
set(RDMA_CORE_BUILD_DIR ${ROOT_BUILD_PATH}/rdma-core/build)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if(EXISTS ${RDMA_CORE_SEARCH_PATHS})
    set(RDMA_CORE_SRC_DIR ${RDMA_CORE_SEARCH_PATHS})
    message(STATUS "Successfully copied ${RDMA_CORE_SEARCH_PATHS} to ${ROOT_BUILD_PATH}.")
else()
    set(RDMA_CORE_URL "https://gitcode.com/cann-src-third-party/rdma-core/releases/download/v42.7-h1/rdma-core-42.7.tar.gz")
    set(RDMA_CORE_PATCH_URL "https://gitcode.com/cann-src-third-party/rdma-core/releases/download/v42.7-h1/rdma-core-42.7.patch")
    file(DOWNLOAD ${RDMA_CORE_PATCH_URL} ${ROOT_BUILD_PATH}/rdma-core-42.7.patch)
    set(FETCH_COMMAND
        URL ${RDMA_CORE_URL}
        DOWNLOAD_DIR ${RDMA_CORE_ROOT_DIR}
        URL_HASH SHA256=aa935de1fcd07c42f7237b0284b5697b1ace2a64f2bcfca3893185bc91b8c74d
        PATCH_COMMAND patch -p1 < ${ROOT_BUILD_PATH}/rdma-core-42.7.patch
    )
    message(STATUS "downloading ${RDMA_CORE_URL} to ${RDMA_CORE_ROOT_DIR}")
endif()

ExternalProject_Add(${RDMA_CORE_NAME}
    ${FETCH_COMMAND}
    CONFIGURE_COMMAND ${CMAKE_COMMAND}
                    -DNO_MAN_PAGES=1
                    -DENABLE_RESOLVE_NEIGH=0
                    -DCMAKE_SKIP_RPATH=True
                    -DNO_PYVERBS=1
                    <SOURCE_DIR>
    BUILD_COMMAND $(MAKE) kern-abi
    INSTALL_COMMAND ""
    SOURCE_DIR "${RDMA_CORE_SRC_DIR}"
    BINARY_DIR "${RDMA_CORE_BUILD_DIR}"
)

execute_process(
  COMMAND ${CMAKE_COMMAND} --build . --target ${RDMA_CORE_NAME}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set(RDMA_CORE_INCLUDE_DIR ${RDMA_CORE_BUILD_DIR}/include)