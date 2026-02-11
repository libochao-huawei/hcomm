# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_library(intf_pub_base INTERFACE)

target_compile_definitions(intf_pub_base INTERFACE
	CFG_BUILD_DEBUG
)

target_compile_options(intf_pub_base INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
    -D_GLIBCXX_USE_CXX11_ABI=0
    -O0 -g
    -w
    -fPIC
    -pipe
)

target_compile_options(intf_pub_base INTERFACE
    $<$<BOOL:${ENABLE_ASAN}>:
        -fsanitize=address              # 启用地址消毒器
        -fsanitize=leak                 # 启用内存泄漏检测
        -fsanitize-recover=address,all  # 允许地址和其他消毒器错误恢复
        -fno-stack-protector            # 禁用栈保护
        -fno-omit-frame-pointer         # 保留帧指针
    >
)

target_link_options(intf_pub_base INTERFACE
    $<$<BOOL:${ENABLE_ASAN}>:
        -fsanitize=address           # 链接时启用地址消毒器
        -fsanitize=leak              # 链接时启用内存泄漏检测
        -fsanitize-recover=address   # 允许地址错误恢复
    >
)

target_compile_options(intf_pub_base INTERFACE
    $<$<BOOL:${ENABLE_GCOV}>:-fprofile-arcs -ftest-coverage>
)

target_link_options(intf_pub_base INTERFACE
    $<$<BOOL:${ENABLE_GCOV}>:-fprofile-arcs -ftest-coverage>
)

target_link_libraries(intf_pub_base INTERFACE
    $<$<BOOL:${ENABLE_GCOV}>:-lgcov>
)

add_library(intf_pub INTERFACE)

target_link_libraries(intf_pub INTERFACE
    $<BUILD_INTERFACE:intf_pub_base>
    json
)

add_library(intf_llt_pub INTERFACE)

target_link_libraries(intf_llt_pub INTERFACE
    $<BUILD_INTERFACE:intf_pub>
    $<$<BOOL:${ENABLE_TEST}>:mockcpp>
    $<$<BOOL:${ENABLE_TEST}>:gtest>
    -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
)
