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
set(RDMA_CORE_SRC_DIR ${ROOT_BUILD_PATH}/rdma-core-42.7)
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
    file(DOWNLOAD ${RDMA_CORE_URL} ${ROOT_BUILD_PATH}/rdma-core-42.7.tar.gz
        EXPECTED_HASH SHA256=aa935de1fcd07c42f7237b0284b5697b1ace2a64f2bcfca3893185bc91b8c74d
    )
    execute_process(
        COMMAND tar -zxf rdma-core-42.7.tar.gz
        WORKING_DIRECTORY ${ROOT_BUILD_PATH}
    )
    execute_process(
        COMMAND patch -p1  -i ${ROOT_BUILD_PATH}/rdma-core-42.7.patch
        WORKING_DIRECTORY ${RDMA_CORE_SRC_DIR}
    )
    message(STATUS "downloading ${RDMA_CORE_URL} to ${RDMA_CORE_ROOT_DIR}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -S "${RDMA_CORE_SRC_DIR}"
        -B "${RDMA_CORE_BUILD_DIR}"
        -DNO_MAN_PAGES=1
        -DENABLE_RESOLVE_NEIGH=0
        -DCMAKE_SKIP_RPATH=True
        -DNO_PYVERBS=1
)

execute_process(
    COMMAND ${CMAKE_COMMAND}
        --build "${RDMA_CORE_BUILD_DIR}"
        --target kern-abi
)

set(RDMA_CORE_INCLUDE_DIR ${RDMA_CORE_BUILD_DIR}/include)