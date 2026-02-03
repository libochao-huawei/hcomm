# ----------------------------------------------------------------------------
# This program is free software, you can redistribute it and/or modify it.
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. See LICENSE in the root of
# the software repository for the full text of the License.
# ----------------------------------------------------------------------------

unset(protobuf_FOUND CACHE)
unset(Protobuf_INCLUDE_DIR CACHE)
unset(Protobuf_LIBRARY CACHE)
unset(Protobuf_PROTOC_EXECUTABLE CACHE)

set(PROTOBUF_NAME "protobuf")
set(PROTOBUF_FILE "protobuf-25.1.zip")
set(PROTOBUF_URL "https://gitcode.com/cann-src-third-party/protobuf/releases/download/v25.1/${PROTOBUF_FILE}")
set(PROTOBUF_PKG_PATH ${CANN_3RD_PKG_PATH}/${PROTOBUF_FILE})
set(PROTOBUF_INSTALL_PATH ${CANN_3RD_LIB_PATH}/protobuf)

# 查找目录下是否已经安装，避免重复编译安装
message(STATUS "[ThirdParty] PROTOBUF_INSTALL_PATH=${PROTOBUF_INSTALL_PATH}")
find_path(Protobuf_INCLUDE_DIR
    NAMES google/protobuf/message.h
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    PATHS ${PROTOBUF_INSTALL_PATH}/include
)

find_library(Protobuf_LIBRARY
    NAMES protobuf libprotobuf
    PATH_SUFFIXES lib lib64
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    PATHS ${PROTOBUF_INSTALL_PATH}
)

find_program(Protobuf_PROTOC_EXECUTABLE
    NAMES protoc
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    PATHS ${PROTOBUF_INSTALL_PATH}/bin
)

# 是否全部找到 protobuf 的头文件、链接库和和编译器
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(protobuf
    FOUND_VAR
    protobuf_FOUND
    REQUIRED_VARS
    Protobuf_INCLUDE_DIR
    Protobuf_LIBRARY
    Protobuf_PROTOC_EXECUTABLE
)
message(STATUS "[ThirdParty] Found Protobuf: ${protobuf_FOUND}")

if(protobuf_FOUND AND NOT FORCE_REBUILD_CANN_3RD)
    message(STATUS "[ThirdParty] Protobuf found in ${PROTOBUF_INSTALL_PATH}, and not force rebuild cann third_party")
elseif()
    # 1. 下载 abseil-cpp 包
    set(ABSEIL_FILE "abseil-cpp-20250127.0.tar.gz")
    set(ABSEIL_URL "https://gitcode.com/cann-src-third-party/abseil-cpp/releases/download/20250127.0/${ABSEIL_FILE}")
    set(ABSEIL_PKG_PATH ${CANN_3RD_PKG_PATH}/${ABSEIL_FILE})

    if(EXISTS ${ABSEIL_PKG_PATH})
        # 离线编译场景，优先使用 pkg 目录下的包
        message(STATUS "[ThirdParty] Found local abseil-cpp package: ${ABSEIL_PKG_PATH}")
        set(ABSEIL_PROJECT_URL ${ABSEIL_PKG_PATH})
    else()
        # 下载 abseil-cpp 包
        message(STATUS "[ThirdParty] Downloading abseil-cpp from ${ABSEIL_URL}")
        set(ABSEIL_PROJECT_URL ${ABSEIL_URL})
    endif()

    include(ExternalProject)
    ExternalProject_Add(third_party_abseil_cpp
        URL ${ABSEIL_PROJECT_URL}
        DOWNLOAD_DIR ${CANN_3RD_PKG_PATH}
        DOWNLOAD_NO_PROGRESS TRUE
        DOWNLOAD_NO_EXTRACT TRUE    # 仅下载，不解压
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
    )

    # 2. 编译安装 protobuf
    if(EXISTS ${PROTOBUF_PKG_PATH})
        # 离线编译场景，优先使用 pkg 目录下的包
        message(STATUS "[ThirdParty] Found local protobuf package: ${PROTOBUF_PKG_PATH}")
        set(PROTOBUF_PROJECT_URL ${PROTOBUF_PKG_PATH})
    else()
        # 下载 protobuf 包
        message(STATUS "[ThirdParty] Downloading protobuf from ${PROTOBUF_URL}")
        set(PROTOBUF_PROJECT_URL ${PROTOBUF_URL})
    endif()

    set(PROTOBUF_OPTS
        -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${PROTOBUF_INSTALL_PATH}
        -DCMAKE_INSTALL_LIBDIR=lib64
        -DBUILD_SHARED_LIBS=ON
        -Dprotobuf_WITH_ZLIB=OFF
        -Dprotobuf_BUILD_TESTS=OFF
    )

    include(ExternalProject)
    ExternalProject_Add(third_party_protobuf
        URL ${PROTOBUF_PROJECT_URL}
        DOWNLOAD_DIR ${CANN_3RD_PKG_PATH}
        DOWNLOAD_COMMAND
            COMMAND tar -zxf ${PROTOBUF_PKG_PATH} --strip-components 1 -C ${SOURCE_DIR}
            COMMAND tar -zxf ${ABSEIL_PKG_PATH} --strip-components 1 -C ${SOURCE_DIR}/third_party/abseil-cpp
        DOWNLOAD_NO_PROGRESS TRUE
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} ${PROTOBUF_OPTS} <SOURCE_DIR>
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) install
        DEPENDS third_party_abseil_cpp
    )
endif()

# 创建导入的库目标
add_library(protobuf SHARED IMPORTED)
add_dependencies(protobuf third_party_protobuf)

set_target_properties(protobuf PROPERTIES
    IMPORTED_LOCATION ${PROTOBUF_INSTALL_PATH}/lib64/libprotobuf.so
    INTERFACE_INCLUDE_DIRECTORIES ${PROTOBUF_INSTALL_PATH}/include
)
