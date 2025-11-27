# Copyright (c) 2024 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ======================================================================================================================

set(CMAKE_BUILD_TYPE Debug)

set(proto_src_files
    ${METADEF_PROTO_DIR}/task.proto
)

protobuf_generate(hcce PROTO_SRCS PROTO_HDRS ${proto_src_files})

set(hccl_engine_src_file
    #/hccl/hcom_graph_adaptor/ge_plugin
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom_plugin.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/plugin_manager.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/hcom_op_utils.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/op_fusion_base.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/ops_kernel_info_store_base.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/ops_kernel_builder_base.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/hcom_base_funs.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/hcom_acl_adapter.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common/hcom_log.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_all_reduce_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_alltoallvc_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_allgather_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_reducescatter_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_broadcast_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_reduce_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_ops_kernel_builder.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_fusion_optimizer.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_graph_optimizer.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_ops_kernel_info_store.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/offline_build_config_parse.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_graph_mc2.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/hcom_graph_superkernel.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/lowering/hcom_build_graph.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/lowering/hcom_launch_kernel.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/lowering/hcom_node_converter.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_all_reduce_fusion.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_graph_optimizer.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_ops_kernel_builder.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_hcom_ops_kernel_info_store.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/auto_tuning/auto_tuning_plugin.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool/communication.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool/cluster.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool/evaluator.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool/layers.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool/model.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool/topology.cc

    #/hccl/hcom_graph_adaptor/hcom_executor
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/hcom_executor/hcom_executor.cc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/hcom_executor/executor_message.cc

    #/hccl/hcom_gradient_split_tune
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_gradient_split_tune/src/gradient_split_tune.cc
    ${PROTO_HDRS}
)

set(hcce_llt_stub
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub/hccl_stub.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub/llt_hccl_stub.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub/llt_hccl_stub_gert.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub/llt_hccl_stub_ge.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub/llt_hccl_stub_sal.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../stub/llt_hccl_stub_hccl.cc
)

set(hccl_engine_src_test_file
    ${CMAKE_CURRENT_SOURCE_DIR}/st_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hcom_plugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hcom_executor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hcom_launch_kernel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hcom_lowering.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hccl_kernel_builder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hccl_graph_optimizer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_auto_tuning_hcom_all_reduce_fusion.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_autotuning_hccl_kernel_builder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_gradient_split_tune.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_gradient_split.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_hccl_kernel_info.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/st_superFastKernel.cc
    ${hcce_llt_stub}
)

add_execstable(hccl_engine_stest
    ${hccl_engine_src_file}
    ${hccl_engine_src_test_file}
)

target_include_directories(hccl_engine_stest PRIVATE
    ${AIR_CODE_DIR}
    ${AIR_CODE_DIR}/inc/external
    ${AIR_CODE_DIR}/inc
    ${AIR_CODE_DIR}/inc/framework
    ${AIR_CODE_DIR}/inc/graph_metadef
    ${AIR_CODE_DIR}/inc/graph_metadef/register
    ${AIR_CODE_DIR}/inc/graph_metadef/exe_graph
    ${AIR_CODE_DIR}/inc/graph_metadef/exe_graph/lowering
    ${AIR_CODE_DIR}/inc/graph_metadef/external/hcom
    ${CMAKE_BINARY_DIR}/proto/hcom_graph_adaptor_proto

    ${AIR_CODE_DIR}/compiler/engines/hcclengine/inc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_gradient_split_tune/inc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/inc
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/hcom_executor
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/common
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/hcom/lowering
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/auto_tuning
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin
    ${AIR_CODE_DIR}/compiler/engines/hcclengine/hcom_graph_adaptor/ge_plugin/calculation_tool

    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/graph
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/hccl
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/hccl/external
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/platform
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/runtime
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/runtime/runtime
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/runtime/runtime/rts
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/msprof
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/adump/external
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/mmpa
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/external
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/slog
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/slog/toolchain
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/ascendcl/external
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/ascend_hal
    ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include

    ${ASCEND_3RD_LIB_PATH}/json/include

    ${CMAKE_BINARY_DIR}/proto/hcce

    ${CMAKE_CURRENT_SOURCE_DIR}/../stub
    ${AIR_CODE_DIR}/tests/engines/hccleng/third_party/mockcpp/mockcpp-2.7/3rdparty
    ${AIR_CODE_DIR}/tests/engines/hccleng/third_party/mockcpp/mockcpp-2.7/include
)

target_compile_definitions(hccl_engine_stest PRIVATE
    RUN_TEST
    google=ascend_private
    $<$<STREQUAL:${BUILD_OPEN_PROJECT},True>:ONLY_COMPILE_OPEN_SRC>
    #-fsanitize=address    #ddd
    #-fno-omit-frame-pointer    #ddd
    #-g    #ddd
)

target_link_options(hccl_engine_stest PRIVATE
    -Wl,-rpath,${ASCEND_COMPILER_DIR}
    #-fsanitize=address    #ddd
)

target_link_directories(hccl_engine_stest PRIVATE
    ${ASCEND_INSTALL_PATH}/lib64
)

find_library(LIBRARY_MOCKCPP libmockcpp.a ${AIR_CODE_DIR}/tests/engines/hccleng/third_party/mockcpp/mockcpp-2.7/build)

target_link_libraries(hccl_engine_stest PRIVATE
    intf_pub
    runtime_headers
    msprof_headers
    -Wl,--as-needed
    GTest::gtest GTest::gtest_main GTest::gmock
    ${LIBRARY_MOCKCPP}
    error_manager
    graph
    graph_base
    metadef
    ascend_protobuf
    register
    opp_registry
    c_sec
    platform
    mmpa
    json
    -Wl,--no-as-needed
    slog
    -ldl
    -lrt
    -lpthread
    -lgcov
)