# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if(BUILD_OPEN_PROJECT) #暂时隔离之前修改

	if(BUILD_OPEN_PROJECT)
	    set(package_include_list
	        ${ASCEND_CANN_PACKAGE_PATH}/include/hccl_alg_analyzer
	        ${ASCEND_CANN_PACKAGE_PATH}/include
			${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/hccl
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/metadef/common/util/error_manager
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/msprof
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/msprof/toolchain
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/runtime
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/runtime/external
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog/toolchain
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/mmpa
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/qos_manager
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/atrace/utrace
	        ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux/include/experiment/ascend_hal
	        ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux/include/experiment/ascend_hal/driver
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/platform
	        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/platform/metadef
	        ${ASCEND_CANN_PACKAGE_PATH}/x86_64-linux/include/experiment/datagw/aicpu
	        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include
	        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest/include
	        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/
	    )
	else()
	    set(package_include_list
	    )
	endif()

	if(NOT BUILD_OPEN_PROJECT)
		message(STATUS "${HCCL_CODE_ROOT}/src/platform/common/ +++++++++++++++++++++++++++++++++++++")
	    set(include_list
	        # 测试框架的代码
	        ${HCCL_ANALYZER_INC_DIR}/inc
	        ${HCCL_ANALYZER_INC_DIR}/checker
	        ${HCCL_ANALYZER_INC_DIR}/adapter_v1
	        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/transport
	        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/notify
	        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/stream

	        # studio工具的代码
	        ${HCCL_INSINGHT_INC_DIR}/cpp_engine/src

	        #用于本地跑llt
	        ${HCCL_ALG_ANALYZER_LIB_ROOT}/hccl_alg

	        # hccl inc目录
	        ${TOP_DIR}/ace/comop/inc
	        ${TOP_DIR}/ace/comop/inc/hccl
	        ${TOP_DIR}/ace/comop/inc/external
	        ${TOP_DIR}/ace/comop/inc/external/hccl
	        ${TOP_DIR}/ace/comop/hccl/whole/hccl/platform/inc

	        # inc目录下头文件
	        ${TOP_DIR}/metadef/inc/common/util/error_manager
	        ${TOP_DIR}/ace/npuruntime/inc
	        ${TOP_DIR}/abl/msprof/inc
	        ${TOP_DIR}/abl/mmpa/inc/mmpa

	        # 添加hccl目录
	        ${HCCL_CODE_ROOT}/hcomm/pub_inc

	        # slog头文件
	        ${TOP_DIR}/abl/slog/inc/toolchain
	        ${TOP_DIR}/abl/atrace/inc/utrace

	        ${TOP_DIR}/abl/mmpa/inc/mmpa
	    )
	else()
	    set(include_list
	    )
	endif()

	add_subdirectory(single_test/misc)
	add_subdirectory(mpi_test/misc) # 依赖MPI库
	add_subdirectory(mpi_test/comm) # 依赖MPI库
	add_subdirectory(platform_test/aicpu_test)
	add_subdirectory(mc2_test/misc)
	add_subdirectory(mc2_test/aicpu)
	add_subdirectory(single_test/algorithms/910B)
	add_subdirectory(single_test/algorithms/8pring)
	add_subdirectory(single_test/comm)
	add_subdirectory(single_test/coll)
	add_subdirectory(single_test/impl)
	add_subdirectory(single_test/executor/misc)
	add_subdirectory(single_test/executor/inter)
	add_subdirectory(single_test/hcom)
	add_subdirectory(single_test/ping_mesh)
	add_subdirectory(device)
else()
	set(HCCL_CODE_ROOT ${TOP_DIR}/hcomm/)
	message(STATUS "${HCCL_CODE_ROOT}/src/platform/common/ *******************************************")
	set(HCCL_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

	set(include_list
	    # inc目录下头文件
	    ${TOP_DIR}/inc
	    ${TOP_DIR}/ace/comop/inc
	    ${TOP_DIR}/hcomm/inc/hccl
		${TOP_DIR}/hcomm/inc/hccl/hccl
		${TOP_DIR}/hcomm/inc/pkg_inc
	    ${TOP_DIR}/graphengine/inc
	    ${TOP_DIR}/metadef/
	    ${TOP_DIR}/metadef/inc
		${TOP_DIR}/metadef/pkg_inc
	    ${TOP_DIR}/inc/common/util/error_manager
	    ${TOP_DIR}/metadef/inc/common/util/error_manager
	    ${TOP_DIR}/metadef/inc/common/util/trace_manager
	    ${TOP_DIR}/ace/npuruntime/runtime/platform/inc
	    ${TOP_DIR}/inc/register
	    ${TOP_DIR}/metadef/inc/register
	    ${TOP_DIR}/inc/graph
	    ${TOP_DIR}/metadef/inc/graph
	    ${TOP_DIR}/inc/common/opskernel
	    ${TOP_DIR}/metadef/inc/common/opskernel
	    ${TOP_DIR}/inc/framework
	    ${TOP_DIR}/graphengine/inc/framework
		${TOP_DIR}/runtime/include/external/
	    ${TOP_DIR}/inc/external
	    ${TOP_DIR}/metadef/inc/external
		${TOP_DIR}/metadef/pkg_inc
	    ${TOP_DIR}/graphengine/inc/external
	    ${TOP_DIR}/inc/external/graph
	    ${TOP_DIR}/metadef/inc/external/graph
	    ${TOP_DIR}/inc/common
	    ${TOP_DIR}/metadef/inc/common
	    ${TOP_DIR}/metadef/inc/common/util
	    ${TOP_DIR}/metadef/inc/exe_graph/lowering
	    ${TOP_DIR}/inc/aicpu/
	    ${TOP_DIR}/inc/aicpu/aicpu_schedule/aicpu_sharder
	    ${TOP_DIR}/inc/driver
	    ${TOP_DIR}/asl/hss/inc
	    ${TOP_DIR}/asl/hss/inc/pub
	    ${TOP_DIR}/asl/hss/common/hash_table
	    ${TOP_DIR}/abl/qos/qosmng
		${TOP_DIR}/air/inc/
		${TOP_DIR}/air/inc/external
		${TOP_DIR}/air/inc/graph_metadef
		${TOP_DIR}/air/inc/graph_metadef/external
		${TOP_DIR}/air/inc/graph_metadef/external/register
		${TOP_DIR}/air/inc/graph_metadef/graph/utils
		${TOP_DIR}/air/graph_metadef/
	    ${TOP_DIR}/metadef/
	    ${TOP_DIR}/metadef/inc/external/register
	    # libc_sec目录下的头文件
	    ${TOP_DIR}/abl/libc_sec/include
	    ${TOP_DIR}/abl/atrace/inc/utrace
	    ${TOP_DIR}/inc/aicpu/common
	    ${TOP_DIR}/ace/csruntime/inc/tsch
	    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api
	    ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl
		${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/include
		${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl/adv_api
		${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl/adv_api/detail

	    ${TOP_DIR}/hlt/hccl/third_party/mellanox/4.2/include
	    ${TOP_DIR}/open_source/json/include
	    ${TOP_DIR}/llt/third_party/googletest/include
	    ${TOP_DIR}/llt/third_party/googletest/include
	    ${TOP_DIR}/llt/third_party/mpi/include
	    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include
	    ${TOP_DIR}/llt/third_party/mockcpp/4.9/include/3rdparty

	    # # hccl/src目录下头文件
	    ${HCCL_CODE_ROOT}/
	    ${HCCL_CODE_ROOT}/src
	    ${HCCL_CODE_ROOT}/src/inc
	    ${HCCL_CODE_ROOT}/src/framework/hcomm/
	    ${HCCL_CODE_ROOT}/src/hccl/op_base/src
	    ${HCCL_CODE_ROOT}/src/common/src
	    ${HCCL_CODE_ROOT}/src/common/src/host
	    ${HCCL_CODE_ROOT}/src/common/src/topo
	    ${HCCL_CODE_ROOT}/src/hccd
	    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm
	    ${HCCL_CODE_ROOT}/src/algorithm/pub_inc
	    ${HCCL_CODE_ROOT}/src/algorithm/impl

	    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/inc
	    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/executor/component/inc
	    ${HCCL_CODE_ROOT}/src/hccl/hccl_comm/impl
	    ${HCCL_CODE_ROOT}/src/hdcs
	    ${HCCL_CODE_ROOT}/src/platform/
	    ${HCCL_CODE_ROOT}/src/platform/common/
	    ${HCCL_CODE_ROOT}/src/platform/common/unique/
	    ${HCCL_CODE_ROOT}/src/platform/common/buffer_manager
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

		${HCCL_CODE_ROOT}/inc/
		${HCCL_CODE_ROOT}/inc/hccl
		${HCCL_CODE_ROOT}/inc/hccl/hccl
		${HCCL_CODE_ROOT}/inc/pkg_inc
	    ${HCCL_CODE_ROOT}/src/pub_inc
	    ${HCCL_CODE_ROOT}/src/pub_inc/inner
	    ${HCCL_CODE_ROOT}/src/pub_inc/aicpu
	    ${HCCL_CODE_ROOT}/src/pub_inc/new
	    ${HCCL_CODE_ROOT}/src/platform/resource/dispatcher_ctx
	    ${HCCL_CODE_ROOT}/src/hccl_heterog/rpc/
	    ${HCCL_CODE_ROOT}/src/hccl_heterog/heterog_cpu/
	    ${HCCL_CODE_ROOT}/src/hccl_heterog/common/
		${HCCL_CODE_ROOT}/src/pkg_inc

	    ${HCCL_CODE_ROOT}/src/framework/
	    ${HCCL_CODE_ROOT}/src/framework/inc/
	    ${HCCL_CODE_ROOT}/src/framework/inc/host
	    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/health/heartbeat/
	    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/detect/detect_connect_anomalies/
	    ${HCCL_CODE_ROOT}/src/framework/cluster_maintenance/recovery/operator_retry
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
		${HCCL_CODE_ROOT}/src/framework/communicator/impl/independent_op/resource/device
	    ${HCCL_CODE_ROOT}/src/framework/hcom/
	    ${HCCL_CODE_ROOT}/src/framework/op_base/src/
	    ${HCCL_CODE_ROOT}/src/framework/nslbdp/
		${HCCL_OPEN_CODE_ROOT}/pkg_inc

	    # hcomm/common 目录下头文件
	    ${HCCL_CODE_ROOT}/src/common/health
	    ${HCCL_CODE_ROOT}/src/common/debug/profiling
	    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc
	    ${HCCL_CODE_ROOT}/src/common/debug/profiling/inc/host
	    ${HCCL_CODE_ROOT}/src/common/debug/config
	    ${HCCL_CODE_ROOT}/src/common/stream/
	    ${HCCL_CODE_ROOT}/src/common/launch_device/

	    # hcomm/algorithm 目录下头文件
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

		${HCCL_CODE_ROOT}/test/llt/stub

	    # hcomm/cltm目录下头文件
	    ${HCCL_CODE_ROOT}/cltm/src

	    # hcomm/operator目录下头文件
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
	    # dump工具头文件
		${TOP_DIR}/abl/adump/inc/
	    ${TOP_DIR}/abl/adump/inc/adump

	    # orion include
	    # ${HCCL_CODE_ROOT}/orion/common/utils
	    # ${HCCL_CODE_ROOT}/orion/common/types

	    # tbe头文件
	    ${HCCL_CODE_ROOT}/src/platform/legacy/hccl_tbe_task/
	    ${HCCL_CODE_ROOT}/src/platform/legacy/common/
		${HCCL_CODE_ROOT}/src/platform/legacy/graph_ctx_mgr/
	    ${HCCL_CODE_ROOT}/src/platform/legacy/inc/

		# hcce头文件
		${HCCL_CODE_ROOT}/src/engines/hcclengine/inc
	)

	add_subdirectory(prepare_st_env)
	add_subdirectory(single_test/misc)
	add_subdirectory(mpi_test/misc)
	add_subdirectory(mpi_test/comm)
	add_subdirectory(platform_test/aicpu_test)
	add_subdirectory(mc2_test/misc)
	add_subdirectory(mc2_test/aicpu)
	add_subdirectory(single_test/algorithms/910B)
	add_subdirectory(single_test/algorithms/8pring)
	add_subdirectory(single_test/comm)
	add_subdirectory(single_test/coll)
	add_subdirectory(single_test/impl)
	add_subdirectory(single_test/executor/misc)
	add_subdirectory(single_test/executor/inter)
	add_subdirectory(single_test/hcom)
	add_subdirectory(single_test/ping_mesh)
	add_subdirectory(binary_package)
	add_subdirectory(device)
endif()

