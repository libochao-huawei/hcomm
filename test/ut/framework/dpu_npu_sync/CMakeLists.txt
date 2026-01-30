# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(src_to_test_list
    ../communicator/main.cc
    ../communicator/hccl_api_base_test.cc
    ut_sync_data_device_test.cc
)
 
add_executable(hccl_utest_framework_communicator ${src_to_test_list})

# 定义头文件路径
set(HCCL_INCLUDE_DIR "/srv/workspace/br_hisi_trunk_ai_5099251946032746497/work_code/hcomm/src/framework/inc")

# 将头文件路径添加到列表变量
list(APPEND UT_COMMON_INCLUDE_LIST ${HCCL_INCLUDE_DIR})
 
#头文件搜索路径
target_include_directories(hccl_utest_framework_communicator PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
)
 
target_compile_definitions(hccl_utest_framework_communicator PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)
 
target_compile_options(hccl_utest_framework_communicator PRIVATE
    -U_FORTIFY_SOURCE
    -fno-var-tracking-assignments
    -Os
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)
 
target_link_directories(hccl_utest_framework_communicator PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)
target_link_libraries(hccl_utest_framework_communicator PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    -lpthread
    hccl_llt
    -Wl,--whole-archive
    -Wl,--no-whole-archi
    -lgcov
    -lrt
    -ldl
)
add_dependencies(hccl_utest_framework_communicator hccl_llt)
 
run_llt_test(
   TARGET  hccl_utest_framework_communicator
   TASK_NUM 1
)