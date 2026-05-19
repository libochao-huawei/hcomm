# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 ccl_kernel 动态链接库，在 device 侧使用
add_library(ccl_kernel SHARED)

# 预处理宏定义
target_compile_definitions(ccl_kernel PRIVATE
    HCCD
    CCL_KERNEL_AICPU
)

# 编译选项
target_compile_options(ccl_kernel PRIVATE
    -Werror
    -Wfloat-equal
    -Wall
    -fno-common
    -fstack-protector-strong
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
)

# 链接选项
target_link_options(ccl_kernel PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -s
)

# 头文件搜索路径
target_include_directories(ccl_kernel PRIVATE
    ${HCOMM_DIR}/include
    ${HCOMM_DIR}/include/hccl
    ${HCOMM_DIR}/pkg_inc
    ${HCOMM_DIR}/pkg_inc/hccl
    ${HCOMM_DIR}/pkg_inc/hcomm/ccu
    ${HCOMM_DIR}/src/pub_inc
    ${HCOMM_DIR}/src/pub_inc/aicpu
    ${HCOMM_DIR}/src/pub_inc/new
    ${HCOMM_DIR}/externel_depends/tsch

    # src/common 头文件
    ${HCOMM_DIR}/src/common/stream
    ${HCOMM_DIR}/src/common/debug/profiling/inc

    # src/legacy 头文件，优先于 src/framework
    ${HCOMM_DIR}/src/legacy/unified_platform/resource/socket
    ${HCOMM_DIR}/src/legacy/framework/env_config

    # src/framework 头文件
    ${HCOMM_DIR}/src/framework
    ${HCOMM_DIR}/src/framework/inc
    ${HCOMM_DIR}/src/framework/op_base/src
    ${HCOMM_DIR}/src/framework/cluster_maintenance/health/heartbeat
    ${HCOMM_DIR}/src/framework/cluster_maintenance/recovery/operator_retry
    ${HCOMM_DIR}/src/framework/common/src/exception
    ${HCOMM_DIR}/src/framework/communicator/impl
    ${HCOMM_DIR}/src/framework/communicator/impl/resource_manager
    ${HCOMM_DIR}/src/framework/next/comms/api_c_adpt
    ${HCOMM_DIR}/src/framework/next/comms/endpoints
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/sockets
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels
    ${HCOMM_DIR}/src/framework/next/comms/common/device
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device
    ${HCOMM_DIR}/src/framework/next/coll_comms
    ${HCOMM_DIR}/src/framework/next/coll_comms/communicator
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank_pairs
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling/aicpu

    # src/platform 头文件
    ${HCOMM_DIR}/src/platform/inc
    ${HCOMM_DIR}/src/platform/inc/adapter
    ${HCOMM_DIR}/src/platform/common
    ${HCOMM_DIR}/src/platform/common/buffer_manager
    ${HCOMM_DIR}/src/platform/common/unique
    ${HCOMM_DIR}/src/platform/common/unfold_cache
    ${HCOMM_DIR}/src/platform/resource/transport
    ${HCOMM_DIR}/src/platform/resource/transport/heterog
    ${HCOMM_DIR}/src/platform/resource/notify
    ${HCOMM_DIR}/src/platform/resource/dispatcher_ctx
    ${HCOMM_DIR}/src/platform/resource/socket
    ${HCOMM_DIR}/src/platform/task
    ${HCOMM_DIR}/src/platform/hccp/inc
    ${HCOMM_DIR}/src/platform/hccp/inc/network

    # src/algorithm 头文件
    ${HCOMM_DIR}/src/algorithm/pub_inc
    ${HCOMM_DIR}/src/algorithm/base/inc
    ${HCOMM_DIR}/src/algorithm/base/alg_template
    ${HCOMM_DIR}/src/algorithm/base/communicator
    ${HCOMM_DIR}/src/algorithm/base/communicator/legacy
    ${HCOMM_DIR}/src/algorithm/impl
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager
    ${HCOMM_DIR}/src/algorithm/impl/task
    ${HCOMM_DIR}/src/algorithm/impl/legacy
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor

    # src/legacy 头文件
    ${LEGACY_INCLUDE_LIST}

    # 三方件头文件
    ${RDMA_CORE_INCLUDE_DIR}
    ${THIRD_PARTY_NLOHMANN_PATH}
)

if(BUILD_OPEN_PROJECT)
    target_include_directories(ccl_kernel PRIVATE
        # ascendc
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/
        ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
    )

    target_compile_definitions(ccl_kernel PRIVATE
        OPEN_BUILD_PROJECT
        -D_GLIBCXX_USE_CXX11_ABI=1
    )

    target_link_libraries(ccl_kernel PRIVATE
        $<BUILD_INTERFACE:acl_rt_headers>
        $<BUILD_INTERFACE:ascend_hal_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:runtime_headers>
        -Wl,--no-as-needed
        c_sec
        mmpa
        ccl_kernel_plf
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
else()
    target_include_directories(ccl_kernel PRIVATE
        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/inc/aicpu/
        ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api
        ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl
        ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/include
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/include/external/acl
        ${TOP_DIR}/runtime/pkg_inc
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/runtime/pkg_inc/profiling
        ${TOP_DIR}/runtime/pkg_inc/trace
        ${TOP_DIR}/runtime/pkg_inc/base
        ${TOP_DIR}/runtime/pkg_inc/aicpu_sched
        ${TOP_DIR}/asc/asc-devkit
        ${TOP_DIR}/asc/asc-devkit/include/adv_api/hccl/internal
    )

    target_link_libraries(ccl_kernel PRIVATE
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:msprof_headers>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:hccl_headers>
        $<BUILD_INTERFACE:npu_runtime_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:kernel_tiling_headers>
        -Wl,--no-as-needed
        c_sec
        ccl_kernel_plf
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )

    install(TARGETS ccl_kernel
        LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
endif()

# 将 ccl_kernel.ini 转换为 json 格式
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
    COMMAND ${HI_PYTHON} ${HCOMM_DIR}/cmake/scripts/parser_ini.py
                         ${CMAKE_CURRENT_LIST_DIR}/device/framework/ccl_kernel.ini
                         ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT "Generating ccl_kernel.json"
 	VERBATIM
)
add_custom_target(ccl_kernel_json ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
)

# 安装
install(TARGETS ccl_kernel
    LIBRARY DESTINATION ${INSTALL_DEVICE_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
    DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/config ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
