# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 ccl_kernel 动态链接库，在 device 侧使用
add_library(ccl_kernel SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator/impl/independent_op/hccl_independent_rank_graph.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator/impl/one_sided_service/i_hccl_one_sided_service.cc
)

# 添加子目录
add_subdirectory(device)
add_subdirectory(${HCOMM_ROOT_DIR}/src/algorithm/base)
add_subdirectory(${HCOMM_ROOT_DIR}/src/algorithm/impl)

# 预处理宏定义
target_compile_definitions(ccl_kernel PRIVATE
    HCCD
    CCL_KERNEL_AICPU
)

# 编译选项
set(CCL_KERNEL_COMPILE_OPTIONS
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
target_compile_options(ccl_kernel PRIVATE ${CCL_KERNEL_COMPILE_OPTIONS})

# 链接选项
set(CCL_KERNEL_LINK_OPTIONS
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -s
)
target_link_options(ccl_kernel PRIVATE ${CCL_KERNEL_LINK_OPTIONS})

set(CCL_KERNEL_INCLUDE_LIST
    ${HCOMM_ROOT_DIR}/include/hccl
    ${HCOMM_ROOT_DIR}/pkg_inc
    ${HCOMM_ROOT_DIR}/pkg_inc/hccl

    ${HCOMM_ROOT_DIR}/src/pub_inc
    ${HCOMM_ROOT_DIR}/src/pub_inc/inner
    ${HCOMM_ROOT_DIR}/src/pub_inc/aicpu
    ${HCOMM_ROOT_DIR}/src/pub_inc/new

    ${HCOMM_ROOT_DIR}/src/platform/
    ${HCOMM_ROOT_DIR}/src/platform/inc/
    ${HCOMM_ROOT_DIR}/src/platform/inc/adapter
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/heterog/
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/host/
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/
    ${HCOMM_ROOT_DIR}/src/platform/resource/notify/
    ${HCOMM_ROOT_DIR}/src/platform/resource/dispatcher_ctx
    ${HCOMM_ROOT_DIR}/src/platform/task
    ${HCOMM_ROOT_DIR}/src/platform/common
    ${HCOMM_ROOT_DIR}/src/platform/common/unique
    ${HCOMM_ROOT_DIR}/src/platform/common/unfold_cache

    ${HCOMM_ROOT_DIR}/src/platform/hccp/inc
    ${HCOMM_ROOT_DIR}/src/platform/hccp/inc/network

    ${HCOMM_ROOT_DIR}/src/framework
    ${HCOMM_ROOT_DIR}/src/framework/common
    ${HCOMM_ROOT_DIR}/src/framework/common/src
    ${HCOMM_ROOT_DIR}/src/framework/common/src/mgr
    ${HCOMM_ROOT_DIR}/src/framework/communicator
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/resource/engine

    ${HCOMM_ROOT_DIR}/src/algorithm/base/inc
    ${HCOMM_ROOT_DIR}/src/algorithm/pub_inc
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_aiv_template
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/component
    ${HCOMM_ROOT_DIR}/src/algorithm/base/mc2_handler
    ${HCOMM_ROOT_DIR}/src/algorithm/base/communicator
    ${HCOMM_ROOT_DIR}/src/algorithm/impl
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/inc
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/legacy
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/legacy/operator
    ${HCOMM_ROOT_DIR}/src/algorithm/base/communicator/legacy
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/resource_manager
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/task
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/operator
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/operator/registry
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/registry
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_send_receive
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_all_gather
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all
    ${HCOMM_ROOT_DIR}/src/algorithm/impl/coll_executor/coll_scatter

    ${HCOMM_ROOT_DIR}/src/common/health
    ${HCOMM_ROOT_DIR}/src/common/debug/profiling/inc
    ${HCOMM_ROOT_DIR}/src/common/debug/profiling/inc/aicpu
    ${HCOMM_ROOT_DIR}/src/common/debug/config/
    ${HCOMM_ROOT_DIR}/src/common/debug/
    ${HCOMM_ROOT_DIR}/src/common/stream
    ${HCOMM_ROOT_DIR}/src/common/launch_device

    ${HCOMM_ROOT_DIR}/src/framework/inc
    ${HCOMM_ROOT_DIR}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCOMM_ROOT_DIR}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCOMM_ROOT_DIR}/src/framework/cluster_maintenance/snapshot/
    ${HCOMM_ROOT_DIR}/src/framework/common/src
    ${HCOMM_ROOT_DIR}/src/framework/common/src/aicpu
    ${HCOMM_ROOT_DIR}/src/framework/common/src/config
    ${HCOMM_ROOT_DIR}/src/framework/common/src/task
    ${HCOMM_ROOT_DIR}/src/framework/common/src/topo
    ${HCOMM_ROOT_DIR}/src/framework/hcom
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/one_sided_service/
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/zero_copy
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/resource_manager
    ${HCOMM_ROOT_DIR}/src/framework/op_base/src/
    ${HCOMM_ROOT_DIR}/src/framework/device/
    ${HCOMM_ROOT_DIR}/src/framework/device/aicpu_kfc
    ${HCOMM_ROOT_DIR}/src/framework/device/aicpu_kfc/inc
    ${HCOMM_ROOT_DIR}/src/framework/device/aicpu_kfc/dfx
    ${HCOMM_ROOT_DIR}/src/framework/device/common
    ${HCOMM_ROOT_DIR}/src/framework/device/inc
    ${HCOMM_ROOT_DIR}/src/framework/device/framework
    ${HCOMM_ROOT_DIR}/src/framework/cluster_maintenance/recovery/operator_retry
    ${HCOMM_ROOT_DIR}/src/framework/common/src/exception
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/resource
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/thread
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/channel
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/channel/device
    ${HCOMM_ROOT_DIR}/src/framework/communicator/impl/independent_op/data_api

    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_all_gather
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_all_reduce
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_alltoall
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_alltoallv
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_broadcast
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_reduce
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template/temp_scatter
    ${HCOMM_ROOT_DIR}/src/algorithm/base/alg_template
    ${HCOMM_ROOT_DIR}/src/algorithm/pub_inc

    ${HCOMM_ROOT_DIR}/src/platform/
    ${HCOMM_ROOT_DIR}/src/platform/inc/
    ${HCOMM_ROOT_DIR}/src/platform/inc/adapter
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/heterog/
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/host/
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/
    ${HCOMM_ROOT_DIR}/src/platform/resource/notify/
    ${HCOMM_ROOT_DIR}/src/platform/task
    ${HCOMM_ROOT_DIR}/src/platform/common
    ${HCOMM_ROOT_DIR}/src/platform/common/unique
    ${HCOMM_ROOT_DIR}/src/platform/common/unfold_cache

    ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径

    ${RDMA_CORE_INCLUDE_DIR}
    ${HCOMM_ROOT_DIR}/src/platform/
    ${HCOMM_ROOT_DIR}/src/platform/inc/
    ${HCOMM_ROOT_DIR}/src/platform/inc/adapter
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/heterog/
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/host/
    ${HCOMM_ROOT_DIR}/src/platform/resource/transport/
    ${HCOMM_ROOT_DIR}/src/platform/resource/notify/
    ${HCOMM_ROOT_DIR}/src/platform/resource/dispatcher_ctx
    ${HCOMM_ROOT_DIR}/src/platform/task
    ${HCOMM_ROOT_DIR}/src/platform/common
    ${HCOMM_ROOT_DIR}/src/platform/common/unique
    ${hccl_include_list}
    ${HCOMM_ROOT_DIR}/include
    ${HCOMM_ROOT_DIR}/include/hccl
    ${HCOMM_ROOT_DIR}/inc/hccl/hccl
    ${HCOMM_ROOT_DIR}/src/pub_inc
    ${HCOMM_ROOT_DIR}/src/pub_inc/aicpu

    ${HCOMM_ROOT_DIR}/src/framework/device/
    ${HCOMM_ROOT_DIR}/src/framework/device/common
    ${HCOMM_ROOT_DIR}/src/framework/device/inc
    ${HCOMM_ROOT_DIR}/src/framework/device/framework

    # runtime头文件
    ${ASCEND_CANN_PACKAGE_PATH}/include/

    # mmpa头文件
    ${ASCEND_CANN_PACKAGE_PATH}/include/acl/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/

    # 包间接口
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/common/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/runtime
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/

    # driver头文件
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver

    # ascendc
    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/

    ${HCOMM_ROOT_DIR}/externel_depends/tsch/
    ${RDMA_CORE_INCLUDE_DIR}
    ${THIRD_PARTY_NLOHMANN_PATH}

    ${HCOMM_ROOT_DIR}/src/platform/hccp/inc/network/
    ${HCOMM_ROOT_DIR}/src/platform/hccp/inc/
    ${HCOMM_ROOT_DIR}/src/framework/next/comms/endpoint_pairs/sockets/
    ${HCOMM_ROOT_DIR}/src/framework/next/comms/endpoints/
    ${HCOMM_ROOT_DIR}/src/framework/next/comms/comm_engine_res/threads
    ${HCOMM_ROOT_DIR}/src/framework/next/comms/comm_engine_res/threads/slaves/
    ${HCOMM_ROOT_DIR}/src/framework/next/coll_comms/communicator/aicpu
    ${HCOMM_ROOT_DIR}/src/framework/next/coll_comms/dfx
)

