# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 ccl_kernel_plf 动态链接库，在 device 侧使用
add_library(ccl_kernel_plf SHARED)
# 定义 ccl_kernel_plf 静态链接库，在 device 侧使用
add_library(ccl_kernel_plf_a STATIC)

# 预处理宏定义
set(CCL_KERNEL_PLF_COMPILE_DEFINITIONS
    HCCD
    CCL_KERNEL
    CCL_KERNEL_AICPU
    USE_AICORE_REDUCESUM
    USE_AICORE_GATHERV2
    USE_AICORE_GATHERV2_INFER
)
target_compile_definitions(ccl_kernel_plf PRIVATE ${CCL_KERNEL_PLF_COMPILE_DEFINITIONS})
target_compile_definitions(ccl_kernel_plf_a PRIVATE ${CCL_KERNEL_PLF_COMPILE_DEFINITIONS})

# 编译选项
set(CCL_KERNEL_PLF_COMPILE_OPTIONS
    -Werror
    -Wall
    -fno-common
    -fno-strict-aliasing
    -pipe
    -std=c++14
    -D_FORTIFY_SOURCE=2 -O2
    -fstack-protector-all
)
target_compile_options(ccl_kernel_plf PRIVATE
    ${CCL_KERNEL_PLF_COMPILE_OPTIONS}
)
# ccl_kernel_plf_a 额外编译选项
target_compile_options(ccl_kernel_plf_a PRIVATE
    ${CCL_KERNEL_PLF_COMPILE_OPTIONS}
    -ftrapv
    -fPIC
)

# 链接选项
set(CCL_KERNEL_PLF_LINK_OPTIONS
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
)
target_link_options(ccl_kernel_plf PRIVATE
    ${CCL_KERNEL_PLF_LINK_OPTIONS}
    -s
)
target_link_options(ccl_kernel_plf_a PRIVATE
    ${CCL_KERNEL_PLF_LINK_OPTIONS}
)

