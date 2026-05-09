# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 hcomm 动态链接库，在 host 侧使用
add_library(hcomm SHARED)

# 定义预处理宏
target_compile_definitions(hcomm PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

# 编译选项
target_compile_options(hcomm PRIVATE
    -Werror
    -Wfloat-equal
    -Wall
    -fno-common
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
    -fstack-protector-all
    $<$<CONFIG:Debug>:-g>
)

# 链接选项
target_link_options(hcomm PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -s
)

target_include_directories(hcomm PRIVATE
    ${HCOMM_DIR}/include
    ${HCOMM_DIR}/include/hccl
    ${HCOMM_DIR}/pkg_inc
    ${HCOMM_DIR}/pkg_inc/hccl
    ${HCOMM_DIR}/pkg_inc/hcomm/ccu
    ${HCOMM_DIR}/src/pub_inc
    ${HCOMM_DIR}/src/pub_inc/inner
    ${HCOMM_DIR}/src/pub_inc/new

    # src/framework 头文件
    ${HCOMM_DIR}/src/framework
    ${HCOMM_DIR}/src/framework/inc

    # src/common 头文件
    ${HCOMM_DIR}/src/common/debug/profiling/inc
    ${HCOMM_DIR}/src/common/debug/profiling/inc/host
    ${HCOMM_DIR}/src/common/debug/config

    # src/algorithm 头文件
    ${HCOMM_DIR}/src/algorithm/pub_inc
    ${HCOMM_DIR}/src/algorithm/base/inc
    ${HCOMM_DIR}/src/algorithm/base/alg_template
    ${HCOMM_DIR}/src/algorithm/base/communicator

    # src/legacy 头文件
    ${LEGACY_INCLUDE_LIST}
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hcomm PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP
    )

    target_include_directories(hcomm PRIVATE
        # 三方件头文件
        ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/include/legacy
        ${RDMA_CORE_INCLUDE_DIR}
        ${THIRD_PARTY_NLOHMANN_PATH}

        # runtime头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        # mmpa头文件
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/
        # acl头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/acl/
        # driver头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
        # highlevel_api
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/
        # 包间接口
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
    )

    target_link_directories(hcomm PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
    target_link_libraries(hcomm
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        acl_rt
        hccl_alg
        hccl_plf
        hccl_v2
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
else()
    target_include_directories(hcomm PRIVATE
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/ace/npuruntime/inc
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/inc/
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/ace/npuruntime/inc/runtime
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/abl/msprof/inc/toolchain
        ${TOP_DIR}/inc/aicpu
        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc
    )
    target_link_libraries(hcomm
        $<BUILD_INTERFACE:kernel_tiling_headers>
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        mmpa
        hccl_alg
        hccl_plf
        hccl_v2
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
endif()

# 安装 hcomm 库
install(TARGETS hcomm
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
