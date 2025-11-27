# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if (ENABLE_TEST)
    set(OPEN_BUILD_PROJECT "ON")
    add_subdirectory(llt/stub)
    set(HCCL_OPEN_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
    add_subdirectory(llt/ut)

    if (HCOMM_ST)
        add_subdirectory(llt/st)
    endif()
else()
set(HCCL_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(HCCL_OPEN_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(HCCL_OPEN_CODE_ALGORITHM ${CMAKE_CURRENT_SOURCE_DIR}/../src/algorithm)
set(HCCL_OPEN_CODE_FRAMEWORK ${CMAKE_CURRENT_SOURCE_DIR}/../src/framework)
set(HCCL_OPEN_CODE_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../src/common)
set(HCCL_OPEN_CODE_PLATFORM ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform)
set(HCCL_ALG_ANALYZER ${TOP_DIR}/asl/devtools/hccl_tools/hccl_alg_analyzer)
set(HCCL_ANALYZER_INC_DIR ${TOP_DIR}/asl/devtools/hccl_tools/hccl_alg_analyzer/pub_inc)
set(HCCL_INSINGHT_INC_DIR ${TOP_DIR}/llt/ace/comop/hccl/insight)
set(HCCL_ALG_ANALYZER_LIB_ROOT ${TOP_DIR}/llt/ace/comop/hccl)
set(HCCL_STUB_CODE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/stub)
set(mockcpp_INCLUDE_DIRS "/usr/local/mockcpp/include")
set(LIBRARY_MOCKCPP "/usr/local/mockcpp/lib/libmockcpp.a")
set(openmpi_INCLUDE_DIRS "/usr/local/openmpi/include")
set(LIBRARY_OPENMPI "/usr/local/openmpi/lib")
find_library(LIBRARY_GTEST libgtest.so ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest_shared/lib64)

# 测试用例的可执行文件、动态链接库均输出到build/test目录下，方便管理
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HCCL_OPEN_CODE_ROOT}/build/test)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${HCCL_OPEN_CODE_ROOT}/build/test)

set(algorithm_include_list
    ${HCCL_OPEN_CODE_ALGORITHM}/base/inc
    ${HCCL_OPEN_CODE_ALGORITHM}/pub_inc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/task
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/registry
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/registry
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/resource_manager
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_aiv_template
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator
    ${HCCL_OPEN_CODE_ALGORITHM}/base/mc2_handler

    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/component
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_reduce
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_reduce/310P
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather/310P
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather_v
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather_v/310P
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter/310P
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter_v
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter_v/310P
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_broadcast
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_broadcast/310P
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_send_receive
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_to_all
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_scatter

    ${HCCL_OPEN_CODE_ALGORITHM}/impl/inc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/legacy
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/legacy/operator
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/legacy

    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_scatter
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_reduce_scatter
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_reduce
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_broadcast
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_alltoall
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_alltoallv
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_all_reduce
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_all_gather
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/inc_all_reduce_deter
)

set(framework_include_list
    # framework目录下的头文件
    ${HCCL_OPEN_CODE_FRAMEWORK}
    ${HCCL_OPEN_CODE_FRAMEWORK}/inc
    ${HCCL_OPEN_CODE_FRAMEWORK}/hcom
    ${HCCL_OPEN_CODE_FRAMEWORK}/nslbdp
    ${HCCL_OPEN_CODE_FRAMEWORK}/device
    ${HCCL_OPEN_CODE_FRAMEWORK}/device/inc
    ${HCCL_OPEN_CODE_FRAMEWORK}/device/framework
    ${HCCL_OPEN_CODE_FRAMEWORK}/device/utils
    ${HCCL_OPEN_CODE_FRAMEWORK}/device/common
    ${HCCL_OPEN_CODE_FRAMEWORK}/op_base/src

    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/host
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/config
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/topo
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/task
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/mgr
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/exception
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/thread
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/h2d_dto
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/hashtable
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/opexecounter
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/onesided_memory_management

    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/aclgraph
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/resource_manager
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/one_sided_service
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/resource_manager
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/zero_copy
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/independent_op/
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl/independent_op/resource/engine
    
    ${HCCL_OPEN_CODE_FRAMEWORK}/cluster_maintenance/health/heartbeat
    ${HCCL_OPEN_CODE_FRAMEWORK}/cluster_maintenance/recovery/operator_retry
    ${HCCL_OPEN_CODE_FRAMEWORK}/cluster_maintenance/detect/detect_connect_anomalies
)

set(common_include_list
    # common目录下的头文件
    ${HCCL_OPEN_CODE_COMMON}/health
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling/inc
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling/inc/host
    ${HCCL_OPEN_CODE_COMMON}/stream
)

