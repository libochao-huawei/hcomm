# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(hccp_test_list
    main.cc
    ut_rs_common.cc
)

add_executable(hccp_test.bin ${hccp_test_list})

target_include_directories(hccp_test.bin PRIVATE
    ${HCCP_DIR}/inc
    ${HCCP_DIR}/inc/network
    ${HCCP_DIR}/inc/private/network
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    ${TEST_DIR}/st/hccp/stub
    ${RDMA_CORE_INCLUDE_DIR}
)

target_compile_options(hccp_test.bin PRIVATE
    -U_FORTIFY_SOURCE
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
    -Wall
    -Wextra
    -Werror
    -pedantic
    -Wshadow
)

target_link_options(hccp_test.bin PRIVATE
    -Wl,--no-undefined
)

target_link_libraries(hccp_test.bin PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    test_ra
    ascend_hal_host_stub
    -lpthread
    -lgcov
    -lrt
)

install(
    TARGETS hccp_test.bin
    RUNTIME DESTINATION ${HCCP_TEST_DIR}/host
    COMPONENT hccp_test
)
# run_llt_test(
#     TARGET hccp_test 
#     TASK_NUM 1
# )
