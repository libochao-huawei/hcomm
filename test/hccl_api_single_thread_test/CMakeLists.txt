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
    ut_HcclCommInitClusterInfo_API_test.cc
    ut_HcclCommInitClusterInfoConfig_API_test.cc
    ut_HcclCreateSubCommConfig_API_test.cc
    ut_HcclGetRootInfo_API_test.cc
    ut_HcclCommInitRootInfo_API_test.cc
    ut_HcclCommInitRootInfoConfig_API_test.cc
    ut_HcclSetConfig_API_test.cc
    ut_HcclGetConfig_API_test.cc
    ut_HcclGetCommName_API_test.cc
    ut_HcclAllReduce_API_test.cc
    ut_HcclBroadcast_API_test.cc
    ut_HcclReduceScatter_API_test.cc
    ut_HcclReduceScatterV_API_test.cc
    ut_HcclScatter_API_test.cc
    ut_HcclAllGather_API_test.cc
    ut_HcclAllGatherV_API_test.cc
    ut_HcclGetRankSize_API_test.cc
    ut_HcclGetRankId_API_test.cc
    ut_HcclBarrier_API_test.cc
    ut_HcclRecv_API_test.cc
    ut_HcclSend_API_test.cc
    ut_HcclAlltoAllV_API_test.cc
    ut_HcclAlltoAll_API_test.cc
    ut_HcclReduce_API_test.cc
    ut_HcclCommDestroy_API_test.cc
    ut_HcclCommInitAll_API_test.cc
    ut_HcclGetCommAsyncError_API_test.cc
    ut_HcclGetErrorString_API_test.cc
    ut_HcclBatchSendRecv_API_test.cc
    ut_HcclGetCommConfigCapability_API_test.cc
    ut_HcclCommConfigInit_API_test.cc
    ut_HcclCommSuspend_API_test.cc
    ut_HcclCommResume_API_test.cc
    ut_HcclCommSetMemoryRange_API_test.cc
    ut_HcclCommUnsetMemoryRange_API_test.cc
    ut_HcclCommActivateCommMemory_API_test.cc
    ut_HcclCommDeactivateCommMemory_API_test.cc
    ut_HcclCommWorkingDevNicSet_API_test.cc
    ut_HcclCommEngineCtx_API_test.cc
    ut_HcclCommRegMem_API_test.cc
    ut_HcclCommChannel_API_test.cc
)

add_executable(hccl_utest_api_single_thread ${src_to_test_list})

if(BUILD_OPEN_PROJECT)
    set(package_include_list
        ${ASCEND_CANN_PACKAGE_PATH}/include/hccl_alg_analyzer
        ${ASCEND_CANN_PACKAGE_PATH}/include
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/hccl
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/metadef/common/util/error_manager
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/msprof
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/msprof/toolchain
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/runtime
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/runtime/external
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog/toolchain
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/mmpa
        ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux/include/experiment/ascend_hal
        ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux/include/experiment/ascend_hal/driver
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/platform
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/platform/metadef
        ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux/include/experiment/datagw/aicpu
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest/include
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/
    )
else()
    set(package_include_list
    )
endif()

if(NOT BUILD_OPEN_PROJECT)
    set(include_list
        # 测试框架的代码
        ${HCCL_ANALYZER_INC_DIR}/inc
        ${HCCL_ANALYZER_INC_DIR}/checker
        ${HCCL_ANALYZER_INC_DIR}/adapter_v1
        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/transport
        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/notify
        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/stream

        # studio工具的代码
        ${HCCL_INSINGHT_INC_DIR}/cpp_engine/src

        #用于本地跑llt
        ${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg

        # hccl inc目录
        ${TOP_DIR}/ace/comop/inc
        ${TOP_DIR}/ace/comop/inc/hccl
        ${TOP_DIR}/ace/comop/inc/external
        ${TOP_DIR}/ace/comop/inc/external/hccl
        ${TOP_DIR}/ace/comop/hccl/whole/hccl/platform/inc

        # inc目录下头文件
        ${TOP_DIR}/metadef/inc/common/util/error_manager
        ${TOP_DIR}/ace/npuruntime/inc
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/abl/mmpa/inc/mmpa

        # 添加hccl目录
        ${HCCL_CODE_ROOT}/hccl/pub_inc

        # slog头文件
        ${TOP_DIR}/abl/slog/inc/toolchain
        ${TOP_DIR}/abl/atrace/inc/utrace

        ${TOP_DIR}/abl/mmpa/inc/mmpa
    )
else()
    set(include_list
    )
endif()

#头文件搜索路径
target_include_directories(hccl_utest_api_single_thread PRIVATE
    ${package_include_list}
    ${include_list}
    ${algorithm_include_list}
    ${framework_include_list}
    ${common_include_list}
    ${platform_include_list}
    ${other_include_list}
    ${THIRD_PARTY_NLOHMANN_PATH}
    ${mockcpp_INCLUDE_DIRS}
    ${HCCL_STUB_CODE_ROOT}/framework_stub
)

target_compile_definitions(hccl_utest_api_single_thread PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    google=ascend_private
    -DOPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)

target_compile_options(hccl_utest_api_single_thread PRIVATE
    -U_FORTIFY_SOURCE
    -fno-var-tracking-assignments
    -Os
    -O3 -g
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer>
)

if(BUILD_OPEN_PROJECT) # 蓝区构建
    target_link_directories(hccl_utest_api_single_thread PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    find_library(LIBRARY_ASCEND_HAL libascend_hal.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/lib/stub)
    target_link_libraries(hccl_utest_api_single_thread PRIVATE
        framework_stub
        ${LIBRARY_ASCEND_HAL}
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
        ${LIBRARY_GTEST}
        ${LIBRARY_MOCKCPP}
        -lpthread
        -Wl,--start-group
        hccl
        hccl_alg
        hccl_plf
        -Wl,--end-group
        ascend_protobuf
        -lrt
        -ldl

    )
    add_dependencies(hccl_utest_api_single_thread hccl hccl_alg framework_stub)
else() # 黄区CI构建
    target_link_libraries(hccl_utest_api_single_thread  PRIVATE
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:runtime_headers>
        $<BUILD_INTERFACE:cce_headers>
        $<BUILD_INTERFACE:network_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:kernel_tiling_headers> 
        c_sec_static
        ascend_protobuf
        -lrt
        -ldl
        -Wl,--start-group
        hccl
        hccl_alg
        hccl_plf
        -Wl,--end-group
    #    exe_graph
    )
endif()

if(NOT BUILD_OPEN_PROJECT)
run_llt_test(
   TARGET  hccl_utest_api_single_thread
   TASK_NUM 1
)
endif()
