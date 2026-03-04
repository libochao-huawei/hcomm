# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(HCCL_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../)
set(HCCL_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
get_filename_component(HCCL_CODE_ROOT "${HCCL_CODE_ROOT}" REALPATH)
get_filename_component(HCCL_LLT_ROOT "${HCCL_LLT_ROOT}" REALPATH)
set(HCCL_V2_CODE_ROOT ${HCCL_CODE_ROOT}/src/legacy/)

file(GLOB_RECURSE HCCL_V2_UTEST_FILES RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    main.cc
)

add_executable(hccl_v2_utest ${HCCL_V2_UTEST_FILES})

FILE(GLOB_RECURSE ut_sources_list LIST_DIRECTORIES TRUE ${HCCL_LLT_ROOT}/ut)

foreach (dir ${ut_sources_list})
    IF ((IS_DIRECTORY ${dir} ) AND (${dir} MATCHES "test/legacy") AND (NOT ${dir} MATCHES "legacy/depends"))
        target_include_directories(hccl_v2_utest PUBLIC ${dir})
    endif ()
endforeach ()

FILE(GLOB_RECURSE sources_list LIST_DIRECTORIES TRUE ${HCCL_CODE_ROOT}/src/legacy)

message(STATUS "************************=====${sources_list}")
foreach (dir ${sources_list})
    IF ((IS_DIRECTORY ${dir} ) AND (${dir} MATCHES "src/legacy") AND (NOT ${dir} MATCHES "src/legacy/mdpi"))
        target_include_directories(hccl_v2_utest PUBLIC ${dir})
    endif ()
endforeach ()

# 自动找到项目内所有头文件
FILE(GLOB_RECURSE hccl_v2_dir_list LIST_DIRECTORIES TRUE ${HCCL_V2_CODE_ROOT})
foreach (dir ${hccl_v2_dir_list})
    IF ((IS_DIRECTORY ${dir}) AND (${dir} MATCHES "hcomm/src/legacy"))
        target_include_directories(hccl_v2_utest PUBLIC ${dir})
    endif ()
endforeach ()

# 添加项目外部头文件
target_include_directories(hccl_v2_utest PRIVATE
        ${HCCL_CODE_ROOT}/include/
        ${HCCL_CODE_ROOT}/include/hccl
        ${HCCL_CODE_ROOT}/pkg_inc
        ${HCCL_CODE_ROOT}/pkg_inc/hccl

        ${HCCL_CODE_ROOT}/test/ut/depends/include/
        ${HCCL_CODE_ROOT}/test/ut/depends/include/driver/
        ${HCCL_CODE_ROOT}/test/ut/depends/include/mmpa/
        ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/
        ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/base
        # ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/runtime/
        # ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/runtime/runtime/
        ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/profiling/
        ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/trace/
        ${HCCL_CODE_ROOT}/test/legacy/depends/toolchain/
        ${HCCL_CODE_ROOT}/test/legacy/depends/pkg_inc/runtime/
        # ${HCCL_CODE_ROOT}/test/legacy/depends/pkg_inc/runtime/runtime/

        ${HCCL_CODE_ROOT}/src/pub_inc/
        ${HCCL_CODE_ROOT}/src/platform/hccp/inc/network/
        ${RDMA_CORE_INCLUDE_DIR}

        ${CANN_3RD_LIB_PATH}/json/include/
        ${CANN_3RD_LIB_PATH}/gtest/include/
        ${CANN_3RD_LIB_PATH}/mockcpp/include/
        ${CANN_3RD_LIB_PATH}/mockcpp/include/3rdparty/
        ${HCCL_LLT_ROOT}/common/virtual_topo_stub/
        ${HCCL_LLT_ROOT}/ut/framework/topo/ranktable/
        ${HCCL_CODE_ROOT}/externel_depends/tsch
    )

target_link_libraries(hccl_v2_utest PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    -pthread
    # hccl_llt
    -Wl,--whole-archive
    -Wl,--no-whole-archi
    -lgcov
    -lrt
    -ldl
    -pthread
)


# 编译选项
target_compile_options(hccl_v2_utest PRIVATE
        -std=c++14
        -Werror
        -fno-common
        -fno-strict-aliasing
        -pipe
        -U_FORTIFY_SOURCE
        -fno-var-tracking-assignments
        -Os
        -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)

message(STATUS "ORION_MODE=${ORION_MODE}")
target_compile_definitions(hccl_v2_utest PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    # private=public
    # protected=public
    HCOMM_CODE_ROOT_DIR="${HCCL_CODE_ROOT}"
    # $<$<STREQUAL:${ORION_MODE},true>:ORION_MODE>
    # ORION_MODE

    ORION_AICPU_DEPLOY
    ORION_HOST_DEPLOY
)

add_subdirectory(all_source_code)
add_subdirectory(common)
add_subdirectory(service)
add_subdirectory(framework)
add_subdirectory(aicpu)
add_subdirectory(unified_platform)

run_llt_test(
    TARGET hccl_v2_utest
    TASK_NUM 1
)