# 头文件搜索路径
set(CCL_KERNEL_PLF_INCLUDE_LIST
    ${HCOMM_DIR}/include
    ${HCOMM_DIR}/include/hccl
    ${HCOMM_DIR}/pkg_inc
    ${HCOMM_DIR}/pkg_inc/hccl

    ${HCOMM_DIR}/src/pub_inc
    ${HCOMM_DIR}/src/pub_inc/aicpu/
    ${HCOMM_DIR}/src/pub_inc/inner
    ${HCOMM_DIR}/src/pub_inc/new

    ${HCOMM_DIR}/src/algorithm/base/inc
    ${HCOMM_DIR}/src/algorithm/pub_inc
    ${HCOMM_DIR}/src/algorithm/base/alg_aiv_template
    ${HCOMM_DIR}/src/algorithm/base/alg_template
    ${HCOMM_DIR}/src/algorithm/base/alg_template/component
    ${HCOMM_DIR}/src/algorithm/base/mc2_handler
    ${HCOMM_DIR}/src/algorithm/base/communicator
    ${HCOMM_DIR}/src/algorithm/impl
    ${HCOMM_DIR}/src/algorithm/impl/inc
    ${HCOMM_DIR}/src/algorithm/impl/legacy
    ${HCOMM_DIR}/src/algorithm/impl/legacy/operator
    ${HCOMM_DIR}/src/algorithm/base/communicator/legacy
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager
    ${HCOMM_DIR}/src/algorithm/impl/task
    ${HCOMM_DIR}/src/algorithm/impl/operator
    ${HCOMM_DIR}/src/algorithm/impl/operator/registry
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/registry
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_send_receive
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter/310P
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_all_to_all
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor/coll_scatter
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_gather
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_all_reduce
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoall
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_alltoallv
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_broadcast
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_reduce_scatter
    ${HCOMM_DIR}/src/algorithm/base/alg_template/temp_scatter

    ${HCOMM_DIR}/src/common/health
    ${HCOMM_DIR}/src/common/debug/profiling/inc/aicpu
    ${HCOMM_DIR}/src/common/debug/profiling/inc
    ${HCOMM_DIR}/src/common/debug/config/
    ${HCOMM_DIR}/src/common/stream
    ${HCOMM_DIR}/src/common/launch_device

    ${HCOMM_DIR}/src/framework/inc
    ${HCOMM_DIR}/src/framework/inc/hccd
    ${HCOMM_DIR}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/recovery/operator_retry
    ${HCOMM_DIR}/src/framework/
    ${HCOMM_DIR}/src/framework/common/src/
    ${HCOMM_DIR}/src/framework/common/src/exception
    ${HCOMM_DIR}/src/framework/common/src/config
    ${HCOMM_DIR}/src/framework/common/src/task
    ${HCOMM_DIR}/src/framework/common/src/topo
    ${HCOMM_DIR}/src/framework/common/src/mgr
    ${HCOMM_DIR}/src/framework/common/src/aicpu
    ${HCOMM_DIR}/src/framework/hcom
    ${HCOMM_DIR}/src/framework/communicator/impl/
    ${HCOMM_DIR}/src/framework/communicator/impl/one_sided_service/
    ${HCOMM_DIR}/src/framework/communicator/impl/zero_copy
    ${HCOMM_DIR}/src/framework/communicator/impl/resource_manager
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/resource/engine
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel/device
    ${HCOMM_DIR}/src/framework/op_base/src/
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank
    ${HCOMM_DIR}/src/framework/next/coll_comms/communicator
    ${HCOMM_DIR}/src/framework/next/coll_comms/communicator/aicpu
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank_pairs
    ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/sockets/
    ${HCOMM_DIR}/src/framework/next/comms/endpoints/
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/

    ${HCOMM_DIR}/src/platform/
    ${HCOMM_DIR}/src/platform/inc
    ${HCOMM_DIR}/src/platform/inc/adapter
    ${HCOMM_DIR}/src/platform/inc/adapter/hccd
    ${HCOMM_DIR}/src/platform/common/
    ${HCOMM_DIR}/src/platform/common/unique
    ${HCOMM_DIR}/src/platform/common/buffer_manager
    ${HCOMM_DIR}/src/platform/common/unfold_cache
    ${HCOMM_DIR}/src/platform/common/async_unfold_cache
    ${HCOMM_DIR}/src/platform/hccp
    ${HCOMM_DIR}/src/platform/hccp/inc
    ${HCOMM_DIR}/src/platform/hccp/inc/network
    ${HCOMM_DIR}/src/platform/resource/dispatcher_ctx
    ${HCOMM_DIR}/src/platform/resource/transport
    ${HCOMM_DIR}/src/platform/resource/transport/device
    ${HCOMM_DIR}/src/platform/resource/transport/host
    ${HCOMM_DIR}/src/platform/resource/transport/heterog
    ${HCOMM_DIR}/src/platform/resource/transport/onesided
    ${HCOMM_DIR}/src/platform/resource/transport/onesided/device
    ${HCOMM_DIR}/src/platform/resource/notify
    ${HCOMM_DIR}/src/platform/resource/socket
    ${HCOMM_DIR}/src/platform/ping_mesh
    ${HCOMM_DIR}/src/platform/task
    ${HCOMM_DIR}/src/platform/typical

    ${HCOMM_DIR}/src/legacy/unified_platform/resource/transport
    ${HCOMM_DIR}/src/legacy/common/utils/
    ${HCOMM_DIR}/src/legacy/common/exception/
    ${HCOMM_DIR}/src/legacy/common/types/
    ${HCOMM_DIR}/src/legacy/common/
    ${HCOMM_DIR}/src/legacy/framework/resource_manager/socket/
    ${HCOMM_DIR}/src/legacy/unified_platform/external_system/
    ${HCOMM_DIR}/src/legacy/unified_platform/common/

    ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/include/legacy
)
target_include_directories(ccl_kernel_plf PRIVATE
    ${CCL_KERNEL_PLF_INCLUDE_LIST}
)
target_include_directories(ccl_kernel_plf_a PRIVATE
    ${CCL_KERNEL_PLF_INCLUDE_LIST}
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(ccl_kernel_plf PRIVATE
        OPEN_BUILD_PROJECT
    )
    target_compile_definitions(ccl_kernel_plf_a PRIVATE 
        OPEN_BUILD_PROJECT
    )

    set(CCL_KERNEL_PLF_OPEN_INCLUDE_LIST
        # 临时依赖头文件，待删除
        ${HCOMM_DIR}/externel_depends/tsch/
        # 三方件头文件
        ${RDMA_CORE_INCLUDE_DIR}
        ${THIRD_PARTY_NLOHMANN_PATH}
    )
    target_include_directories(ccl_kernel_plf PRIVATE
        ${CCL_KERNEL_PLF_OPEN_INCLUDE_LIST}
    )
    target_include_directories(ccl_kernel_plf_a PRIVATE
        ${CCL_KERNEL_PLF_OPEN_INCLUDE_LIST}
    )

    # 链接库
    set(CCL_KERNEL_PLF_LINK_LIBS
        $<BUILD_INTERFACE:ascend_hal_headers>
        $<BUILD_INTERFACE:atrace_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:runtime_headers>
        $<BUILD_INTERFACE:slog_headers>
        -Wl,--no-as-needed
        c_sec
        aicpu_sharder
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
    target_link_libraries(ccl_kernel_plf
        ${CCL_KERNEL_PLF_LINK_LIBS}
    )
    target_link_libraries(ccl_kernel_plf_a PRIVATE
        ${CCL_KERNEL_PLF_LINK_LIBS}
    )
else()
    set(CCL_KERNEL_PLF_NON_OPEN_INCLUDE_LIST
        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/aicpu/tsd
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/include/external/acl
        ${TOP_DIR}/runtime/include/driver
        ${TOP_DIR}/runtime/pkg_inc
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/runtime/pkg_inc/profiling
        ${TOP_DIR}/runtime/pkg_inc/trace
        ${TOP_DIR}/runtime/pkg_inc/base
        ${TOP_DIR}/runtime/pkg_inc/aicpu_sched
        ${TOP_DIR}/inc/external
        ${TOP_DIR}/inc/aicpu/
        ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/abl/qos/qosmng
        ${TOP_DIR}/runtime/src/dfx/adump/inc/adump
        ${TOP_DIR}/inc/aicpu/common
        ${TOP_DIR}/ace/csruntime/inc/tsch
        ${TOP_DIR}/metadef
        ${TOP_DIR}/metadef/inc
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/metadef/inc/common
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc
    )
    target_include_directories(ccl_kernel_plf PRIVATE
        ${CCL_KERNEL_PLF_NON_OPEN_INCLUDE_LIST}
    )
    target_include_directories(ccl_kernel_plf_a PRIVATE
        ${CCL_KERNEL_PLF_NON_OPEN_INCLUDE_LIST}
    )

    # 链接库
    set(CCL_KERNEL_PLF_LINK_LIBS
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
        aicpu_sharder
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )
    target_link_libraries(ccl_kernel_plf
        ${CCL_KERNEL_PLF_LINK_LIBS}
    )
    target_link_libraries(ccl_kernel_plf_a
        ${CCL_KERNEL_PLF_LINK_LIBS}
    )

    # 安装
    install(TARGETS ccl_kernel_plf
        LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
    install(TARGETS ccl_kernel_plf_a
        ARCHIVE DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
endif()

# 设置依赖
add_dependencies(ccl_kernel_plf json)

# 设置 ccl_kernel_plf_a 输出文件名
set_target_properties(ccl_kernel_plf_a
    PROPERTIES
    OUTPUT_NAME ccl_kernel_plf
)
