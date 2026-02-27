# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 hccl_plf 动态链接库，在 host 侧使用
add_library(hccl_plf SHARED)

# 宏定义
target_compile_definitions(hccl_plf PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},device>:ASCEND_310P_DEVICE>
    USE_AICORE_REDUCESUM
    USE_AICORE_GATHERV2
    USE_AICORE_GATHERV2_INFER
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

# 编译选项
target_compile_options(hccl_plf PRIVATE
    -Werror
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
target_link_options(hccl_plf PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -Wl,--build-id=none
    $<$<CONFIG:Release>:-s>
)

# 头文件搜索路径
target_include_directories(hccl_plf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/adapter/host
    ${CMAKE_CURRENT_SOURCE_DIR}/task
    ${CMAKE_CURRENT_SOURCE_DIR}/common/
    ${CMAKE_CURRENT_SOURCE_DIR}/common/buffer_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/common/unfold_cache
    ${CMAKE_CURRENT_SOURCE_DIR}/common/async_unfold_cache
    ${CMAKE_CURRENT_SOURCE_DIR}/common/p2p_mgmt
    ${CMAKE_CURRENT_SOURCE_DIR}/common/unique
    ${CMAKE_CURRENT_SOURCE_DIR}/common/misc/network_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/common/misc/gradient_segment
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/transport/device
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/transport/host
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/transport/heterog
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/transport/onesided
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/transport/onesided/device
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/mem
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/dispatcher_ctx
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/notify
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/socket
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/stream
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/rma_buffer
    ${CMAKE_CURRENT_SOURCE_DIR}/resource/
    ${CMAKE_CURRENT_SOURCE_DIR}/remote_access
    ${CMAKE_CURRENT_SOURCE_DIR}/task
    ${CMAKE_CURRENT_SOURCE_DIR}/tbe_vector_reduce
    ${CMAKE_CURRENT_SOURCE_DIR}/aiv_communication
    ${CMAKE_CURRENT_SOURCE_DIR}/hccp/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/hccp/inc/network

    ${HCOMM_DIR}/include
    ${HCOMM_DIR}/include/hccl/
    ${HCOMM_DIR}/pkg_inc
    ${HCOMM_DIR}/pkg_inc/hccl

    ${HCOMM_DIR}/src/pub_inc
    ${HCOMM_DIR}/src/pub_inc/aicpu/
    ${HCOMM_DIR}/src/pub_inc/inner
    ${HCOMM_DIR}/src/pub_inc/new

    # legacy
    ${HCOMM_DIR}/src/legacy/unified_platform
    ${HCOMM_DIR}/src/legacy/common
    ${HCOMM_DIR}/src/legacy/common/utils
    ${HCOMM_DIR}/src/legacy/common/exception
    ${HCOMM_DIR}/src/legacy/unified_platform/resource/buffer/aicpu
    ${HCOMM_DIR}/src/legacy/unified_platform/external_system
    ${HCOMM_DIR}/src/legacy/common/types
    ${HCOMM_DIR}/src/legacy/unified_platform/common
    ${HCOMM_DIR}/src/legacy/unified_platform/resource/buffer
    ${HCOMM_DIR}/src/legacy/unified_platform/pub_inc
    ${HCOMM_DIR}/src/legacy/unified_platform/resource
    ${HCOMM_DIR}/src/legacy/framework/topo
    ${HCOMM_DIR}/src/legacy/framework/topo/new_topo_builder/common
    ${HCOMM_DIR}/src/legacy/framework/topo/new_topo_builder/rank_graph
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hccl_plf PRIVATE
        OPEN_BUILD_PROJECT
        LOG_CPP
    )

    target_include_directories(hccl_plf PRIVATE
        # 三方件头文件
        ${THIRD_PARTY_NLOHMANN_PATH}
        ${RDMA_CORE_INCLUDE_DIR}
        ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/include/legacy/
    )

    target_link_libraries(hccl_plf
    PRIVATE
        $<BUILD_INTERFACE:ascend_hal_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:runtime_headers>
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        mmpa
        runtime
        acl_rt
        error_manager
        hccl_legacy
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    PUBLIC
        hccl_headers
    )
else()
    target_include_directories(hccl_plf PRIVATE
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${TOP_DIR}/abl/msprof/inc/
        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/aicpu/
        ${TOP_DIR}/inc/driver/
        ${TOP_DIR}/metadef/pkg_inc/
        ${TOP_DIR}/metadef/inc/external/
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc
    )

    target_link_libraries(hccl_plf
    PRIVATE
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:hccl_headers>
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        mmpa
        runtime
        acl_rt
        error_manager
        hccl_legacy
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    PUBLIC
        hccl_headers
    )
endif()

# 设置依赖
add_dependencies(hccl_plf json)

# 安装
install(TARGETS hccl_plf
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