set(platform_include_list
    # platform目录下的头文件
    ${HCCL_OPEN_CODE_PLATFORM}
    ${HCCL_OPEN_CODE_PLATFORM}/common
    ${HCCL_OPEN_CODE_PLATFORM}/typical
    ${HCCL_OPEN_CODE_PLATFORM}/remote_access
    ${HCCL_OPEN_CODE_PLATFORM}/inc
    ${HCCL_OPEN_CODE_PLATFORM}/inc/adapter
    ${HCCL_OPEN_CODE_PLATFORM}/resource/mem
    ${HCCL_OPEN_CODE_PLATFORM}/resource/notify
    ${HCCL_OPEN_CODE_PLATFORM}/resource/socket
    ${HCCL_OPEN_CODE_PLATFORM}/resource/stream
    ${HCCL_OPEN_CODE_PLATFORM}/resource/rma_buffer
    ${HCCL_OPEN_CODE_PLATFORM}/resource/transport
    ${HCCL_OPEN_CODE_PLATFORM}/resource/transport/heterog
    ${HCCL_OPEN_CODE_PLATFORM}/resource/transport/host
    ${HCCL_OPEN_CODE_PLATFORM}/resource/transport/onesided
    ${HCCL_OPEN_CODE_PLATFORM}/resource/transport/device
    ${HCCL_OPEN_CODE_PLATFORM}/task
    ${HCCL_OPEN_CODE_PLATFORM}/ping_mesh
    ${HCCL_OPEN_CODE_PLATFORM}/tbe_vector_reduce

    ${HCCL_OPEN_CODE_PLATFORM}/hccp/inc
    ${HCCL_OPEN_CODE_PLATFORM}/hccp/inc/network
    ${HCCL_OPEN_CODE_PLATFORM}/legacy/hccl_tbe_task
)

set(other_include_list
    ${HCCL_OPEN_CODE_ROOT}/inc/hccl
    ${HCCL_OPEN_CODE_ROOT}/inc/hccl/hccl
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/asl
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/toolchain
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/tsch
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/driver
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/register
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/external
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/external/ge_common
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/common/plugin
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/common/ge_common
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/common/util/trace_manager
    ${HCCL_OPEN_CODE_ROOT}/externel_depends/metadef/inc/exe_graph/lowering
    ${HCCL_OPEN_CODE_ROOT}/src
    ${HCCL_OPEN_CODE_ROOT}/src/inc
    ${HCCL_OPEN_CODE_ROOT}/src/pub_inc
    ${HCCL_OPEN_CODE_ROOT}/src/pub_inc/inner
    ${HCCL_OPEN_CODE_ROOT}/src/pub_inc/new
    ${HCCL_OPEN_CODE_ROOT}/src/hccd
    ${HCCL_OPEN_CODE_ROOT}/src/hcom/src/gradient_segment
    ${HCCL_OPEN_CODE_ROOT}/src/hccl/hccl_comm/inc
    ${HCCL_OPEN_CODE_ROOT}/src/hccl/hccl_comm/comm_conn
    ${HCCL_OPEN_CODE_ROOT}/src/hccl/hccl_comm/executor/component/inc
    ${HCCL_OPEN_CODE_ROOT}/src/hccl_heterog/common
    ${HCCL_OPEN_CODE_ROOT}/src/hccl_heterog/rpc
    ${HCCL_OPEN_CODE_ROOT}/src/hccl_heterog/heterog_cpu
    ${HCCL_OPEN_CODE_ROOT}/src/hcom/src
    ${HCCL_OPEN_CODE_ROOT}/cltm/src
)
# 桩函数
# add_subdirectory(stub)
# 算法组测试用例
# add_subdirectory(algorithm)
# # 增加framework下的所有测试用例
# add_subdirectory(framework)
# 增加对外接口单线程测试
# add_subdirectory(hccl_api_single_thread_test)

# 下面的测试用例仅解决编译问题，需要进行拆分
# llt目录下的测试用例
# add_subdirectory(llt/ut)
# add_subdirectory(llt/st)
# add_subdirectory(framework)

set(CMAKE_PREFIX_PATH
    ${ASCEND_3RD_LIB_PATH}/json 
    ${ASCEND_3RD_LIB_PATH}/gtest_shared/lib64
    ${ASCEND_INSTALL_PATH}
)

# find_package(GTest REQUIRED)

set(COMMON_INC_PATH
    ${ASCEND_3RD_LIB_PATH}/gtest_shared/include
)

# UT
if(ENABLE_UT)
    add_subdirectory(ut)
endif()

# ST
if(ENABLE_ST)
    add_subdirectory(st)
endif()
