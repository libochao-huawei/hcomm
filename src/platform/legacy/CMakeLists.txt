set(CMAKE_VERBOSE_MAKEFILE on)

if(BUILD_OPEN_PROJECT)
    set(HCCL_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    set(HCCL_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../inc/hccl/hccl)
    message(STATUS "open HCCL_INC_DIR = ${HCCL_INC_DIR}")
    set(HCCL_OPEN_CODE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    set(HCCL_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../../../)
else()
    message(STATUS "close TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${TOP_DIR}/hccl/src/)
    set(HCCL_OPEN_CODE_BASE_DIR ${TOP_DIR}/hccl/src/)
    set(HCCL_THIRD_PARTY_DIR ${TOP_DIR})
endif()

add_library(hccl_legacy  SHARED)
 
#头文件搜索路径
if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hccl_legacy PRIVATE
    OPEN_BUILD_PROJECT
    LOG_CPP)

    target_include_directories(hccl_legacy PRIVATE
        ${HCCL_BASE_DIR}/../inc
        ${HCCL_BASE_DIR}/../inc/hccl/
        ${HCCL_BASE_DIR}/../inc/hccl/hccl
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/hccl_tbe_task
        ${CMAKE_CURRENT_SOURCE_DIR}/graph_ctx_mgr
        ${CMAKE_CURRENT_SOURCE_DIR}/common

        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/c_sec/include/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/ascendcl/external/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/external/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/metadef/external/register/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/mmpa/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/msprof/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/platform/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/runtime/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/runtime/runtime/
        ${ASCEND_SDK_PACKAGE_PATH}/opensdk/opensdk/include/slog/toolchain/
        ${ASCEND_SDK_PACKAGE_PATH}/runtime/include/ # external/graph/operator.h
        ${ASCEND_SDK_PACKAGE_PATH}/runtime/include/external
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc

        ${THIRD_PARTY_NLOHMANN_PATH}

        ${HCCL_BASE_DIR}/../externel_depends/driver/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc
        ${ASCEND_CANN_PACKAGE_PATH}/include/acl/error_codes/

        # 缺runtime包临时引用放置在extenel_depends中的头文件
        ${HCCL_BASE_DIR}/../externel_depends/c_sec/include/
        ${HCCL_BASE_DIR}/../externel_depends/include/metadef/
        ${HCCL_BASE_DIR}/../externel_depends/include/metadef/external/
        ${HCCL_BASE_DIR}/../externel_depends/include/metadef/external/register/
        ${HCCL_BASE_DIR}/../externel_depends/include/mmpa/
        ${HCCL_BASE_DIR}/../externel_depends/include/msprof/
        ${HCCL_BASE_DIR}/../externel_depends/include/platform/
        ${HCCL_BASE_DIR}/../externel_depends/include/runtime/
        ${HCCL_BASE_DIR}/../externel_depends/include/runtime/runtime/
    )
    target_link_directories(hccl_legacy PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
else()
    set(tbe_include_list
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/ace/npuruntime/runtime/platform/inc
        ${TOP_DIR}/air/inc/graph_metadef/
        ${TOP_DIR}/air/inc/graph_metadef/external
        ${TOP_DIR}/air/inc/graph_metadef/external/register/
        ${TOP_DIR}/metadef/inc/external/
        ${TOP_DIR}/metadef/inc/
        ${TOP_DIR}/metadef/pkg_inc
        ${TOP_DIR}/metadef/inc/external/exe_graph/runtime/
        ${TOP_DIR}/metadef/inc/external/graph/
        ${TOP_DIR}/open_source/json/include
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/hccl_tbe_task
        ${CMAKE_CURRENT_SOURCE_DIR}/graph_ctx_mgr
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${TOP_DIR}/abl/msprof/inc/
    )
    target_include_directories(hccl_legacy PRIVATE
        ${tbe_include_list}
    )
endif()
 
target_compile_options(hccl_legacy PRIVATE
    -Werror
    -fno-common
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
    -fstack-protector-all
)
 
target_link_options(hccl_legacy PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    -Wl,--build-id=none
)
 
if(BUILD_OPEN_PROJECT)
    target_link_libraries(hccl_legacy
    PRIVATE
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        mmpa
        runtime
        ascendcl
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    PUBLIC
        hccl_headers
    )
else()
    target_link_libraries(hccl_legacy
    PRIVATE
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:hccl_headers>
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        mmpa
        runtime
        ascendcl
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    PUBLIC
        hccl_headers
    )
endif()

add_subdirectory(common)
add_subdirectory(graph_ctx_mgr)
add_subdirectory(hccl_tbe_task)


install(TARGETS  hccl_legacy
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)