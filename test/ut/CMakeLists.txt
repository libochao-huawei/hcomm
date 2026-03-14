# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set(HCCL_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
message(STATUS "${HCCL_CODE_ROOT}/src/platform/common/ *******************************************")
set(HCCL_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(UT_COMMON_INCLUDE_LIST
    ${HCCL_CODE_ROOT}
    ${HCCL_CODE_ROOT}/include
    ${HCCL_CODE_ROOT}/include/hccl
    ${HCCL_CODE_ROOT}/pkg_inc
    ${HCCL_CODE_ROOT}/pkg_inc/hccl
    ${HCCL_CODE_ROOT}/pkg_inc/hcomm/ccu

    # src/pub_inc 目录下头文件
    ${HCCL_CODE_ROOT}/src/pub_inc
    ${HCCL_CODE_ROOT}/src/pub_inc/inner
    ${HCCL_CODE_ROOT}/src/pub_inc/aicpu
    ${HCCL_CODE_ROOT}/src/pub_inc/new

    # src/hccd 目录下头文件
    ${HCCL_CODE_ROOT}/src/hccd

    # src/common 目录下头文件
    ${HCCL_CODE_ROOT}/src/common/health
    ${HCCL_CODE_ROOT}/src/common/stream
    ${HCCL_CODE_ROOT}/src/common/launch_device
    ${HCCL_CODE_ROOT}/src/common/src
    ${HCCL_CODE_ROOT}/src/common/src/host
    ${HCCL_CODE_ROOT}/src/common/src/topo
    ${HCCL_CODE_ROOT}/src/common/debug/config
    ${HCCL_CODE_ROOT}/src/common/debug/profiling
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc
    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc/host

    # src/framework 目录下头文件
    ${HCCL_CODE_ROOT}/src/framework/
    ${HCCL_CODE_ROOT}/src/framework/inc/
    ${HCCL_CODE_ROOT}/src/framework/inc/host
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/health/heartbeat/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/snapshot
    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/aclgraph
    ${HCCL_CODE_ROOT}/src/framework/common/src
    ${HCCL_CODE_ROOT}/src/framework/common/src/host
    ${HCCL_CODE_ROOT}/src/framework/common/src/task
    ${HCCL_CODE_ROOT}/src/framework/common/src/topo
    ${HCCL_CODE_ROOT}/src/framework/common/src/exception
    ${HCCL_CODE_ROOT}/src/framework/common/src/config
    ${HCCL_CODE_ROOT}/src/framework/common/src/onesided_memory_management
    ${HCCL_CODE_ROOT}/src/framework/common/src/order_launch
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/one_sided_service
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/channel/device
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/rank_graph
    ${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/engine
    ${HCCL_CODE_ROOT}/src/framework/hcom/
    ${HCCL_CODE_ROOT}/src/framework/hcom/gradient_segment
    ${HCCL_CODE_ROOT}/src/framework/op_base/src/
    ${HCCL_CODE_ROOT}/src/framework/group
    ${HCCL_CODE_ROOT}/src/framework/nslbdp/
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/api_c_adpt

    # src/platform 目录下头文件
    ${HCCL_CODE_ROOT}/src/platform/
    ${HCCL_CODE_ROOT}/src/platform/common/
    ${HCCL_CODE_ROOT}/src/platform/common/unique/
    ${HCCL_CODE_ROOT}/src/platform/common/buffer_manager
    ${HCCL_CODE_ROOT}/src/platform/common/unfold_cache
    ${HCCL_CODE_ROOT}/src/platform/ping_mesh/
    ${HCCL_CODE_ROOT}/src/platform/inc/
    ${HCCL_CODE_ROOT}/src/platform/inc/adapter
    ${HCCL_CODE_ROOT}/src/platform/inc/adapter/host
    ${HCCL_CODE_ROOT}/src/platform/resource/
    ${HCCL_CODE_ROOT}/src/platform/resource/mem
    ${HCCL_CODE_ROOT}/src/platform/resource/notify/
    ${HCCL_CODE_ROOT}/src/platform/resource/rma_buffer
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/device/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/host/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/heterog/
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided
    ${HCCL_CODE_ROOT}/src/platform/resource/transport/onesided/device
    ${HCCL_CODE_ROOT}/src/platform/resource/socket/
    ${HCCL_CODE_ROOT}/src/platform/resource/dispatcher_ctx
    ${HCCL_CODE_ROOT}/src/platform/task/
    ${HCCL_CODE_ROOT}/src/platform/aiv_communication/
    ${HCCL_CODE_ROOT}/src/platform/debug/
    ${HCCL_CODE_ROOT}/src/platform/debug/dfx/
    ${HCCL_CODE_ROOT}/src/platform/debug/dfx/trace/
    ${HCCL_CODE_ROOT}/src/platform/task/
    ${HCCL_CODE_ROOT}/src/platform/task/rtsq_interact/
    ${HCCL_CODE_ROOT}/src/platform/typical
    ${HCCL_CODE_ROOT}/src/platform/hccp/inc
    ${HCCL_CODE_ROOT}/src/platform/hccp/inc/network
    ${RDMA_CORE_INCLUDE_DIR}

    # hccl/algorithm 目录下头文件
    ${HCCL_CODE_ROOT}/src/algorithm/pub_inc
    ${HCCL_CODE_ROOT}/src/algorithm/base
    ${HCCL_CODE_ROOT}/src/algorithm/base/inc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoall
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_broadcast
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_gather
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/temp_send_recv
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/component
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/component/inc
    ${HCCL_CODE_ROOT}/src/algorithm/base/alg_template/inc_all_reduce_deter
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator
    ${HCCL_CODE_ROOT}/src/algorithm/base/mc2_handler
    ${HCCL_CODE_ROOT}/src/algorithm/impl
    ${HCCL_CODE_ROOT}/src/algorithm/impl/inc
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy
    ${HCCL_CODE_ROOT}/src/algorithm/impl/legacy/operator
    ${HCCL_CODE_ROOT}/src/algorithm/base/communicator/legacy
    ${HCCL_CODE_ROOT}/src/algorithm/impl/task
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator
    ${HCCL_CODE_ROOT}/src/algorithm/impl/operator/registry
    ${HCCL_CODE_ROOT}/src/algorithm/impl/resource_manager
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/registry
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_send_receive
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_to_all
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_all_gather_v/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor//coll_reduce_scatter/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_reduce
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_scatter
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast
    ${HCCL_CODE_ROOT}/src/algorithm/impl/coll_executor/coll_broadcast/310P

    ${HCCL_CODE_ROOT}/test/hccl_api_single_thread_test

    # hccl/operator目录下头文件
    ${HCCL_CODE_ROOT}/operator

    # proto.h头文件
    ${CMAKE_BINARY_DIR}/proto/hccl_llt

    # /src/framework/device/目录下头文件
    ${HCCL_CODE_ROOT}/src/framework/device/
    ${HCCL_CODE_ROOT}/src/framework/device/inc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/inc
    ${HCCL_CODE_ROOT}/src/framework/device/aicpu_kfc/common
    ${HCCL_CODE_ROOT}/src/framework/device/common
    ${HCCL_CODE_ROOT}/src/framework/device/utils
    ${HCCL_CODE_ROOT}/src/framework/device/framework
    ${HCCL_CODE_ROOT}/src/framework/next/comms/comm_engine_res/threads

    # framework/next
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/communicator
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/communicator/aicpu
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/rank/
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/rank_pairs/
    ${HCCL_CODE_ROOT}/src/framework/next/common/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoints/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/sockets/

    # legacy include
    ${HCCL_CODE_ROOT}/src/legacy/common/
    ${HCCL_CODE_ROOT}/src/legacy/common/utils/
    ${HCCL_CODE_ROOT}/src/legacy/common/exception/
    ${HCCL_CODE_ROOT}/src/legacy/common/types/
    ${HCCL_CODE_ROOT}/src/legacy/framework/resource_manager/socket/
    ${HCCL_CODE_ROOT}/src/legacy/framework/communicator/
    ${HCCL_CODE_ROOT}/src/legacy/framework/communicator/aicpu/
    ${HCCL_CODE_ROOT}/src/legacy/framework/communicator/aicpu/inc
    ${HCCL_CODE_ROOT}/src/legacy/framework/topo/
    ${HCCL_CODE_ROOT}/src/legacy/framework/topo/new_topo_builder/common/
    ${HCCL_CODE_ROOT}/src/legacy/framework/topo/new_topo_builder/rank_graph/
    ${HCCL_CODE_ROOT}/src/legacy/framework/topo/new_topo_builder/rank_table_info/
    ${HCCL_CODE_ROOT}/src/legacy/service/collective/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/external_system/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/socket/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/transport/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/transport/aicpu/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/notify/aicpu/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/stream/aicpu/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/buffer/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/buffer/aicpu/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/connection/aicpu/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/pub_inc/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/common/
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/

    # tbe头文件
    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/
    ${HCCL_CODE_ROOT}/src/platform/legacy/common/
    ${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/
    ${HCCL_CODE_ROOT}/src/platform/legacy/inc/

    # hcce头文件
    ${HCCL_CODE_ROOT}/src/engines/hcclengine/inc

    ${HCCL_CODE_ROOT}/test/ut/stub
    ${HCCL_CODE_ROOT}/test/ut/stub/legacy/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/aicpu/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/aicpu/common/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/runtime/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/profiling/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/base/
    ${HCCL_CODE_ROOT}/test/ut/depends/include/base/context_builder
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/dump/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/trace/
    ${HCCL_CODE_ROOT}/test/ut/depends/pkg_inc/asc/include/adv_api/hccl/internal

    ${HCCL_CODE_ROOT}/test/ut/depends/include/
    ${HCCL_CODE_ROOT}/test/ut/depends/runtime/include/
    ${HCCL_CODE_ROOT}/test/ut/depends/include/mmpa/
    ${HCCL_CODE_ROOT}/test/ut/depends/include/driver/
    ${HCCL_CODE_ROOT}/test/ut/depends/include/acl/
    ${HCCL_CODE_ROOT}/test/ut/depends/include/ascendc/highlevel_api/
    ${HCCL_CODE_ROOT}/test/ut/depends/include/hcomm-legacy/

    ${CANN_3RD_LIB_PATH}/json/include/
    ${CANN_3RD_LIB_PATH}/gtest/include/
    ${CANN_3RD_LIB_PATH}/mockcpp/include/
    ${CANN_3RD_LIB_PATH}/mockcpp/include/3rdparty/

    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal

    #next
    ${HCCL_CODE_ROOT}/src/framework/next/comms/comm_engine_res/threads
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/communicator
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/communicator/aicpu
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/rank
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/rank_pairs
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/channels
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/channels/aicpu/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/sockets/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoints/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/api_c_adpt/

    ${HCCL_CODE_ROOT}/src/legacy/interface/
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx/profiling
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx/profiling/aicpu
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx/profiling/host
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx/taskException
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx/taskException/aicpu
    ${HCCL_CODE_ROOT}/src/framework/next/coll_comms/dfx/taskException/host
    ${HCCL_CODE_ROOT}/src/framework/device/framework
    ${HCCL_CODE_ROOT}/src/framework/next/comms/endpoint_pairs/channels/slaves
    ${HCCL_CODE_ROOT}/src/framework/dfx/task_exception
    ${HCCL_CODE_ROOT}/src/legacy/framework/dfx/task_exception
    ${HCCL_CODE_ROOT}/src/legacy/framework/dfx/common
    ${HCCL_CODE_ROOT}/src/legacy/framework/dfx/profiling
    ${HCCL_CODE_ROOT}/src/legacy/framework/dfx/
    ${HCCL_CODE_ROOT}/src/legacy/framework/resource_manager/stream
    ${HCCL_CODE_ROOT}/src/legacy/unified_platform/resource/stream
    ${HCCL_CODE_ROOT}/src/legacy/interface/
    ${HCCL_CODE_ROOT}/src/legacy/framework/dfx/aicpu/profiling

    # ccu 头文件
    ${HCCL_CODE_ROOT}/src/framework/next/comms/adpt/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/common/
 
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/pub_inc/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/ccu_device/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/ccu_device/ccu_comp/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_pfe/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/ccu_device/ccu_comp/ccu_channel/ccu_channel_ctx_v1/
    ${HCCL_CODE_ROOT}/src/framework/next/comms/ccu/ccu_transport/
)

add_subdirectory(stub)
add_subdirectory(framework/next)
add_subdirectory(common/prepare_ut_env)
add_subdirectory(common/binary_package)
add_subdirectory(misc)
add_subdirectory(impl)
add_subdirectory(device)
add_subdirectory(platform/hcom)
add_subdirectory(platform/resource/dispatcher)
add_subdirectory(platform/resource/notify)
add_subdirectory(platform/resource/transport)
add_subdirectory(platform/ping_mesh)
add_subdirectory(framework/communicator)
add_subdirectory(framework/cluster_maintenance)
add_subdirectory(framework/op_base_api)
# add_subdirectory(framework/hcom) // 单个ut测试文件已经编译通过，但是耦合编译会出现段错误问题，暂不开启
add_subdirectory(framework/communicator/impl)
add_subdirectory(framework/resource)
add_subdirectory(framework/communicator/impl/independent_op/data_api)
add_subdirectory(aicpu_kfc/aicpu)
add_subdirectory(aicpu_kfc/misc)
add_subdirectory(inter/comm)
add_subdirectory(inter/hcom)
add_subdirectory(platform/hccp)
