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

# 预处理宏定义
target_compile_definitions(hcomm PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
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

# 头文件搜索路径
target_include_directories(hcomm PRIVATE
    ${HCOMM_ROOT_DIR}/include
    ${HCOMM_ROOT_DIR}/include/hccl
    ${HCOMM_ROOT_DIR}/pkg_inc/
    ${HCOMM_ROOT_DIR}/pkg_inc/hccl/
    ${HCOMM_ROOT_DIR}/pkg_inc/hcomm/ccu
    ${HCOMM_ROOT_DIR}/src/pub_inc
    ${HCOMM_ROOT_DIR}/src/pub_inc/new/

    ${HCOMM_ROOT_DIR}/src/algorithm/base/inc
    ${HCOMM_ROOT_DIR}/src/algorithm/pub_inc
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_aiv_template
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template
    ${HCOMM_ROOT_DIR}/src/algorithm/base/mc2_handler
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/component
    ${HCOMM_ROOT_DIR}/src/algorithm/base/communicator
    ${HCOMM_ROOT_DIR}/src/algorithm/base/communicator/legacy
    ${HCOMM_ROOT_DIR}/src/algorithm/impl
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/legacy
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/inc
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/task
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/resource_manager
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor

    ${HCOMM_ROOT_DIR}/src/common/health/
    ${HCOMM_ROOT_DIR}/src/common/stream/
    ${HCOMM_ROOT_DIR}/src/common/launch_device/
    ${HCOMM_ROOT_DIR}/src/common/debug/profiling/inc
    ${HCOMM_ROOT_DIR}/src/common/debug/profiling/inc/host
    ${HCOMM_ROOT_DIR}/src/common/debug/config

    ${HCOMM_ROOT_DIR}/src/platform/resource/socket
    ${HCOMM_ROOT_DIR}/src/platform/inc
    ${HCOMM_ROOT_DIR}/src/platform/inc/adapter
    ${HCOMM_ROOT_DIR}/src/platform/inc/adapter
    ${HCOMM_ROOT_DIR}/src/platform/common
    ${HCOMM_ROOT_DIR}/src/platform/legacy/inc
    ${HCOMM_ROOT_DIR}/src/platform/hccp/inc/
    ${HCOMM_ROOT_DIR}/src/platform/resource/dispatcher_ctx
    ${HCOMM_ROOT_DIR}/src/platform/hccp/inc/network

    ${HCOMM_ROOT_DIR}/src/legacy/interface
    ${HCOMM_ROOT_DIR}/src/legacy/framework/topo/new_topo_builder/rank_graph
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hcomm PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP
    )

    target_include_directories(hcomm PRIVATE
        # hcomm_utils host 侧头文件
        ${CANN_3RD_LIB_PATH}/hcomm_utils/host/include/legacy
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
        ${TOP_DIR}/abl/msprof/inc                           # experiment/msprof/
        ${TOP_DIR}/ace/npuruntime/inc                       # experiment/runtime/
        ${TOP_DIR}/inc/driver                               # experiment/ascend_hal/driver/
        ${TOP_DIR}/inc/                                     # experiment/ascend_hal/driver/
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

install(TARGETS hcomm
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)

# 添加 src/legacy/ 目录下的头文件搜索路径
FILE(GLOB_RECURSE LEGACY_SOURCE_LIST LIST_DIRECTORIES TRUE ${HCOMM_ROOT_DIR}/src/legacy/*)
foreach(legacy_dir ${LEGACY_SOURCE_LIST})
    if(IS_DIRECTORY ${legacy_dir})
        target_include_directories(hcomm PRIVATE ${legacy_dir})
    endif()
endforeach()

# 添加 src/framework/ 子目录
add_subdirectory(common)
add_subdirectory(communicator)
add_subdirectory(hcom)
add_subdirectory(op_base)
add_subdirectory(group)
add_subdirectory(cluster_maintenance)
add_subdirectory(nslbdp)
add_subdirectory(next)

# 添加 src/common/ 子目录
add_subdirectory(${HCOMM_ROOT_DIR}/src/common/error_code)
add_subdirectory(${HCOMM_ROOT_DIR}/src/common/debug/config)
