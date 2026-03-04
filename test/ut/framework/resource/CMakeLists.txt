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
    main.cc
    hccl_api_base_test.cc
    # ut_channel_API_test.cc
    # ut_DpuNotifyManager_test.cc
    ut_HcclEngineCtxDestroy_API_test.cc
    ut_HcclEngineCtxDestroy_API_V2_test.cc
    ut_HcclEngineCtxGet_API_test.cc
    ut_HcclEngineCtxCreate_API_test.cc
    ut_HcclCreateOpResCtx_API_test.cc
    ut_HcclGetRemoteIpcHcclBuf_API_test.cc
)

add_executable(hccl_utest_framework_resource ${src_to_test_list})

#头文件搜索路径
target_include_directories(hccl_utest_framework_resource PRIVATE
    ${UT_COMMON_INCLUDE_LIST}
)
 
target_compile_definitions(hccl_utest_framework_resource PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)
 
target_compile_options(hccl_utest_framework_resource PRIVATE
    -U_FORTIFY_SOURCE
    -fno-var-tracking-assignments
    -Os
    -O0 -g --coverage -fprofile-arcs -ftest-coverage -fno-access-control
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)
 
target_link_directories(hccl_utest_framework_resource PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)
target_link_libraries(hccl_utest_framework_resource PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    -pthread
    hccl_llt
    -Wl,--whole-archive
    -Wl,--no-whole-archi
    -lgcov
    -lrt
    -ldl
)
add_dependencies(hccl_utest_framework_resource hccl_llt)

run_llt_test(
   TARGET  hccl_utest_framework_resource
   TASK_NUM 1
)