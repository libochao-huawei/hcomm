# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if(BUILD_OPEN_PROJECT)
    set(package_include_list
        ${ASCEND_CANN_PACKAGE_PATH}/include/hccl_alg_analyzer
        ${ASCEND_CANN_PACKAGE_PATH}/include/hccl_alg_analyzer/adapter_v1
        ${ASCEND_CANN_PACKAGE_PATH}/include
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/hccl
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/metadef/common/util/error_manager/
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/msprof/
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/msprof/toolchain
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog/
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/runtime/external
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/slog/toolchain
        ${ASCEND_CANN_PACKAGE_PATH}/include/experiment/mmpa
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/gtest/include
    )
else()
    set(package_include_list
    )
endif()

if(NOT BUILD_OPEN_PROJECT)
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
      ${HCCL_CODE_ROOT}/hccl/pub_inc

      # slog头文件
      ${TOP_DIR}/abl/slog/inc/toolchain
      ${TOP_DIR}/abl/atrace/inc/utrace
      ${HCCL_OPEN_CODE_ROOT}/test/algorithm/testcase

      ${TOP_DIR}/abl/mmpa/inc/mmpa
   )
else()
   set(include_list
      ${HCCL_OPEN_CODE_ROOT}/test/algorithm/testcase
   )
endif()

