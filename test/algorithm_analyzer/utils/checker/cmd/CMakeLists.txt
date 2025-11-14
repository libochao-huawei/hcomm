set(HCCL_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole)
set(HCCL_ORION_ROOT ${TOP_DIR}/ace/comop/hccl/orion)
set(HCCL_OPEN_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/open_source)
set(HCCL_ALG_ROOT ${TOP_DIR}/asl/devtools/hccl_tools/hccl_alg_analyzer)
set(HCCL_ALG_V2_CODE_ROOT ${HCCL_ORION_ROOT}/service/collective)
set(HCCL_OPEN_CODE_ALGORITHM ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/algorithm)
set(HCCL_OPEN_CODE_FRAMEWORK ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/framework)
set(HCCL_ANALYZER_LIB_ROOT ${TOP_DIR}/output/llt/hccl_lib)
set(HCCL_OPEN_CODE_COMMON ${HCCL_OPEN_CODE_ROOT}/src/domain/collective_communication/common)
get_filename_component(ALG_ANALYZER_TEST_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto ABSOLUTE)
get_filename_component(ALG_ANALYZER_UI_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../ui/proto ABSOLUTE)

set(include_list
    ${TOP_DIR}/inc
    ${TOP_DIR}/inc/external
    ${TOP_DIR}/inc/external/acl
    ${TOP_DIR}/abl/libc_sec/include

    # 测试框架的代码
    ${HCCL_ALG_ROOT}/inc
    ${HCCL_ALG_ROOT}/checker
    ${HCCL_ALG_ROOT}/checker/param_trans
    ${HCCL_ALG_ROOT}/checker/common/types
    ${HCCL_ALG_ROOT}/checker/rank_info_recorder
    ${HCCL_ALG_ROOT}/checker/mem_layout
    ${HCCL_ALG_ROOT}/checker/link_type_recorder
    ${HCCL_ALG_ROOT}/checker/task_collector
    ${HCCL_ALG_ROOT}/checker/semantics_check
    ${HCCL_ALG_ROOT}/checker/ui/data_dumper
    ${HCCL_ALG_ROOT}/checker/mem_conflict_check
    ${HCCL_ALG_ROOT}/checker/param_trans
    ${HCCL_ALG_ROOT}/pub_inc/checker
    ${HCCL_ALG_ROOT}/pub_inc/inc
    ${HCCL_ALG_ROOT}/pub_inc/adapter_v1/notify
    ${HCCL_ALG_ROOT}/pub_inc/adapter_v1/transport
    ${HCCL_ALG_ROOT}/pub_inc/adapter_v1/stream
    ${HCCL_ALG_ROOT}/adapter_v1/aiv
    
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
    ${TOP_DIR}/drivers/network/inc

    # 添加hccl目录
    ${HCCL_OPEN_CODE_ALGORITHM}/pub_inc
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_all_gather
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_all_reduce
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_alltoallv
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_alltoall
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_reduce_scatter
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_reduce
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_scatter
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/temp_broadcast
    ${HCCL_CODE_ROOT}/hccl/pub_inc
    ${HCCL_CODE_ROOT}/hccl/pub_inc/inner
    ${HCCL_CODE_ROOT}/hccl/pub_inc/new

    ${HCCL_OPEN_CODE_ALGORITHM}/base/inc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/task
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/coll_executor
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/resource_manager
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator
    ${HCCL_OPEN_CODE_ALGORITHM}/base/alg_template/component/inc

    ${HCCL_OPEN_CODE_ALGORITHM}/impl/inc
    ${HCCL_OPEN_CODE_ALGORITHM}/impl/legacy
    ${HCCL_OPEN_CODE_ALGORITHM}/base/communicator/legacy

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

    ${HCCL_OPEN_CODE_COMMON}/debug/profiling
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling/inc
    ${HCCL_OPEN_CODE_COMMON}/debug/profiling/inc/host
    ${HCCL_OPEN_CODE_COMMON}/debug/config
    ${HCCL_OPEN_CODE_COMMON}/launch_device
)

set(orion_include_list
    #2.0 - checker
    ${HCCL_ALG_ROOT}/adapter_v2
    ${HCCL_ALG_ROOT}/adapter_v2/stub_ccu
    ${HCCL_ALG_ROOT}/adapter_v2/adapter_rts
    ${HCCL_ALG_V2_CODE_ROOT}/primitive
)

