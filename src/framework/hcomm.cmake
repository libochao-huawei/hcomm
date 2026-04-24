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

# 添加 legacy 目录
add_subdirectory(${HCOMM_ROOT_DIR}/legacy)

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

target_link_options(hcomm PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -s
)

target_compile_definitions(hcomm PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hcomm PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP
    )

    target_include_directories(hcomm PRIVATE
        ${HCCL_BASE_DIR}/pub_inc
        ${HCCL_BASE_DIR}/pub_inc/new/
        ${HCCL_BASE_DIR}/../include
        ${HCCL_BASE_DIR}/../include/hccl
        ${HCCL_BASE_DIR}/../pkg_inc/
        ${HCCL_BASE_DIR}/../pkg_inc/hccl/
        ${HCCL_BASE_DIR}/../pkg_inc/hcomm/ccu
        ${HCCL_BASE_DIR}/platform/common
        ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/include/legacy
        ${HCCL_BASE_DIR}/platform/legacy/inc
        ${RDMA_CORE_INCLUDE_DIR}
        ${HCCL_BASE_DIR}/platform/hccp/inc/
        ${HCCL_BASE_DIR}/platform/resource/dispatcher_ctx
        ${HCCL_BASE_DIR}/platform/hccp/inc/network

        ${HCCL_BASE_DIR}/platform/inc/adapter
        
        ${THIRD_PARTY_NLOHMANN_PATH}

        ${HCCL_BASE_DIR}/common/debug/profiling/inc
        ${HCCL_BASE_DIR}/common/debug/profiling/inc/host
        ${HCCL_BASE_DIR}/common/debug/config
        ${HCCL_BASE_DIR}/legacy/interface
        ${HCCL_BASE_DIR}/legacy/framework/topo/new_topo_builder/rank_graph

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

        ${THIRD_PARTY_NLOHMANN_PATH}

        ${HCCL_BASE_DIR}/framework/op_base/src

        ${ORION_HEAD_LIST}
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
        ${TOP_DIR}/hcomm/include/hccl/                      # ${ASCEND_CANN_PACKAGE_PATH}/include
        ${TOP_DIR}/hcomm/src/algorithm/pub_inc
        ${TOP_DIR}/abl/msprof/inc                           # experiment/msprof/
        ${TOP_DIR}/ace/npuruntime/inc                       # experiment/runtime/
        ${TOP_DIR}/inc/driver                               # experiment/ascend_hal/driver/
        ${TOP_DIR}/inc/                                     # experiment/ascend_hal/driver/
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/hcomm/
        ${HCCL_BASE_DIR}/common/debug/profiling/inc
        ${HCCL_BASE_DIR}/common/debug/profiling/inc/host
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/debug/config
        ${TOP_DIR}/ace/npuruntime/inc/runtime
        ${TOP_DIR}/runtime/include/external

        ${TOP_DIR}/abl/msprof/inc/toolchain
        ${HCCL_BASE_DIR}/platform/common
        ${HCCL_BASE_DIR}/inc
        ${TOP_DIR}/inc/aicpu
        ${HCCL_BASE_DIR}/pkg_inc

        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc

        ${ORION_HEAD_LIST}
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

install(TARGETS hcomm
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)
