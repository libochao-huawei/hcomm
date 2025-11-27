# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccl_ut_prepare_env_list 
    main.cc
)
add_executable(hccl_ut_prepare_env  ${hccl_ut_prepare_env_list})

target_include_directories(hccl_ut_prepare_env  PRIVATE
    ${include_list}
)

target_compile_options(hccl_ut_prepare_env   PRIVATE 
    -U_FORTIFY_SOURCE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
)

target_compile_options(hccl_ut_prepare_env   PRIVATE 
    -U_FORTIFY_SOURCE
    # -fsanitize=address -fsanitize=leak -fsanitize-recover=address
    -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    # -Wall -Wextra -Wfloat-equal -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
)

target_link_libraries(hccl_ut_prepare_env  PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    # $<BUILD_INTERFACE:intf_llt_pub_asan_cxx14>
    -lpthread
    -Wl,--whole-archive
    # ${HCCL_OPEN_CODE_ROOT}/third_party/mockcpp/build/src/libmockcpp.a
    # libgtest.a
    -Wl,--no-whole-archive
    -lgcov
    -lrt
    -lpthread
    # -fsanitize=address -fsanitize=leak -fsanitize-recover=address
)

add_dependencies(hccl_ut_prepare_env gtest_main)

# add_custom_command(
#     TARGET hccl_ut_prepare_env
#     POST_BUILD
#     COMMAND
#         ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH} ./hccl_ut_prepare_env
#     COMMENT "Run ops hccl_ut_prepare_env"
# )
# if(NOT BUILD_OPEN_PROJECT)
run_llt_test(
    TARGET  hccl_ut_prepare_env
    TASK_NUM 1
)
# endif()