set(analyzer_test_list
    ${HCCL_ALG_ROOT}/checker/cmd/src/main.cc
    ${HCCL_ALG_ROOT}/checker/cmd/src/CheckerCmd.cc
    ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto/cmd.pb.cc
)

add_executable(hccl_alg_analyzer_test
    ${analyzer_test_list}
)

target_compile_definitions(hccl_alg_analyzer_test PRIVATE
    google=ascend_private
    _GLIBCXX_USE_CXX11_ABI=0
)

#头文件搜索路径
target_include_directories(hccl_alg_analyzer_test PRIVATE
    ${include_list}
)

target_compile_options(hccl_alg_analyzer_test PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O2
    -pipe
)

if(HOST_HCCL_LLT STREQUAL "1")
    if(EXISTS "${HCCL_ANALYZER_LIB_ROOT}/libhccl_alg_analyzer.so")
        find_library(LIBRARY_ANALYZER libhccl_alg_analyzer.so ${HCCL_ANALYZER_LIB_ROOT}/)
            target_link_libraries(hccl_alg_analyzer_test PRIVATE
                hccl_alg_adaption
                ${LIBRARY_ANALYZER}
                $<BUILD_INTERFACE:intf_llt_pub>
                -lpthread
                -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
                ascend_protobuf
                c_sec_static
                -lrt
                -ldl
        )
    else()
        target_link_libraries(hccl_alg_analyzer_test PRIVATE
            hccl_alg_adaption
            hccl_alg_analyzer
            $<BUILD_INTERFACE:intf_llt_pub>
            -lpthread
            -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
            ascend_protobuf
            c_sec_static
            -lrt
            -ldl
        )
    endif()
elseif(HOST_HCCL_LLT STREQUAL "2")
    target_include_directories(hccl_alg_analyzer_test PRIVATE
        ${include_list}
        ${orion_include_list}
    )

    target_compile_definitions(hccl_alg_analyzer_test PRIVATE
        google=ascend_private
        _GLIBCXX_USE_CXX11_ABI=0
        -DOPEN_HCCL_TEST
        -DCCL_KERNEL_AICPU
        -DHCCL_ALG_ANALYZER_DAVID
    )

    target_include_directories(hccl_alg_analyzer_test PRIVATE
        ${include_list}
        ${orion_include_list}
    )

    target_link_libraries(hccl_alg_analyzer_test PRIVATE
        hccl_alg_analyzer
        hccl_alg_adaption_v2
        hccl_orion_alg_test
        $<BUILD_INTERFACE:intf_llt_pub>
        -lpthread
        -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
    )

else()
    target_link_libraries(hccl_alg_analyzer_test PRIVATE
        $<BUILD_INTERFACE:intf_pub>
        hccl_alg_adaption
        hccl_alg_analyzer
        -lpthread
        -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib
        ascend_protobuf
        c_sec_static
        -lrt
        -ldl
        ofed_headers
    )
endif()

add_custom_command(
    TARGET hccl_alg_analyzer_test
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/hccl_lib
    COMMAND cp hccl_alg_analyzer_test ${CMAKE_INSTALL_PREFIX}/hccl_lib
)

add_custom_target(alg_analyzer_test_proto)
add_custom_command(TARGET alg_analyzer_test_proto
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto
    COMMAND cd ${ALG_ANALYZER_TEST_PROTO_DIR} && ${PROTOC_PROGRAM} --cpp_out=${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto cmd.proto
    DEPENDS ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto/cmd.pb.cc
    COMMAND cd ${ALG_ANALYZER_UI_PROTO_DIR} && ${PROTOC_PROGRAM} --cpp_out=${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto analysis_result.proto
    DEPENDS ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto/analysis_result.pb.cc
)

set_property(SOURCE ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto/cmd.pb.cc PROPERTY GENERATED 1)
set_property(SOURCE ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto/analysis_result.pb.cc PROPERTY GENERATED 1)

add_dependencies(hccl_alg_analyzer_test alg_analyzer_test_proto)

install(TARGETS hccl_alg_analyzer_test
    LIBRARY DESTINATION lib OPTIONAL
    RUNTIME DESTINATION bin OPTIONAL
)
