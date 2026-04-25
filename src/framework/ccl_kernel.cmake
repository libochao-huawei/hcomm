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

set(CCL_KERNEL_INCLUDE_LIST
    ${HCCL_BASE_DIR}/../include/hccl
    ${HCCL_BASE_DIR}/pub_inc
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${HCCL_BASE_DIR}/algorithm/base/inc
    ${HCCL_BASE_DIR}/algorithm/pub_inc
    ${HCCL_BASE_DIR}/algorithm/base/alg_aiv_template
    ${HCCL_BASE_DIR}/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/component
    ${HCCL_BASE_DIR}/algorithm/base/mc2_handler
    ${HCCL_BASE_DIR}/algorithm/base/communicator
    ${HCCL_BASE_DIR}/algorithm/impl
    ${HCCL_BASE_DIR}/algorithm/impl/inc
    ${HCCL_BASE_DIR}/algorithm/impl/legacy
    ${HCCL_BASE_DIR}/algorithm/impl/legacy/operator
    ${HCCL_BASE_DIR}/algorithm/base/communicator/legacy
    ${HCCL_BASE_DIR}/algorithm/impl/resource_manager
    ${HCCL_BASE_DIR}/algorithm/impl/task
    ${HCCL_BASE_DIR}/algorithm/impl/operator
    ${HCCL_BASE_DIR}/algorithm/impl/operator/registry
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/registry
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_all_to_all
    ${HCCL_BASE_DIR}/algorithm/impl/coll_executor/coll_scatter
    ${HCCL_BASE_DIR}/common/health
    ${HCCL_BASE_DIR}/common/debug/profiling/inc
    ${HCCL_BASE_DIR}/common/debug/profiling/inc/aicpu
    ${HCCL_BASE_DIR}/common/debug/config/
    ${HCCL_BASE_DIR}/common/debug/
    ${HCCL_BASE_DIR}/common/stream
    ${HCCL_BASE_DIR}/common/launch_device
    ${HCCL_BASE_DIR}/framework/inc
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/snapshot/
    ${HCCL_BASE_DIR}/framework/common/src
    ${HCCL_BASE_DIR}/framework/common/src/aicpu
    ${HCCL_BASE_DIR}/framework/common/src/config
    ${HCCL_BASE_DIR}/framework/common/src/task
    ${HCCL_BASE_DIR}/framework/common/src/topo
    ${HCCL_BASE_DIR}/framework/hcom
    ${HCCL_BASE_DIR}/framework/communicator/impl/
    ${HCCL_BASE_DIR}/framework/communicator/impl/one_sided_service/
    ${HCCL_BASE_DIR}/framework/communicator/impl/zero_copy
    ${HCCL_BASE_DIR}/framework/communicator/impl/resource_manager
    ${HCCL_BASE_DIR}/framework/op_base/src/
    ${HCCL_BASE_DIR}/framework/device/
    ${HCCL_BASE_DIR}/framework/device/aicpu_kfc
    ${HCCL_BASE_DIR}/framework/device/aicpu_kfc/inc
    ${HCCL_BASE_DIR}/framework/device/aicpu_kfc/dfx
    ${HCCL_BASE_DIR}/framework/device/common
    ${HCCL_BASE_DIR}/framework/device/inc
    ${HCCL_BASE_DIR}/framework/device/framework
    ${HCCL_BASE_DIR}/framework/cluster_maintenance/recovery/operator_retry
    ${HCCL_BASE_DIR}/framework/common/src/exception
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/resource
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/thread
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/rank_graph
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/channel/device
    ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/data_api

    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_all_gather
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_alltoall
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_broadcast
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_reduce
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_BASE_DIR}/algorithm/base/alg_template/temp_scatter
    ${HCCL_BASE_DIR}/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/algorithm/pub_inc
    ${HCCL_BASE_DIR}/../pkg_inc/
    ${HCCL_BASE_DIR}/../pkg_inc/hccl/

    ${HCCL_BASE_DIR}/platform/
    ${HCCL_BASE_DIR}/platform/inc/
    ${HCCL_BASE_DIR}/platform/inc/adapter
    ${HCCL_BASE_DIR}/platform/resource/transport/heterog/
    ${HCCL_BASE_DIR}/platform/resource/transport/host/
    ${HCCL_BASE_DIR}/platform/resource/transport/
    ${HCCL_BASE_DIR}/platform/resource/notify/
    ${HCCL_BASE_DIR}/platform/task
    ${HCCL_BASE_DIR}/platform/common
    ${HCCL_BASE_DIR}/platform/common/unique
    ${HCCL_BASE_DIR}/platform/common/unfold_cache

    ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/ # c_sec、mmpa、unified_dlog动态库搜索路径

    ${RDMA_CORE_INCLUDE_DIR}
    ${HCCL_BASE_DIR}/platform/
    ${HCCL_BASE_DIR}/platform/inc/
    ${HCCL_BASE_DIR}/platform/inc/adapter
    ${HCCL_BASE_DIR}/platform/resource/transport/heterog/
    ${HCCL_BASE_DIR}/platform/resource/transport/host/
    ${HCCL_BASE_DIR}/platform/resource/transport/
    ${HCCL_BASE_DIR}/platform/resource/notify/
    ${HCCL_BASE_DIR}/platform/resource/dispatcher_ctx
    ${HCCL_BASE_DIR}/platform/task
    ${HCCL_BASE_DIR}/platform/common
    ${HCCL_BASE_DIR}/platform/common/unique
    ${hccl_include_list}
    ${HCCL_BASE_DIR}/../include
    ${HCCL_BASE_DIR}/../include/hccl
    ${HCCL_BASE_DIR}/../inc/hccl/hccl
    ${HCCL_BASE_DIR}/../pkg_inc/
    ${HCCL_BASE_DIR}/../pkg_inc/hccl/
    ${HCCL_BASE_DIR}/pub_inc
    ${HCCL_BASE_DIR}/pub_inc/aicpu

    ${HCCL_BASE_DIR}/framework/device/
    ${HCCL_BASE_DIR}/framework/device/common
    ${HCCL_BASE_DIR}/framework/device/inc
    ${HCCL_BASE_DIR}/framework/device/framework

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

    ${HCCL_BASE_DIR}/../externel_depends/tsch/
    ${RDMA_CORE_INCLUDE_DIR}
    ${THIRD_PARTY_NLOHMANN_PATH}

    ${HCCL_BASE_DIR}/platform/hccp/inc/network/
    ${HCCL_BASE_DIR}/platform/hccp/inc/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoint_pairs/sockets/
    ${HCCL_BASE_DIR}/framework/next/comms/endpoints/
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads
    ${HCCL_BASE_DIR}/framework/next/comms/comm_engine_res/threads/slaves/
    ${HCCL_BASE_DIR}/framework/next/coll_comms/communicator/aicpu
    ${HCCL_BASE_DIR}/framework/next/coll_comms/dfx
)

