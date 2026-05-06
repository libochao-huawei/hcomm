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
    ${HCOMM_DIR}/include/hccl
    ${HCOMM_DIR}/src/algorithm/base/inc
    ${HCOMM_DIR}/src/algorithm/pub_inc
    ${HCOMM_DIR}/src/pub_inc
    ${HCOMM_DIR}/src/pub_inc/inner
    ${HCOMM_DIR}/src/pub_inc/new
    ${HCOMM_DIR}/src/algorithm/base/alg_aiv_template
    ${HCOMM_DIR}/src/algorithm/base/alg_template
    ${HCOMM_DIR}/src/algorithm/base/mc2_handler
    ${HCOMM_DIR}/src/algorithm/base/alg_template/component
    ${HCOMM_DIR}/src/algorithm/base/communicator
    ${HCOMM_DIR}/src/algorithm/base/communicator/legacy
    ${HCOMM_DIR}/src/algorithm/impl
    ${HCOMM_DIR}/src/algorithm/impl/legacy
    ${HCOMM_DIR}/src/algorithm/impl/inc
    ${HCOMM_DIR}/src/algorithm/impl/task
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager
    ${HCOMM_DIR}/src/algorithm/impl/coll_executor

    ${HCOMM_DIR}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/recovery/operator_retry/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/snapshot/
    ${HCOMM_DIR}/src/framework/cluster_maintenance/aclgraph/
    ${HCOMM_DIR}/src/framework/common/src/
    ${HCOMM_DIR}/src/framework/common/src/host
    ${HCOMM_DIR}/src/framework/common/src/config
    ${HCOMM_DIR}/src/framework/common/src/topo
    ${HCOMM_DIR}/src/framework/common/src/task
    ${HCOMM_DIR}/src/framework/common/src/exception
    ${HCOMM_DIR}/src/framework/common/src/h2d_tlv
    ${HCOMM_DIR}/src/framework/common/src/thread
    ${HCOMM_DIR}/src/framework/common/src/h2d_dto
    ${HCOMM_DIR}/src/framework/common/src/onesided_memory_management
    ${HCOMM_DIR}/src/framework/common/src/mgr
    ${HCOMM_DIR}/src/framework/communicator/impl/
    ${HCOMM_DIR}/src/framework/communicator/impl/resource_manager
    ${HCOMM_DIR}/src/framework/communicator/impl/one_sided_service
    ${HCOMM_DIR}/src/framework/communicator/impl/zero_copy
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/resource/engine
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel
    ${HCOMM_DIR}/src/framework/communicator/impl/independent_op/channel/device

    ${HCOMM_DIR}/src/framework/hcom/
    ${HCOMM_DIR}/src/framework/hcom/gradient_segment
    ${HCOMM_DIR}/src/framework/inc/
    ${HCOMM_DIR}/src/framework/
    ${HCOMM_DIR}/src/framework/nslbdp/
    ${HCOMM_DIR}/src/framework/op_base/src
    ${HCOMM_DIR}/src/framework/group

    ${HCOMM_DIR}/src/common/health/
    ${HCOMM_DIR}/src/common/stream/
    ${HCOMM_DIR}/src/common/launch_device/

    ${HCOMM_DIR}/src/platform/resource/socket
    ${HCOMM_DIR}/src/platform/inc
    ${HCOMM_DIR}/src/platform/inc/adapter

    ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads
    ${HCOMM_DIR}/src/framework/next/comms/comm_engine_res/threads/slaves/
    ${HCOMM_DIR}/src/framework/next/coll_comms/
    ${HCOMM_DIR}/src/framework/next/coll_comms/communicator
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank/comm_mems/
    ${HCOMM_DIR}/src/framework/next/coll_comms/rank_pairs
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling/aicpu
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/profiling/host
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/taskException
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/taskException/aicpu
    ${HCOMM_DIR}/src/framework/next/coll_comms/dfx/taskException/host
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels/aicpu/
    ${HCOMM_DIR}/src/framework/next/comms/endpoint_pairs/channels/aicpu/device/
    ${HCOMM_DIR}/src/framework/next/comms/endpoints/server_socket/

    # ccu 头文件
    ${HCOMM_DIR}/src/framework/next/comms/adpt

    ${HCOMM_DIR}/src/framework/next/comms/ccu/pub_inc/
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_pfe
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_channel_ctx_v1

    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_transport/
    ${HCOMM_DIR}/src/framework/next/comms/ccu/ccu_kernel
    ${HCOMM_DIR}/src/framework/next/comms/common/device/
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

# 添加 src/legacy 目录下的头文件搜索路径
FILE(GLOB_RECURSE LEGACY_SOURCE_LIST LIST_DIRECTORIES TRUE ${HCOMM_DIR}/src/legacy/*)
foreach(legacy_dir ${LEGACY_SOURCE_LIST})
    IF (IS_DIRECTORY ${legacy_dir} AND NOT ${legacy_dir} MATCHES "src/legacy/mdpi")
        target_include_directories(hcomm PRIVATE ${legacy_dir})
    endif()
endforeach()

# 添加 src/legacy 子目录
add_subdirectory(${HCOMM_DIR}/src/legacy)
