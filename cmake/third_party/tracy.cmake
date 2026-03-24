# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

include_guard(GLOBAL)

if(NOT DEFINED CANN_3RD_LIB_PATH OR CANN_3RD_LIB_PATH STREQUAL "")
    set(CANN_3RD_LIB_PATH ${PROJECT_SOURCE_DIR}/third_party)
endif()

# Tracy Profiler 集成 (BSD 3-Clause License)
# 仅编译 Tracy Client 静态库，供 host 侧按需链接

unset(tracy_FOUND CACHE)
unset(TRACY_INCLUDE CACHE)
unset(TRACY_CLIENT_STATIC_LIBRARY CACHE)

# 线上构建禁止从个人仓下载代码，Tracy 仅支持离线本地源码构建。
# 保留原下载配置供历史追溯，明确注释掉避免误用。
# set(TRACY_GIT_URL "https://gitcode.com/clggstudy/tracy.git")
# set(TRACY_GIT_TAG "master")
set(TRACY_SRC_PATH ${PROJECT_SOURCE_DIR}/tools/tracy_src)

if(CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "")
    set(TRACY_TARGET_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
else()
    set(TRACY_TARGET_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif()

if(PRODUCT_SIDE AND NOT PRODUCT_SIDE STREQUAL "")
    set(TRACY_TARGET_TAG "${PRODUCT_SIDE}-${TRACY_TARGET_PROCESSOR}")
else()
    set(TRACY_TARGET_TAG ${TRACY_TARGET_PROCESSOR})
endif()

set(TRACY_INSTALL_ROOT ${CANN_3RD_LIB_PATH}/tracy_install)
set(TRACY_INSTALL_PATH ${TRACY_INSTALL_ROOT}/${TRACY_TARGET_TAG})
message(STATUS "[ThirdParty] Tracy target tag: ${TRACY_TARGET_TAG}")

# 查找是否已经编译安装
find_path(TRACY_INCLUDE
    NAMES tracy/Tracy.hpp
    PATH_SUFFIXES include
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    PATHS ${TRACY_INSTALL_PATH}
)
find_library(TRACY_CLIENT_STATIC_LIBRARY
    NAMES libtracy-client.a
    PATH_SUFFIXES lib64
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    PATHS ${TRACY_INSTALL_PATH}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tracy
    FOUND_VAR
    tracy_FOUND
    REQUIRED_VARS
    TRACY_INCLUDE
    TRACY_CLIENT_STATIC_LIBRARY
)
message(STATUS "[ThirdParty] Found Tracy client: ${tracy_FOUND}")

if(tracy_FOUND AND NOT FORCE_REBUILD_CANN_3RD)
    if(NOT TARGET third_party_tracy_client)
        add_custom_target(third_party_tracy_client)
    endif()
    message(STATUS "[ThirdParty] Tracy client found in ${TRACY_INSTALL_PATH}, and not force rebuild cann third_party")
else()
    if(NOT EXISTS ${TRACY_SRC_PATH}/public/TracyClient.cpp)
        message(FATAL_ERROR "[ThirdParty] Offline Tracy source not found: ${TRACY_SRC_PATH}. Please prepare tools/tracy_src/public in advance.")
    endif()

    message(STATUS "[ThirdParty] Building Tracy from local source: ${TRACY_SRC_PATH}")

    # Tracy Client 编译选项
    set(TRACY_CXXFLAGS
        -std=c++14
        -D_GLIBCXX_USE_CXX11_ABI=0
        -O2
        -D_FORTIFY_SOURCE=2
        -fPIC
        -fstack-protector-all
    )

    include(ExternalProject)
    ExternalProject_Add(third_party_tracy_client
        # 离线模式：禁止网络下载，仅使用 SOURCE_DIR 中的本地源码。
        # GIT_REPOSITORY ${TRACY_GIT_URL}
        # GIT_TAG ${TRACY_GIT_TAG}
        # GIT_SHALLOW TRUE
        # TLS_VERIFY OFF
        # DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}
        # DOWNLOAD_NO_PROGRESS TRUE
        DOWNLOAD_COMMAND ""
        SOURCE_DIR ${TRACY_SRC_PATH}
        # 只编译 TracyClient.cpp 成静态库，不编译 Tracy GUI 或工具链
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            ${CMAKE_COMMAND} -E make_directory <BINARY_DIR> &&
            ${CMAKE_CXX_COMPILER} ${TRACY_CXXFLAGS} -DTRACY_ENABLE -I<SOURCE_DIR>/public -c <SOURCE_DIR>/public/TracyClient.cpp -o <BINARY_DIR>/TracyClient.o &&
            ${CMAKE_AR} rcs <BINARY_DIR>/libtracy-client.a <BINARY_DIR>/TracyClient.o
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${TRACY_INSTALL_PATH}/lib64 &&
            ${CMAKE_COMMAND} -E make_directory ${TRACY_INSTALL_PATH}/include &&
            ${CMAKE_COMMAND} -E copy_if_different
            <BINARY_DIR>/libtracy-client.a
            ${TRACY_INSTALL_PATH}/lib64/libtracy-client.a &&
            ${CMAKE_COMMAND} -E copy_directory
            <SOURCE_DIR>/public/tracy
            ${TRACY_INSTALL_PATH}/include/tracy
        UPDATE_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
    )
endif()

if(NOT EXISTS ${TRACY_INSTALL_PATH}/include)
    file(MAKE_DIRECTORY "${TRACY_INSTALL_PATH}/include")
endif()

add_library(tracy_client STATIC IMPORTED)
if(NOT BUILD_OPEN_PROJECT OR (BUILD_OPEN_PROJECT AND KERNEL_MODE))
    target_compile_definitions(ccl_kernel_plf_a PRIVATE
    HCCD
    CCL_KERNEL
    CCL_KERNEL_AICPU
    USE_AICORE_REDUCESUM
    USE_AICORE_GATHERV2
    USE_AICORE_GATHERV2_INFER)
#endif()
add_dependencies(tracy_client third_party_tracy_client)
set_target_properties(tracy_client PROPERTIES
    IMPORTED_LOCATION ${TRACY_INSTALL_PATH}/lib64/libtracy-client.a
    INTERFACE_INCLUDE_DIRECTORIES ${TRACY_INSTALL_PATH}/include
)
if(UNIX)
    # Propagate Tracy runtime dependencies to downstream link lines.
    set_property(TARGET tracy_client APPEND PROPERTY INTERFACE_LINK_LIBRARIES "pthread;${CMAKE_DL_LIBS}")
endif()

set(TRACY_SRC_INCLUDE ${TRACY_INSTALL_PATH}/include)
