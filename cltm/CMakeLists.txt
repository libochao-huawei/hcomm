# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.14)
add_library(cltm  SHARED)

target_compile_options(cltm PRIVATE 
    -Werror 
)

#头文件搜索路径
target_include_directories(cltm PRIVATE
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/hccl
    ${TOP_DIR}/open_source/json/include
)

add_subdirectory(src)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
    message(STATUS "optional:-std=c++11")   
endif(CMAKE_COMPILER_IS_GNUCXX)

set(INSTALL_LIBRARY_DIR lib)

install(TARGETS cltm  OPTIONAL
    EXPORT rdfx-targets
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}
	
)

