# ----------------------------------------------------------------------------
# This program is free software, you can redistribute it and/or modify.
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------


add_library(intf_pub_base INTERFACE)

target_compile_definitions(intf_pub_base INTERFACE
	CFG_BUILD_DEBUG
)

target_compile_options(intf_pub_base INTERFACE
    -D_GLIBCXX_USE_CXX11_ABI=0
    -g
    -fprofile-arcs
    -ftest-coverage
    $<$<BOOL:${ENABLE_GCOV}>:--coverage>
    -w
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
    $<$<BOOL:${ENABLE_ASAN}>:-fsanitize=address,undefined -fno-stack-protector -fno-omit-frame-pointer>
    $<$<BOOL:${ENABLE_GCOV}>:-fprofile-arcs -ftest-coverage>
    $<$<BOOL:${ENABLE_TEST}>:-ffunction-sections -fdata-sections>
    -fPIC
    -pipe
)

target_link_options(intf_pub_base INTERFACE
    $<$<BOOL:${ENABLE_GCOV}>:-fprofile-arcs>
    $<$<BOOL:${ENABLE_GCOV}>:-ftest-coverage>
    $<$<BOOL:${ENABLE_ASAN}>:-fsanitize=address,undefined -fsanitize-recover=address>
    $<$<BOOL:${ENABLE_TEST}>:-Wl,--gc-sections>
)


add_library(intf_pub INTERFACE)

target_link_libraries(intf_pub INTERFACE
    $<BUILD_INTERFACE:intf_pub_base>
    $<$<BOOL:${ENABLE_TEST}>:-Wl,--no-whole-archive>
    -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
)