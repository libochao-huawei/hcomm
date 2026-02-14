# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(rdma_agent_utest_list
    main.cc
    ut_ra_common.cc
)

add_executable(rdma_agent_utest ${rdma_agent_utest_list})

target_include_directories(rdma_agent_utest PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
)

target_compile_definitions(rdma_agent_utest PRIVATE
    HALF_T=float
    google=ascend_private
    -DCCL_LLT
)

target_compile_options(rdma_agent_utest PRIVATE
    -U_FORTIFY_SOURCE
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
    -Wall
    -Wextra
    -Werror
    -pedantic
    -Wshadow
)

target_link_options(rdma_agent_utest PRIVATE
    -Wl,--no-undefined
)

target_link_libraries(rdma_agent_utest PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    hccp_llt
    hccp_stub
    -lpthread
    -lgcov
    -lrt
)

run_llt_test(
    TARGET rdma_agent_utest 
    TASK_NUM 1
)
