# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set(FRAMEWORK_STUB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

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
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest/include
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/json/include
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

set(framework_stub_list
    #/llt/hccl/stub
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_gdr.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_sal.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_profiling_plugin.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_mc2.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_GenRankTable.cc

    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_ge.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_fe.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_error_manager.cc
    ${FRAMEWORK_STUB_ROOT}/framework_stub/llt_hccl_stub_gert.cc
)

add_library(framework_stub STATIC ${framework_stub_list})

#头文件搜索路径
target_include_directories(framework_stub  PRIVATE
    ${package_include_list}
    ${include_list}
    ${algorithm_include_list}
    ${framework_include_list}
    ${common_include_list}
    ${platform_include_list}
    ${other_include_list}
    ${THIRD_PARTY_NLOHMANN_PATH}
    ./
)

target_compile_definitions(framework_stub    PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
    HALF_T=float
    google=ascend_private
    CCL_KERNEL
    -DOPEN_HCCL_TEST
    OPEN_BUILD_PROJECT
)

target_compile_options(framework_stub   PRIVATE
    -U_FORTIFY_SOURCE -g
    -std=c++14
)
    
target_link_directories(framework_stub PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/lib64
)

target_link_libraries(framework_stub  PUBLIC
    -Wl,--no-as-needed
    -lascendalog
    -lmmpa
    -lc_sec
    -lhccl
    -lhccl_alg
    -lhccl_plf
    -lascend_protobuf
    -lrt
    -ldl
    -lpthread
)

add_dependencies(framework_stub hccl hccl_alg hccl_plf)