target_include_directories(ccl_kernel PRIVATE
    ${CCL_KERNEL_INCLUDE_LIST}
)

target_include_directories(ccl_kernel PRIVATE
    ${ORION_HEAD_LIST}
)

if(NOT BUILD_OPEN_PROJECT)
    target_include_directories(ccl_kernel PRIVATE
        ${HCCL_BASE_DIR}/../include/hccl
        ${HCCL_BASE_DIR}/pub_inc
        ${HCCL_BASE_DIR}/pub_inc/inner
        ${HCCL_BASE_DIR}/pub_inc/aicpu
        ${HCCL_BASE_DIR}/pub_inc/new

        ${HCCL_BASE_DIR}/hccl/hccl_comm/wrapper/notify/

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

        ${HCCL_BASE_DIR}/platform/
        ${HCCL_BASE_DIR}/platform/inc/
        ${HCCL_BASE_DIR}/platform/inc/adapter
        ${HCCL_BASE_DIR}/platform/resource/transport/heterog/
        ${HCCL_BASE_DIR}/platform/resource/transport/host/
        ${HCCL_BASE_DIR}/platform/resource/transport/
        ${HCCL_BASE_DIR}/platform/resource/notify/
        ${HCCL_BASE_DIR}/platform/resource/dispatcher_ctx
        ${HCCL_BASE_DIR}/platform/task
        ${HCCL_BASE_DIR}/platform/common
        ${HCCL_BASE_DIR}/platform/common/unique
        ${HCCL_BASE_DIR}/platform/common/unfold_cache

        ${HCCL_BASE_DIR}/platform/hccp/inc
        ${HCCL_BASE_DIR}/platform/hccp/inc/network
	      ${HCCL_BASE_DIR}/../pkg_inc/
        ${HCCL_BASE_DIR}/../pkg_inc/hccl/

        ${HCCL_BASE_DIR}/framework
        ${HCCL_BASE_DIR}/framework/common
        ${HCCL_BASE_DIR}/framework/common/src
        ${HCCL_BASE_DIR}/framework/common/src/mgr
        ${HCCL_BASE_DIR}/framework/communicator
        ${HCCL_BASE_DIR}/framework/communicator/impl/independent_op/resource/engine
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
        ccl_kernel_plf      # 链接 ccl_kernel_plf 动态库
        mmpa
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

set(CCL_KERNEL_TAR_DIR ${HCCL_BASE_DIR}/../build_device/ccl_kernel_tar_pkg/aicpu_kernels_device)
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
    -D_GLIBCXX_USE_CXX11_ABI=1
)
target_compile_options(aicpu_custom PRIVATE
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
target_link_options(aicpu_custom PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -s
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
add_dependencies(aicpu_custom ccl_kernel ccl_kernel_plf_a aicpu_custom_json)
install(TARGETS aicpu_custom
    LIBRARY DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
    DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
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
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
    DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
)
