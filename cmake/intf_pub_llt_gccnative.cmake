# ----------------------------------------------------------------------------
# This program is free software, you can redistribute it and/or modify.
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------


add_library(intf_llt_base INTERFACE)

target_compile_definitions(intf_llt_base INTERFACE
	CFG_BUILD_DEBUG
)

target_compile_options(intf_llt_base INTERFACE
    -D_GLIBCXX_USE_CXX11_ABI=0
    -g
    -fprofile-arcs
    -ftest-coverage
    --coverage
    -w
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
    $<$<STREQUAL:${ENABLE_ASAN},ON>:-fsanitize=address -fno-omit-frame-pointer -static-libasan -fsanitize=undefined -static-libubsan -fsanitize=leak -static-libtsan>
    -fPIC
    -pipe
)

target_link_options(intf_llt_base INTERFACE
    -fprofile-arcs -ftest-coverage
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -static-libasan -fsanitize=undefined  -static-libubsan -fsanitize=leak -static-libtsan>

)


add_library(intf_llt_pub INTERFACE)

target_link_libraries(intf_llt_pub INTERFACE
    $<BUILD_INTERFACE:intf_llt_base>
    -Wl,--whole-archive
    ${ASCEND_3RD_LIB_PATH}/mockcpp_shared/lib/libmockcpp.a
    ${ASCEND_3RD_LIB_PATH}/gtest_shared/lib/libgtest.so
    -Wl,--no-whole-archive
    -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
)