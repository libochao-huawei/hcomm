set(HCCL_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(HCCL_ORION_ROOT ${TOP_DIR}/ace/comop/hccl/orion)
set(HCCL_OPEN_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/open_source)
set(HCCL_ALG_ROOT ${TOP_DIR}/asl/devtools/hccl_tools/hccl_alg_analyzer)
set(HCCL_OPEN_CODE_ALGORITHM ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm)
set(HCCL_OPEN_CODE_FRAMEWORK ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework)
set(HCCL_OPEN_CODE_COMMON ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common)

set(include_list
    # 测试框架的代码
    ${HCCL_ALG_ROOT}/inc
    ${HCCL_ALG_ROOT}/checker
    ${HCCL_ALG_ROOT}/checker/common/types
    ${HCCL_ALG_ROOT}/checker/rank_info_recorder
    ${HCCL_ALG_ROOT}/checker/mem_layout
    ${HCCL_ALG_ROOT}/checker/link_type_recorder
    ${HCCL_ALG_ROOT}/checker/task_collector
    ${HCCL_ALG_ROOT}/checker/semantics_check
    ${HCCL_ALG_ROOT}/checker/singletask_check
    ${HCCL_ALG_ROOT}/checker/ui/data_dumper
    ${HCCL_ALG_ROOT}/checker/param_trans
    ${HCCL_ALG_ROOT}/checker/mem_conflict_check

    ${HCCL_ALG_ROOT}/adapter_v1/stream
    ${HCCL_ALG_ROOT}/adapter_v1/aiv
    ${HCCL_ALG_ROOT}/adapter_v1

    ${HCCL_ORION_ROOT}/service/collective/primitive

    # hccl inc目录
    ${TOP_DIR}/ace/comop/inc
    ${TOP_DIR}/ace/comop/inc/hccl
    ${TOP_DIR}/ace/comop/inc/external
    ${TOP_DIR}/ace/comop/inc/external/hccl

    # inc目录下头文件
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/aicpu
    ${TOP_DIR}/inc/driver
    ${TOP_DIR}/metadef/inc/common/util/error_manager
    ${TOP_DIR}/metadef/inc
    ${TOP_DIR}/metadef/pkg_inc
    ${TOP_DIR}/ace/npuruntime/inc
    ${TOP_DIR}/ace/npuruntime/inc/external
    ${TOP_DIR}/ace/npuruntime/inc/runtime
    ${TOP_DIR}/abl/msprof/inc
    ${TOP_DIR}/abl/atrace/inc/utrace
    ${TOP_DIR}/abl/mmpa/inc/mmpa
    ${TOP_DIR}/drivers/network/inc
    ${TOP_DIR}/abl/mmpa/inc/mmpa

    # 添加hccl目录
    ${HCCL_CODE_ROOT}/hccl/algorithm/pub_inc
    ${HCCL_CODE_ROOT}/hccl/pub_inc
    ${HCCL_CODE_ROOT}/hccl/pub_inc/inner
    ${HCCL_CODE_ROOT}/hccl/pub_inc/new
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_gather
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_all_reduce
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoallv
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_alltoall
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce_scatter
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_reduce
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_scatter
    ${HCCL_CODE_ROOT}/hccl/algorithm/base/alg_template/temp_broadcast

    ${HCCL_OPEN_CODE_ALGORITHM}/base/inc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/task
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/resource_manager
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/component

    ${HCCL_OPEN_CODE_ALGORITHM}/impl/inc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/legacy
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/legacy

    ${HCCL_OPEN_CODE_COMMON}/debug/profiling
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling/inc
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling/inc/host
    ${HCCL_OPEN_CODE_COMMON}/debug/config

    # platform目录下的头文件
    ${HCCL_CODE_ROOT}/hccl/platform/inc
    ${HCCL_CODE_ROOT}/hccl/platform/inc/adapter
    ${HCCL_CODE_ROOT}/hccl/platform/common

    # framework目录下的头文件
    ${HCCL_OPEN_CODE_FRAMEWORK}/inc
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/host
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/config
    ${HCCL_OPEN_CODE_FRAMEWORK}/common/src/topo
    ${HCCL_OPEN_CODE_FRAMEWORK}/communicator/impl

    # libc_sec目录下的头文件
    ${TOP_DIR}/abl/slog/inc/toolchain

    # json 目录的头文件
    ${TOP_DIR}/open_source/json/include

    ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto
)

set(checker_to_test_list

)

add_library(hccl_alg_analyzer SHARED ${checker_to_test_list})

add_subdirectory(checker)
add_subdirectory(adapter_v1)

# 需要针对protobuf自动编译生成的文件设置GENERATED属性，否则会编译报错
set_property(SOURCE ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto/analysis_result.pb.cc PROPERTY GENERATED 1)

#头文件搜索路径
target_include_directories(hccl_alg_analyzer PRIVATE
    ${include_list}
)

if (HOST_HCCL_LLT STREQUAL "2")
    target_compile_definitions(hccl_alg_analyzer PRIVATE
        google=ascend_private
        -DOPEN_HCCL_TEST
        -DHCCL_ALG_ANALYZER_DAVID   #checker v2相关的编译宏
        _GLIBCXX_USE_CXX11_ABI=0
    )
else()
    target_compile_definitions(hccl_alg_analyzer PRIVATE
        google=ascend_private
        -DOPEN_HCCL_TEST
        _GLIBCXX_USE_CXX11_ABI=0
    )
endif()

target_compile_options(hccl_alg_analyzer PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O2
    -pipe
)

if(HOST_HCCL_LLT STREQUAL "1")
    target_link_libraries(hccl_alg_analyzer PRIVATE
        hccl_alg_adaption
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:atrace_headers>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )
elseif(HOST_HCCL_LLT STREQUAL "2")
    target_link_libraries(hccl_alg_analyzer PRIVATE
        hccl_alg_adaption_v2
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:atrace_headers>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )
else()
    target_link_libraries(hccl_alg_analyzer PRIVATE
        hccl_alg_adaption
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:atrace_headers>
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
        ofed_headers
    )
endif()

add_custom_command(TARGET hccl_alg_analyzer POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:hccl_alg_analyzer>
)

install(TARGETS hccl_alg_analyzer
    LIBRARY DESTINATION lib OPTIONAL
    RUNTIME DESTINATION bin OPTIONAL
)

add_custom_target(hccl_alg_analyzer_lib
    DEPENDS hccl_alg_analyzer
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cp libhccl_alg_analyzer.so ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cd ${CMAKE_INSTALL_PREFIX}/hccl_lib && strip libhccl_alg_analyzer.so
    COMMAND cp $<TARGET_FILE:ascend_protobuf> ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cp ${CMAKE_INSTALL_PREFIX}/lib/libc_sec.so ${CMAKE_INSTALL_PREFIX}/hccl_lib
)