target_include_directories(ccl_kernel PRIVATE
    ${CCL_KERNEL_INCLUDE_LIST}
    ${ORION_HEAD_LIST}
)

if(NOT BUILD_OPEN_PROJECT)
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
        mmpa
        ccl_kernel_plf      # 链接 ccl_kernel_plf 动态库
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )

    install(TARGETS ccl_kernel
        LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    )
endif()

if(BUILD_OPEN_PROJECT)
    # 链接ascendalog需要LOG_CPP，链接slog不需要
    target_compile_definitions(ccl_kernel PRIVATE
        OPEN_BUILD_PROJECT
        _GLIBCXX_USE_CXX11_ABI=1
    )
    if(FULL_MODE)
        set(CCL_KERNEL_PLF_PATH ccl_kernel_plf)
    else()
        set(CCL_KERNEL_PLF_PATH
            -Wl,--whole-archive
            ccl_kernel_plf_a
            -Wl,--no-whole-archive
            -Wl,-Bsymbolic
        )
    endif()
    target_link_libraries(ccl_kernel PRIVATE
        -Wl,--no-as-needed
        ascend_hal
        c_sec
        mmpa
        ${CCL_KERNEL_PLF_PATH}
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
endif()

# 解析 ccl_kernel.ini
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
    COMMAND ${HI_PYTHON} ${HCOMM_ROOT_DIR}/scripts/parser_ini.py
            ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/ccl_kernel.ini
            ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/ccl_kernel.ini
    COMMENT "Generating ccl_kernel.json"
    VERBATIM
)
add_custom_target(ccl_kernel_json
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ccl_kernel.json
    DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/config OPTIONAL
)

