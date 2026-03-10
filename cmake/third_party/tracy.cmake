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

# Tracy Profiler 集成 (BSD 3-Clause License)
# 仅集成 import-chrome 转换工具的编译，不集成 Client 运行时（使用自研 HCCL Log 替代）
# 用途：将 HCCL 日志转换为 .tracy 格式，供 Tracy GUI 离线分析

unset(tracy_FOUND CACHE)
unset(TRACY_IMPORT_CHROME CACHE)

# GitCode 镜像地址（国内加速），对应 Tracy v0.11.1
set(TRACY_VERSION "0.11.1")
set(TRACY_FILE "tracy-${TRACY_VERSION}.tar.gz")
set(TRACY_URL "https://gitcode.com/gh_mirrors/tr/tracy/archive/refs/tags/v${TRACY_VERSION}.tar.gz")
set(TRACY_PKG_PATH ${CANN_3RD_LIB_PATH}/${TRACY_FILE})
set(TRACY_SRC_PATH ${CANN_3RD_LIB_PATH}/tracy)
set(TRACY_INSTALL_PATH ${CANN_3RD_LIB_PATH}/tracy_install)

# 查找是否已经编译安装
find_program(TRACY_IMPORT_CHROME
    NAMES import-chrome
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    PATHS ${TRACY_INSTALL_PATH}/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tracy
    FOUND_VAR
    tracy_FOUND
    REQUIRED_VARS
    TRACY_IMPORT_CHROME
)
message(STATUS "[ThirdParty] Found Tracy import-chrome: ${tracy_FOUND}")

if(tracy_FOUND AND NOT FORCE_REBUILD_CANN_3RD)
    message(STATUS "[ThirdParty] Tracy import-chrome found in ${TRACY_INSTALL_PATH}, and not force rebuild cann third_party")
else()
    if(EXISTS ${TRACY_PKG_PATH})
        message(STATUS "[ThirdParty] Found local Tracy package: ${TRACY_PKG_PATH}")
        set(TRACY_PROJECT_URL ${TRACY_PKG_PATH})
    else()
        message(STATUS "[ThirdParty] Downloading Tracy from ${TRACY_URL}")
        set(TRACY_PROJECT_URL ${TRACY_URL})
    endif()

    # Tracy import-chrome 编译选项
    set(TRACY_CXXFLAGS "-D_GLIBCXX_USE_CXX11_ABI=0 -O2 -D_FORTIFY_SOURCE=2 -fPIC -fstack-protector-all")

    include(ExternalProject)
    ExternalProject_Add(third_party_tracy
        URL ${TRACY_PROJECT_URL}
        TLS_VERIFY OFF
        DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}
        DOWNLOAD_NO_PROGRESS TRUE
        SOURCE_DIR ${TRACY_SRC_PATH}
        # 只编译 import-chrome 工具（不编译 Server GUI）
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            ${CMAKE_COMMAND} -E env
                "CXX=${CMAKE_CXX_COMPILER}"
                "CXXFLAGS=${TRACY_CXXFLAGS}"
            $(MAKE) -C <SOURCE_DIR>/import -j4
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${TRACY_INSTALL_PATH}/bin &&
            ${CMAKE_COMMAND} -E copy_if_different
                <SOURCE_DIR>/import/build/unix/import-chrome
                ${TRACY_INSTALL_PATH}/bin/import-chrome
        UPDATE_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
    )
endif()

# 导出头文件路径（供 hcomm_profiler.h 引用 Tracy 的 Chrome JSON 格式定义）
set(TRACY_SRC_INCLUDE ${TRACY_SRC_PATH}/public)
