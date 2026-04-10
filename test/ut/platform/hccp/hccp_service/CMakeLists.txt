# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set(hccp_utest_list
    main.cc
    stub/tsd.c 
    stub/mm.c 
    stub/hccp_pub.c
    ${SRC_DIR}/test/ut/platform/hccp/stub/ut_dispatch.cc
    ${SRC_DIR}/src/platform/hccp/hccp_service/param.c
    ${SRC_DIR}/src/platform/hccp/common/dl_hal_function.c
    ${SRC_DIR}/src/platform/hccp/common/ascend_hal_dl.c
    ${SRC_DIR}/src/platform/hccp/hccp_service/main.c
    st_test.cc 
    tc_hccp.c
    )

add_executable(hccp_utest ${hccp_utest_list})

set_property(TARGET hccp_utest PROPERTY C_STANDARD 11)
target_include_directories(hccp_utest PRIVATE
    stub
    ${SRC_DIR}/test/ut/platform/hccp/ut_include
    ${SRC_DIR}/test/ut/depends/include
    ${SRC_DIR}/test/ut/depends/pkg_inc/aicpu/tsd
    ${SRC_DIR}/test/ut/depends/pkg_inc/aicpu/
    ${SRC_DIR}/test/ut/hccp/rdma_service_normal/rdma_service_normal/include
    ${SRC_DIR}/test/ut/depends/include
    ${SRC_DIR}/src/platform/hccp/hccp_service
    ${SRC_DIR}/src/platform/hccp/rdma_agent/adapter
    ${SRC_DIR}/src/platform/hccp/rdma_agent/adapter/async
    ${SRC_DIR}/src/platform/hccp/rdma_agent/hdc
    ${SRC_DIR}/src/platform/hccp/rdma_agent/inc
    ${SRC_DIR}/src/platform/hccp/rdma_service
    ${SRC_DIR}/src/platform/hccp/common
    ${SRC_DIR}/src/platform/hccp/inc/private/network
    ${SRC_DIR}/src/platform/hccp/inc/network
    ${SRC_DIR}/src/platform/hccp/external_depends/inc
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    ${SRC_DIR}/src/platform/hccp/external_depends/host_lite
    ${RDMA_CORE_INCLUDE_DIR}
    ${SRC_DIR}/src/platform/hccp/external_depends/rdma-core/providers/hns
    ${SRC_DIR}/src/platform/hccp/external_depends/rdma-core/include	 
    ${CANN_3RD_LIB_PATH}/json/include/	 
    ${CANN_3RD_LIB_PATH}/gtest/include/	 
    ${CANN_3RD_LIB_PATH}/mockcpp/include/	 
    ${CANN_3RD_LIB_PATH}/mockcpp/include/3rdparty/
)

target_compile_options(hccp_utest PRIVATE
        -U_FORTIFY_SOURCE
 	     -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
 	     $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>
 	     -Wall
 	     -Wextra
 	     -Werror
 	     -pedantic
 	     -Wshadow
)

target_compile_definitions(hccp_utest PRIVATE
    HNS_ROCE_LLT 
    USE_DISPATCH_EXPECT
    CONFIG_CGROUP
    CONFIG_HCCP_LLT
)

target_link_libraries(hccp_utest PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    -lrt
    -pthread
    -lgcov
    -ldl
)

run_llt_test(
    TARGET hccp_utest
    TASK_NUM 1
)