# 拷贝 ccl_kernel 到指定目录，供打包使用
set(CCL_KERNEL_TAR_DIR ${HCOMM_ROOT_DIR}/build_device/ccl_kernel_tar_pkg/aicpu_kernels_device)
add_custom_command(
    TARGET ccl_kernel
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CCL_KERNEL_TAR_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccl_kernel> ${CCL_KERNEL_TAR_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccl_kernel_plf> ${CCL_KERNEL_TAR_DIR}
    COMMAND chmod 750 ${CCL_KERNEL_TAR_DIR}/lib*
    COMMENT "Copying libccl_kernel_plf.so libccl_kernel.so to ${CCL_KERNEL_TAR_DIR}"
)

# 定义 aicpu_custom 动态链接库，在 device 侧使用
# 与 ccl_kernel 内容一致，但运行在 AICPU Custom 进程
# 与 ccl_kernel 区别在于，aicpu_custom 链接 ccl_kernel_plf_a 静态库，ccl_kernel 链接 ccl_kernel_plf 动态库
add_library(aicpu_custom SHARED)
add_library(aicpu_custom SHARED)
get_target_property(CCL_KERNEL_ALL_SOURCES ccl_kernel SOURCES)
if(CCL_KERNEL_ALL_SOURCES)
    list(REMOVE_DUPLICATES CCL_KERNEL_ALL_SOURCES)
    target_sources(aicpu_custom PRIVATE ${CCL_KERNEL_ALL_SOURCES})
endif()
target_compile_definitions(aicpu_custom PRIVATE
    HCCD
    CCL_KERNEL_AICPU
    OPEN_BUILD_PROJECT
    _GLIBCXX_USE_CXX11_ABI=1
)
target_compile_options(aicpu_custom PRIVATE
    ${CCL_KERNEL_COMPILE_OPTIONS}
)
target_link_options(aicpu_custom PRIVATE
    ${CCL_KERNEL_LINK_OPTIONS}
)
target_include_directories(aicpu_custom PRIVATE
    ${CCL_KERNEL_INCLUDE_LIST}
    ${ORION_HEAD_LIST}
)
target_link_directories(aicpu_custom PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/
)
target_link_libraries(aicpu_custom PRIVATE
    -Wl,--no-as-needed
    ascend_hal
    c_sec
    mmpa
    -Wl,--whole-archive
    ccl_kernel_plf_a        # 链接 ccl_kernel_plf 静态库
    -Wl,--no-whole-archive
    -Wl,--as-needed
    -lrt
    -ldl
    -lpthread
)

# 解析 libaicpu_custom.ini
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
    COMMAND ${HI_PYTHON} ${HCOMM_ROOT_DIR}/scripts/parser_ini.py
            ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/aicpu_custom.ini
            ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/device/framework/aicpu_custom.ini
    COMMENT "Generating libaicpu_custom.json"
    VERBATIM
)
add_custom_target(aicpu_custom_json
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
)
add_dependencies(aicpu_custom ccl_kernel ccl_kernel_plf_a aicpu_custom_json)

# 安装
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
    DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
)
install(TARGETS aicpu_custom
    LIBRARY DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
)
