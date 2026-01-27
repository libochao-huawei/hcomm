set(HCCL_V2_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/orion)
set(HCCL_NEXT_CODE_ROOT ${TOP_DIR}/ace/comop/hccl/whole/hccl_next)
set(HCCL_LLT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")

file(GLOB_RECURSE HCCL_V2_UTEST_FILES RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    main.cc
)

add_executable(hccl_v2_utest ${HCCL_V2_UTEST_FILES})

FILE(GLOB_RECURSE ut_sources_list LIST_DIRECTORIES TRUE ${HCCL_LLT_ROOT}/ut)

foreach (dir ${ut_sources_list})
    IF ((IS_DIRECTORY ${dir} ) AND (${dir} MATCHES "hccl/orion") AND (NOT ${dir} MATCHES "hccl/orion/mdpi"))
        target_include_directories(hccl_v2_utest PUBLIC ${dir})
    endif ()
endforeach ()

FILE(GLOB_RECURSE sources_list LIST_DIRECTORIES TRUE ${HCCL_V2_CODE_ROOT})

foreach (dir ${sources_list})
    IF ((IS_DIRECTORY ${dir} ) AND (${dir} MATCHES "hccl/orion") AND (NOT ${dir} MATCHES "hccl/orion/mdpi"))
        target_include_directories(hccl_v2_utest PUBLIC ${dir})
    endif ()
endforeach ()

# 自动找到项目内所有头文件
FILE(GLOB_RECURSE hccl_next_dir_list LIST_DIRECTORIES TRUE ${HCCL_NEXT_CODE_ROOT})
foreach (dir ${hccl_next_dir_list})
    IF ((IS_DIRECTORY ${dir}) AND (${dir} MATCHES "whole/hccl_next"))
        target_include_directories(hccl_v2_utest PUBLIC ${dir})
    endif ()
endforeach ()

# 添加项目外部头文件
target_include_directories(hccl_v2_utest PRIVATE
        ${TOP_DIR}/drivers/network/inc/network/
        ${TOP_DIR}/open_source/json/include/
        ${TOP_DIR}/abl/atrace/inc/utrace
        ${TOP_DIR}/ace/comop/inc/
        ${TOP_DIR}/ace/comop/inc/hccl/
        ${TOP_DIR}/ace/comop/inc/external/
        ${TOP_DIR}/ace/comop/hccl/orion/communicator
        ${TOP_DIR}/abl/adump/inc/adump
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/abl/mmpa/inc/mmpa
        ${TOP_DIR}/abl/mmpa/inc/mmpa/sub_inc
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/runtime/pkg_inc/
        ${TOP_DIR}/runtime/pkg_inc/profiling
    )

target_link_libraries(hccl_v2_utest PRIVATE
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:runtime_headers>
        $<BUILD_INTERFACE:cce_headers>
        $<BUILD_INTERFACE:hccl_headers>
        -lrt
        # $<$<STREQUAL:${PRODUCT_SIDE},host>:ascend_hal_stub>
        # $<$<STREQUAL:${PRODUCT_SIDE},device>:ascend_hal>

    c_sec_static
    hccl_headers
    # tsdclient
)


# 编译选项
target_compile_options(hccl_v2_utest PRIVATE
        -std=c++14
        -Werror
        -fno-common
        -fno-strict-aliasing
        -pipe
        -g
        -O0
)

target_include_directories(hccl_v2_utest PRIVATE
        ${TOP_DIR}/drivers/network/inc/network/
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/abl/libc_sec/include
        ${TOP_DIR}/abl/slog/inc/toolchain
        ${TOP_DIR}/ace/npuruntime/inc
        ${TOP_DIR}/ace/npuruntime/inc/runtime/
        ${TOP_DIR}/ace/npuruntime/inc/external/runtime
        ${TOP_DIR}/ace/npuruntime/acl/inc/external/
        ${TOP_DIR}/ace/npuruntime/acl/inc/external/acl
        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/aicpu
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/abl/qos
        ${TOP_DIR}/metadef/inc/external
        ${TOP_DIR}/metadef/pkg_inc
)

target_compile_definitions(hccl_v2_utest PRIVATE
    private=public
    protected=public

    ORION_AICPU_DEPLOY
    ORION_HOST_DEPLOY
)

add_subdirectory(all_source_code)
add_subdirectory(common)
add_subdirectory(service)
add_subdirectory(framework)
add_subdirectory(aicpu)
add_subdirectory(unified_platform)

run_llt_test(
    TARGET hccl_v2_utest
    TASK_NUM 1
)