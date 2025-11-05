# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
add_library(hcom_gradient_split_tune  SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gradient_split_tune.cc
)

target_compile_options(hcom_gradient_split_tune  PRIVATE
    -Werror
	-Wall
	-fno-common
	-fno-strict-aliasing
	-pipe
)

#头文件搜索路径
target_include_directories(hcom_gradient_split_tune  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/toolchain
    ${TOP_DIR}/libc_sec/include
    ${TOP_DIR}/metadef/inc/common/util
    ${TOP_DIR}/metadef/inc/external
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/open_source/json/include
    ${TOP_DIR}/abl/mmpa/inc/mmpa
    ${TOP_DIR}/ace/npuruntime/runtime/platform/inc/
)

target_link_libraries(hcom_gradient_split_tune   PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<BUILD_INTERFACE:slog_headers>
    $<BUILD_INTERFACE:hccl_headers>
    $<BUILD_INTERFACE:mmpa_headers>
    -Wl,--no-as-needed
    slog
    mmpa
    platform
    c_sec
    -Wl,--as-needed
    )
install(TARGETS hcom_gradient_split_tune
    LIBRARY DESTINATION lib  OPTIONAL
)

