# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.5)
project(hcomm_test CXX)
option(MOCK_HCCL "enable mock hccl" OFF)   # 默认 OFF，可被 -DMOCK_HCCL=ON/1 覆盖

set(ASCEND_HOME_PATH $ENV{ASCEND_HOME_PATH} CACHE PATH "Ascend SDK root directory (or set ASCEND_HOME_PATH env)")
if(NOT ASCEND_HOME_PATH)
  message(WARNING "ASCEND_HOME_PATH is not set. You can pass -DASCEND_HOME_PATH=/path/to/ascend when invoking cmake.")
endif()

set(HCOMM_BASE ../../)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# EnsureSharedOrStub
# 用法: EnsureSharedOrStub(<name> <out_var> [<path1> <path2> ...])
# 示例: EnsureSharedOrStub(hcomm COMMON_TO_LINK ${ASCEND_HOME_PATH})
function(EnsureSharedOrStub name out_var)
    # 把剩余参数当作 find_library 的 PATHS
    set(_paths ${ARGN})

    find_library(_found NAMES ${name} PATHS ${_paths})
    if(_found)
        set(${out_var} ${_found} PARENT_SCOPE)
        return()
    endif()

    # 生成极简 C++ stub
    set(_dir ${CMAKE_BINARY_DIR}/stubs)
    file(MAKE_DIRECTORY ${_dir})
    set(_src ${_dir}/${name}_stub.cpp)
    file(WRITE ${_src} "extern \"C\" int __stub_placeholder(void) { return 0; }\n")

    set(_target ${name}_stub_target)
    if(NOT TARGET ${_target})
        add_library(${_target} SHARED ${_src})
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME "${name}" LINKER_LANGUAGE CXX)
    endif()

    set(${out_var} ${_target} PARENT_SCOPE)
endfunction()

set(ACL_INC_DIR   "${ASCEND_HOME_PATH}/include")
set(ACL_LIB_DIR   "${ASCEND_HOME_PATH}/lib64")

link_directories(
  "${ACL_LIB_DIR}"
)

add_executable(hcomm_test hcomm_test.cpp)

target_include_directories(hcomm_test PRIVATE
  ${HCOMM_BASE}/inc
  ${HCOMM_BASE}/inc/hccl
  ${HCOMM_BASE}/pkg_inc
  ${HCOMM_BASE}/src/pub_inc
  ${ACL_INC_DIR}
)

# 把 bool 转成 0/1
if(MOCK_HCCL)
  set(_MOCK_HCCL_VAL 1)
else()
  set(_MOCK_HCCL_VAL 0)
endif()
target_compile_definitions(hcomm_test PRIVATE MOCK_HCCL=${_MOCK_HCCL_VAL})

target_compile_options(hcomm_test PRIVATE
    # -U_FORTIFY_SOURCE
    -O0 -g
    # $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)

EnsureSharedOrStub(hcomm HCCL_FWK_TO_LINK)

EnsureSharedOrStub(hccl HCCL_TO_LINK)

# HCCL_FWK_TO_LINK 要么是真实库路径（字符串），要么是 stub 的 target 名称
target_link_libraries(hcomm_test PRIVATE ${HCCL_FWK_TO_LINK})

target_link_libraries(hcomm_test PRIVATE 
    ${HCCL_TO_LINK}
    ascendcl) 

target_link_libraries(hcomm_test PRIVATE pthread)

message(STATUS "CMake configuration done. Binaries will be placed in ${CMAKE_BINARY_DIR}/bin")
message(STATUS "ASCEND_HOME_PATH = ${ASCEND_HOME_PATH}")

add_custom_command(
    TARGET hcomm_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/libhcomm.so
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/libhccl.so
)

message(STATUS "post build: libhcomm.so and libhccl.so will be deleted in ${CMAKE_BINARY_DIR}")