file(GLOB_RECURSE ALL_REDUCE_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_reduce/*.cc")
file(GLOB_RECURSE ALL_REDUCE_EXECUTOR_310P "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_reduce/310P/*.cc")

file(GLOB_RECURSE ALL_GATHER_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather/*.cc")
file(GLOB_RECURSE ALL_GATHER_EXECUTOR_310P "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather/310P/*.cc")

file(GLOB_RECURSE ALL_GATHER_V_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_gather_v/*.cc")

file(GLOB_RECURSE REDUCE_SCATTER_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter/*.cc")
file(GLOB_RECURSE REDUCE_SCATTER_EXECUTOR_310P "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter/310P/*.cc")

file(GLOB_RECURSE REDUCE_SCATTER_V_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter_v/*.cc")
file(GLOB_RECURSE REDUCE_SCATTER_EXECUTOR_310P "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce_scatter_v/310P/*.cc")

file(GLOB_RECURSE REDUCE_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_reduce/*.cc")

file(GLOB_RECURSE SEND_RECV_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_send_receive/*.cc")

file(GLOB_RECURSE BROADCAST_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_broadcast/*.cc")
file(GLOB_RECURSE BROADCAST_EXECUTOR_310P "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_broadcast/310P/*.cc")

file(GLOB_RECURSE ALL_TO_ALL_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_to_all/*.cc")
list(REMOVE_ITEM ALL_TO_ALL_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_all_to_all/coll_all_to_all_executor_aicpu.cc")

file(GLOB_RECURSE SCATTER_EXECUTOR "${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_scatter/*.cc")

file(GLOB_RECURSE TEMPLATE_BASE "${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/*.cc")
file(GLOB_RECURSE TEMPLATE_BASE_COMPONENT "${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/component/*.cc")

set(src_to_test_list
)

set(src_list_alg
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/alg_env_config.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/topo_matcher.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/alg_configurator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_alg_utils.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/hccl_alg.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/hccl_aiv.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/hccl_aiv_host.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/topo_info_extractor.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_hd_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_mesh_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_p2p_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_partial_mesh_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_ring_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_transport_req_base.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_hccs_plus_sio_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_nb_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_nhr_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_nhr_v1_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_ahc_broke_transport_req.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_ahc_transport_req_base.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/calc_ahc_transport_req.cc

    ${HCCL_OPEN_CODE_ALGORITHM}/base/mc2_handler/mc2_handler.cc

    ${HCCL_OPEN_CODE_ALGORITHM}/impl/task/threadManage.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/task/parallel_task_loader.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/task/task_loader.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/registry/coll_alg_op_registry.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/coll_alg_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/all_reduce_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/all_gather_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/all_gather_v_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/reduce_scatter_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/reduce_scatter_v_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/send_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/receive_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/batch_write_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/broadcast_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/reduce_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/alltoall_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/scatter_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/operator/batchsendrecv_operator.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/registry/coll_alg_exec_registry.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_executor_base.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_native_executor_base.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/coll_comm_executor.cc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor/alg_profiling.cc

    ${ALL_REDUCE_EXECUTOR}
    ${ALL_REDUCE_EXECUTOR_310P}

    ${ALL_GATHER_EXECUTOR}
    ${ALL_GATHER_EXECUTOR_310P}

    ${ALL_GATHER_V_EXECUTOR}

    ${REDUCE_SCATTER_EXECUTOR}
    ${REDUCE_SCATTER_EXECUTOR_310P}

    ${REDUCE_SCATTER_V_EXECUTOR}

    ${SEND_RECV_EXECUTOR}

    ${BROADCAST_EXECUTOR}
    ${BROADCAST_EXECUTOR_310P}

    ${REDUCE_EXECUTOR}

    ${SCATTER_EXECUTOR}

    ${ALL_TO_ALL_EXECUTOR}

    ${TEMPLATE_BASE}
    ${TEMPLATE_BASE_COMPONENT}
)

add_library(hccl_alg_test SHARED ${src_list_alg})

target_include_directories(hccl_alg_test PRIVATE
    ${package_include_list}
    ${include_list}
    ${algorithm_include_list}
    ${framework_include_list}
    ${common_include_list}
    ${other_include_list}
)

target_compile_options(hccl_alg_test PRIVATE
    -Werror
    -fno-common
    -fno-strict-aliasing
    -pipe
    -std=c++14
    -Os
    -O2
    -fstack-protector-all
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer -g>
)

target_compile_definitions(hccl_alg_test PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    _GLIBCXX_USE_CXX11_ABI=0
    OPEN_BUILD_PROJECT
)

if(HOST_HCCL_LLT STREQUAL "1")
    target_link_libraries(hccl_alg_test PRIVATE
        -Wl,--no-as-needed
        -Wl,--no-as-needed
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        c_sec
    )
elseif(NOT BUILD_OPEN_PROJECT)
    target_link_libraries(hccl_alg_test PRIVATE
        -Wl,--no-as-needed
        -Wl,--no-as-needed
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        c_sec
        ofed_headers
    )
elseif(BUILD_OPEN_PROJECT)
    target_link_libraries(hccl_alg_test PRIVATE
        $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    )
endif()

add_custom_target(hccl_alg_test_lib
    DEPENDS hccl_alg_test
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cp libhccl_alg_test.so ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cd ${CMAKE_INSTALL_PREFIX}/hccl_lib
)

add_custom_command(TARGET hccl_alg_test POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:hccl_alg_test>
)

install(TARGETS hccl_alg_test
    LIBRARY DESTINATION lib OPTIONAL
    RUNTIME DESTINATION bin OPTIONAL
)

set_source_files_properties(
    ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_aiv_template/aiv_communication.h PROPERTIES LANGUAGE CXX
)

if(NOT BUILD_OPEN_PROJECT)
    add_library(hccl_alg_aiv_test SHARED
        ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm/base/alg_aiv_template/aiv_communication.h
        ${HCCL_ALG_ANALYZER}/adapter_v1/aiv/aiv_base_stub.cc
        ${HCCL_ALG_ANALYZER}/adapter_v1/aiv/aiv_copy_stub.cc
        ${HCCL_ALG_ANALYZER}/adapter_v1/aiv/aiv_memory_stub.cc
        ${HCCL_ALG_ANALYZER}/adapter_v1/aiv/aiv_sync_stub.cc
    )

    target_include_directories(hccl_alg_aiv_test PRIVATE
        ${HCCL_ANALYZER_INC_DIR}/adapter_v1/aiv
        ${HCCL_OPEN_CODE_ALGORITHM}/pub_inc
        ${HCCL_OPEN_CODE_FRAMEWORK}/inc
        ${HCCL_ALG_ANALYZER}/adapter_v1
        ${HCCL_ALG_ANALYZER}/adapter_v1/aiv
        ${HCCL_ALG_ANALYZER}/inc
        ${HCCL_ALG_ANALYZER}/checker
        ${HCCL_ALG_ANALYZER}/checker/rank_info_recorder
        ${HCCL_ALG_ANALYZER}/checker/mem_layout
        ${HCCL_ALG_ANALYZER}/checker/link_type_recorder
        ${HCCL_ALG_ANALYZER}/checker/task_collector
        ${HCCL_CODE_ROOT}/hccl/pub_inc
        ${HCCL_CODE_ROOT}/hccl/algorithm/pub_inc
        ${TOP_DIR}/ace/comop/inc
        ${TOP_DIR}/ace/comop/inc/hccl
        ${TOP_DIR}/ace/comop/inc/external
        ${TOP_DIR}/ace/comop/inc/external/hccl
        ${TOP_DIR}/ace/npuruntime/inc
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/abl/slog/inc/toolchain
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/metadef/inc/common/util/error_manager
    )

    target_compile_options(hccl_alg_aiv_test PRIVATE
        -Werror
        -Wno-unused-variable
        -Wno-unused-value
        -Wno-sign-compare
        -Wno-attributes
        -fno-common
        -fno-strict-aliasing
        -pipe
        -std=c++17
        -Os
        -O0
        -x c++
        -fstack-protector-all
    )

    target_compile_definitions(hccl_alg_aiv_test PRIVATE
        google=ascend_private
        -DOPEN_HCCL_TEST
    )

    target_link_libraries(hccl_alg_aiv_test PRIVATE
        $<BUILD_INTERFACE:intf_pub_cxx17>
        c_sec
    )

    add_custom_command(
        TARGET hccl_alg_aiv_test POST_BUILD
        COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/hccl_lib
        COMMAND cp libhccl_alg_aiv_test.so ${CMAKE_INSTALL_PREFIX}/hccl_lib
    )
endif()

add_subdirectory(testcase)
add_subdirectory(executor_testcase_generalization)
add_subdirectory(executor_reduce_testcase_generalization)
add_subdirectory(executor_alltoall_A3_pipeline_testcase)

if(NOT BUILD_OPEN_PROJECT)
run_llt_test(
   TARGET  open_hccl_test
   TASK_NUM 1
)
endif()