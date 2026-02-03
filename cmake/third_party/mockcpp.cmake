# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

unset(mockcpp_FOUND CACHE)
unset(MOCKCPP_INCLUDE CACHE)
unset(MOCKCPP_STATIC_LIBRARY CACHE)

# 查找目录下是否已经安装，避免重复编译安装
set(MOCKCPP_INSTALL_PATH ${CANN_3RD_LIB_PATH}/mockcpp)
message(STATUS "[ThirdParty] MOCKCPP_INSTALL_PATH=${MOCKCPP_INSTALL_PATH}")
find_path(MOCKCPP_INCLUDE
        NAMES mockcpp/mockcpp.hpp
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${MOCKCPP_INSTALL_PATH}/include)
find_library(MOCKCPP_STATIC_LIBRARY
        NAMES libmockcpp.a
        PATH_SUFFIXES lib lib64
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${MOCKCPP_INSTALL_PATH})

# 是否全部找到 mockcpp 的头文件和二进制
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(mockcpp
        FOUND_VAR
        mockcpp_FOUND
        REQUIRED_VARS
        MOCKCPP_INCLUDE
        MOCKCPP_STATIC_LIBRARY
)
message(STATUS "[ThirdParty] Found MockCpp: ${mockcpp_FOUND}")

# 编译选项设置
if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(mockcpp_CFLAGS   "-fPIC -D_GLIBCXX_USE_CXX11_ABI=0")
    set(mockcpp_CXXFLAGS "-fPIC -D_GLIBCXX_USE_CXX11_ABI=0")
else()
    set(mockcpp_CFLAGS   "-fPIC -D_GLIBCXX_USE_CXX11_ABI=0")
    set(mockcpp_CXXFLAGS "-fPIC -D_GLIBCXX_USE_CXX11_ABI=0 -std=c++11")
endif()

if(mockcpp_FOUND AND NOT FORCE_REBUILD_CANN_3RD)
    message(STATUS "[ThirdParty] MockCpp found in ${MOCKCPP_INSTALL_PATH}, and not force rebuild cann third_party")
else()
    set(REQ_URL "https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h2/mockcpp-2.7.zip")
    set(PATCH_URL "https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h2/mockcpp-2.7_py3.patch")

    # 设置依赖的 boost 路径
    set(BOOST_INCLUDE_DIRS ${OPEN_SOURCE_DIR}/boost-1.87.0)

    # 设置下载和源码目录
    set(MOCKCPP_DOWNLOAD_DIR ${CANN_3RD_PKG_PATH})
    set(MOCKCPP_SOURCE_DIR ${CANN_3RD_LIB_PATH}/../llt/third_party/mockcpp/mockcpp-2.7)

    # 检查是否有本地源码
    if(EXISTS ${MOCKCPP_SOURCE_DIR})
        message(STATUS "[ThirdParty] Found local mockcpp source: ${MOCKCPP_SOURCE_DIR}")
        set(MOCKCPP_USE_LOCAL_SRC TRUE)
    else()
        message(STATUS "[ThirdParty] Will download mockcpp from ${REQ_URL}")
        set(MOCKCPP_USE_LOCAL_SRC FALSE)
    endif()

    # 设置 CMake 选项
    set(MOCKCPP_OPTS
        -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}
        -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
        -DCMAKE_CXX_FLAGS=${mockcpp_CXXFLAGS}
        -DCMAKE_C_FLAGS=${mockcpp_CFLAGS}
        -DBOOST_INCLUDE_DIRS=${BOOST_INCLUDE_DIRS}
        -DCMAKE_SHARED_LINKER_FLAGS=""
        -DCMAKE_EXE_LINKER_FLAGS=""
        -DBUILD_32_BIT_TARGET_BY_64_BIT_COMPILER=OFF
        -DCMAKE_INSTALL_PREFIX=${MOCKCPP_INSTALL_PATH}
    )
    
    # 下载补丁文件
    set(PATCH_FILE ${MOCKCPP_DOWNLOAD_DIR}/mockcpp-2.7_py3.patch)
    if(NOT EXISTS ${PATCH_FILE})
        message(STATUS "[ThirdParty] Downloading mockcpp patch from ${PATCH_URL}")
        file(DOWNLOAD
            ${PATCH_URL}
            ${PATCH_FILE}
            TLS_VERIFY OFF
            TIMEOUT 60
        )
    endif()
    
    include(ExternalProject)
    if(MOCKCPP_USE_LOCAL_SRC)
        ExternalProject_Add(third_party_mockcpp
            SOURCE_DIR ${MOCKCPP_SOURCE_DIR}
            PATCH_COMMAND git init && git apply ${PATCH_FILE}
            CONFIGURE_COMMAND ${CMAKE_COMMAND} ${MOCKCPP_OPTS} <SOURCE_DIR>
            BUILD_COMMAND ${MAKE_CMD}
            INSTALL_COMMAND ${MAKE_CMD} install
            EXCLUDE_FROM_ALL TRUE
        )
    else()
        ExternalProject_Add(third_party_mockcpp
            URL ${REQ_URL}
            TLS_VERIFY OFF
            DOWNLOAD_DIR ${MOCKCPP_DOWNLOAD_DIR}
            PATCH_COMMAND git init && git apply ${PATCH_FILE}
            CONFIGURE_COMMAND ${CMAKE_COMMAND} ${MOCKCPP_OPTS} <SOURCE_DIR>
            BUILD_COMMAND ${MAKE_CMD}
            INSTALL_COMMAND ${MAKE_CMD} install
            EXCLUDE_FROM_ALL TRUE
        )
    endif()
endif()

# 创建导入的目标
add_library(mockcpp STATIC IMPORTED)
add_dependencies(mockcpp third_party_mockcpp)

set_target_properties(mockcpp PROPERTIES
    IMPORTED_LOCATION ${MOCKCPP_INSTALL_PATH}/lib/libmockcpp.a
    INTERFACE_INCLUDE_DIRECTORIES ${MOCKCPP_INSTALL_PATH}/include
)
