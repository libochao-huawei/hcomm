set(HCCL_ORION_ROOT ${TOP_DIR}/ace/comop/hccl/orion)
set(HCCL_ALG_ROOT ${TOP_DIR}/asl/devtools/hccl_tools/hccl_alg_analyzer)
set(HCCL_ALG_V2_CODE_ROOT ${HCCL_ORION_ROOT}/service/collective)
set(HCCL_NEXT_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole/hccl_next)
set(HCCL_ALG_NEXT_CODE_ROOT ${HCCL_NEXT_CODE_ROOT}/service/collective)

set(include_list
    # hccl inc目录
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/runtime/include/external
    ${TOP_DIR}/runtime/pkg_inc/runtime
    ${TOP_DIR}/runtime/pkg_inc/
    ${TOP_DIR}/runtime/pkg_inc/profiling
    ${TOP_DIR}/ace/npuruntime/inc
    ${TOP_DIR}/ace/npuruntime/inc/runtime
    ${TOP_DIR}/ace/npuruntime/inc/external
    ${TOP_DIR}/ace/npuruntime/acl/inc/external/
    ${TOP_DIR}/ace/npuruntime/acl/inc/external/acl
    ${TOP_DIR}/ace/comop/inc/external
    ${TOP_DIR}/ace/comop/inc/external/hccl
    ${TOP_DIR}/drivers/network/inc/network
    
    ${TOP_DIR}/abl/msprof/inc
    ${TOP_DIR}/abl/slog/inc/toolchain
    ${TOP_DIR}/open_source/json/include

    ${HCCL_ALG_ROOT}/adapter_v2
    ${HCCL_ALG_ROOT}/adapter_v2/stub_ccu
    ${HCCL_ALG_ROOT}/adapter_v2/transport
    ${HCCL_ALG_ROOT}/adapter_v2/common
    ${HCCL_ALG_ROOT}/inc
    ${HCCL_ALG_ROOT}/checker

    ${HCCL_ORION_ROOT}/common
    ${HCCL_ORION_ROOT}/common/types
    ${HCCL_ORION_ROOT}/common/utils
    ${HCCL_ORION_ROOT}/common/exception
    ${HCCL_ORION_ROOT}/include
    ${HCCL_ORION_ROOT}/unified_platform
    ${HCCL_ORION_ROOT}/unified_platform/common
    ${HCCL_ORION_ROOT}/unified_platform/resource/buffer
    ${HCCL_ORION_ROOT}/unified_platform/external_system
    ${HCCL_ORION_ROOT}/unified_platform/resource
    ${HCCL_ORION_ROOT}/unified_platform/resource/transport/aicpu
    ${HCCL_ORION_ROOT}/unified_platform/resource/stream/aicpu
    ${HCCL_ORION_ROOT}/unified_platform/resource/notify/aicpu
    ${HCCL_ORION_ROOT}/unified_platform/resource/connection/aicpu
    ${HCCL_ORION_ROOT}/unified_platform/resource/buffer/aicpu
    ${HCCL_ORION_ROOT}/unified_platform/resource/socket
    ${HCCL_ORION_ROOT}/unified_platform/pub_inc
    ${HCCL_ORION_ROOT}/unified_platform/pub_inc/ccu
    ${HCCL_ORION_ROOT}/unified_platform/pub_inc/resource
    ${HCCL_ORION_ROOT}/unified_platform/pub_inc/resource/transport
    ${HCCL_ORION_ROOT}/unified_platform/pub_inc/resource/net_dev
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_microcode
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_device
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/interface
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/context
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/arithmetic
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/loop
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/common
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/sync
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/data
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/control
    ${HCCL_ORION_ROOT}/unified_platform/ccu/ccu_representation/reps/translator
    ${HCCL_ORION_ROOT}/unified_platform/ccu/dfx

    ${HCCL_ORION_ROOT}/framework
    ${HCCL_ORION_ROOT}/framework/entrace
    ${HCCL_NEXT_CODE_ROOT}/framework/topo
    ${HCCL_ORION_ROOT}/framework/device_mode
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/phy_topo
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/rank_graph
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/common
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/rank_table_info
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/rank_graph_builder
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/topo_info
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/phy_topo_builder
    ${HCCL_NEXT_CODE_ROOT}/framework/topo/new_topo_builder/virtual_topo
    ${HCCL_ORION_ROOT}/framework/resource_manager/transport/aicpu
    ${HCCL_ORION_ROOT}/framework/resource_manager/buffer/aicpu
    ${HCCL_ORION_ROOT}/framework/resource_manager/socket

    ${HCCL_ORION_ROOT}/framework/communicator
    ${HCCL_ORION_ROOT}/framework/communicator/aicpu
    ${HCCL_ORION_ROOT}/framework/dfx/common
    ${HCCL_ORION_ROOT}/framework/ccu
    ${HCCL_ORION_ROOT}/framework/ccu/ccu_ins
    ${HCCL_ORION_ROOT}/framework/ccu/ccu_manager
    ${HCCL_ORION_ROOT}/framework/env_config

    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/interface
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/interface/host
    ${HCCL_ALG_NEXT_CODE_ROOT}/instruction
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/selector
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/alg_registry
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/coll_alg_factory/alg_executor
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/coll_alg_factory/alg_executor/ins_alg_executor
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/coll_alg_factory/alg_template
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/coll_alg_factory/alg_template/ins_alg_template
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/coll_alg_factory/alg_topo_match
    ${HCCL_ALG_NEXT_CODE_ROOT}/alg/coll_alg_factory/utils/common

    ${HCCL_ALG_V2_CODE_ROOT}
    ${HCCL_ALG_V2_CODE_ROOT}/primitive
    ${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_executor/prim_alg_executor
    ${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_template/prim_alg_template
    ${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_template/ccu_alg_template
    ${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_template/aiv_alg_template
    ${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_ccu_context
    ${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_aiv_instruction

    ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto
    ${TOP_DIR}/inc/external/
    ${TOP_DIR}/llt/ace/comop/hccl/orion/common/
    ${TOP_DIR}/llt/ace/comop/hccl/orion/common/acl_stub
    ${TOP_DIR}/llt/ace/comop/hccl/orion/common/virtual_topo_stub
    ${TOP_DIR}/ace/comop/hccl/orion/framework/aiv/aiv_ins
    ${TOP_DIR}/abl/adump/inc/adump
    ${TOP_DIR}/metadef/inc/external/
    ${TOP_DIR}/metadef/pkg_inc
)

file(GLOB_RECURSE ALG_CCU_CONTEXT_SRC "${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_ccu_context/*.cc")
file(GLOB_RECURSE ALG_CCU_TEMPLATE_SRC "${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_template/ccu_alg_template/*.cc")
file(GLOB_RECURSE ALG_AIV_TEMPLATE_SRC "${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_template/aiv_alg_template/*.cc")
file(GLOB_RECURSE ALG_PRIM_SRC "${HCCL_ALG_V2_CODE_ROOT}/primitive/*.cc")
file(GLOB_RECURSE ALG_PRIM_EXECUTOR_SRC "${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_executor/prim_alg_executor/*.cc")
file(GLOB_RECURSE ALG_PRIM_TEMPLATE_SRC "${HCCL_ALG_V2_CODE_ROOT}/alg/coll_alg_factory/alg_template/prim_alg_template/*.cc")

file(GLOB_RECURSE ALG_NEXT_SRC "${HCCL_ALG_NEXT_CODE_ROOT}/alg/*.cc")

# 排除不想编译的文件
file(GLOB_RECURSE EXCLUDE_BATCH_SEND_RECV_SRC "${HCCL_ALG_NEXT_CODE_ROOT}/*/ins_batchSendRecv_executor.cc")
file(GLOB_RECURSE EXCLUDE_DEVICE_SRC "${HCCL_ALG_NEXT_CODE_ROOT}/*/device_utils.cc")
list(REMOVE_ITEM ALG_NEXT_SRC ${EXCLUDE_BATCH_SEND_RECV_SRC})
list(REMOVE_ITEM ALG_NEXT_SRC ${EXCLUDE_DEVICE_SRC})
file(GLOB_RECURSE PRIMITIVE_SRC "${HCCL_ALG_V2_CODE_ROOT}/primitive/*.cc")

# 被测代码编译
set(david_to_test_list
    #被测代码
    ${HCCL_ALG_V2_CODE_ROOT}/instruction/instruction.cc
    ${HCCL_ALG_V2_CODE_ROOT}/coll_operator.cc
    ${ALG_CCU_CONTEXT_SRC}
    ${ALG_CCU_TEMPLATE_SRC}
    ${ALG_AIV_TEMPLATE_SRC}
    ${ALG_PRIM_SRC}
    ${ALG_PRIM_EXECUTOR_SRC}
    ${ALG_PRIM_TEMPLATE_SRC}
    ${ALG_NEXT_SRC}
    ${PRIMITIVE_SRC}
    ${TOP_DIR}/llt/ace/comop/hccl/orion/common/acl_stub/acl_stub.cc
)

set_legacy_protobuf()

add_library(hccl_orion_alg_test SHARED ${david_to_test_list})

#头文件搜索路径
target_include_directories(hccl_orion_alg_test PRIVATE
    ${include_list}
)

target_compile_definitions(hccl_orion_alg_test PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
)

target_compile_options(hccl_orion_alg_test PRIVATE
    -U_FORTIFY_SOURCE
    -O0
    -g
    -pipe
)

target_link_libraries(hccl_orion_alg_test PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)

add_custom_target(hccl_orion_alg_test_lib
    DEPENDS hccl_orion_alg_test
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cp libhccl_orion_alg_test.so ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cd ${CMAKE_INSTALL_PREFIX}/hccl_lib
)

# 测试用例编译可执行程序
set(test_case_list
)

add_executable(orion_alg_testcase ${test_case_list})
add_subdirectory(testcase)

#头文件搜索路径
target_include_directories(orion_alg_testcase PRIVATE
    ${include_list}
)

target_compile_definitions(orion_alg_testcase PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    -DCCL_KERNEL_AICPU
    -DHCCL_ALG_ANALYZER_DAVID
)

target_compile_options(orion_alg_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)

target_link_libraries(orion_alg_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)

add_dependencies(orion_alg_testcase
    hccl_alg_adaption_v2
    hccl_alg_analyzer_lib
    hccl_alg_analyzer_test
    hccl_orion_alg_test_lib
)

add_executable(orion_alg_aicpu_2d_testcase ${test_case_list})
add_subdirectory(aicpu_2d_testcase)
 
#头文件搜索路径
target_include_directories(orion_alg_aicpu_2d_testcase PRIVATE
    ${include_list}
)
 
target_compile_definitions(orion_alg_aicpu_2d_testcase PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    -DCCL_KERNEL_AICPU
    -DHCCL_ALG_ANALYZER_DAVID
)
 
target_compile_options(orion_alg_aicpu_2d_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)
 
target_link_libraries(orion_alg_aicpu_2d_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)
 
add_dependencies(orion_alg_aicpu_2d_testcase
    hccl_alg_adaption_v2
    hccl_alg_analyzer_lib
    hccl_alg_analyzer_test
    hccl_orion_alg_test_lib
)

add_executable(orion_alg_aiv_1d_testcase ${test_case_list})
add_subdirectory(aiv_1d_testcase)
 
#头文件搜索路径
target_include_directories(orion_alg_aiv_1d_testcase PRIVATE
    ${include_list}
)
 
target_compile_definitions(orion_alg_aiv_1d_testcase PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    -DCCL_KERNEL_AICPU
    -DHCCL_ALG_ANALYZER_DAVID
)
 
target_compile_options(orion_alg_aiv_1d_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)
 
target_link_libraries(orion_alg_aiv_1d_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)
 
add_dependencies(orion_alg_aiv_1d_testcase
    hccl_alg_adaption_v2
    hccl_alg_analyzer_lib
    hccl_alg_analyzer_test
    hccl_orion_alg_test_lib
)
 
add_executable(orion_alg_ccu_1d_testcase ${test_case_list})
add_subdirectory(ccu_1d_testcase)

#头文件搜索路径
target_include_directories(orion_alg_ccu_1d_testcase PRIVATE
    ${include_list}
)

target_compile_definitions(orion_alg_ccu_1d_testcase PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    -DCCL_KERNEL_AICPU
    -DHCCL_ALG_ANALYZER_DAVID
)

target_compile_options(orion_alg_ccu_1d_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)

target_link_libraries(orion_alg_ccu_1d_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)

add_dependencies(orion_alg_ccu_1d_testcase hccl_alg_analyzer hccl_orion_alg_test hccl_alg_adaption_v2)

add_executable(orion_alg_ccu_1d_hf16p_testcase ${test_case_list})
add_subdirectory(ccu_1d_hf16p_testcase)

#头文件搜索路径
target_include_directories(orion_alg_ccu_1d_hf16p_testcase PRIVATE
    ${include_list}
)

target_compile_definitions(orion_alg_ccu_1d_hf16p_testcase PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    -DCCL_KERNEL_AICPU
    -DHCCL_ALG_ANALYZER_DAVID
)

target_compile_options(orion_alg_ccu_1d_hf16p_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)

target_link_libraries(orion_alg_ccu_1d_hf16p_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)

add_dependencies(orion_alg_ccu_1d_hf16p_testcase hccl_alg_analyzer hccl_orion_alg_test hccl_alg_adaption_v2)

add_executable(orion_alg_ccu_2d_testcase ${test_case_list})
add_subdirectory(ccu_2d_testcase)

#头文件搜索路径
target_include_directories(orion_alg_ccu_2d_testcase PRIVATE
    ${include_list}
)

target_compile_definitions(orion_alg_ccu_2d_testcase PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    -DCCL_KERNEL_AICPU
    -DHCCL_ALG_ANALYZER_DAVID
)

target_compile_options(orion_alg_ccu_2d_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)

target_link_libraries(orion_alg_ccu_2d_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)

add_executable(orion_alg_function_ut_testcase ${test_case_list})
add_subdirectory(function_ut_testcase)
 
#头文件搜索路径
target_include_directories(orion_alg_function_ut_testcase PRIVATE
    ${include_list}
    ${HCCL_ALG_ROOT}/checker/task_collector
    ${HCCL_ALG_ROOT}/adapter_v2
    ${HCCL_ALG_ROOT}/checker/mem_conflict_check
)
 
target_compile_definitions(orion_alg_function_ut_testcase PRIVATE
    google=ascend_private
    -DHCCL_ALG_ANALYZER_DAVID
)
 
target_compile_options(orion_alg_function_ut_testcase PRIVATE
    -U_FORTIFY_SOURCE
    -g
    -O0
)
 
target_link_libraries(orion_alg_function_ut_testcase PRIVATE
    hccl_alg_analyzer
    hccl_orion_alg_test
    hccl_alg_adaption_v2
    $<BUILD_INTERFACE:intf_llt_pub>
    ascend_protobuf
    c_sec_static
    -lrt
    -ldl
)
 
add_dependencies(orion_alg_function_ut_testcase
    hccl_alg_adaption_v2
    hccl_alg_analyzer_lib
    hccl_alg_analyzer_test
    hccl_orion_alg_test_lib
)

add_dependencies(orion_alg_ccu_2d_testcase hccl_alg_analyzer hccl_orion_alg_test hccl_alg_adaption_v2 orion_alg_function_ut_testcase)