set(CMAKE_VERBOSE_MAKEFILE on)

if(BUILD_OPEN_PROJECT)
    set(HCCL_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    set(HCCL_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../inc/hccl/hccl)
    message(STATUS "open HCCL_INC_DIR = ${HCCL_INC_DIR}")
    set(HCCL_OPEN_CODE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../)
    set(HCCL_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../../../)
else()
    message(STATUS "close TOP_DIR = ${TOP_DIR}")
    set(HCCL_BASE_DIR ${TOP_DIR}/hcomm/src/)
    set(HCCL_OPEN_CODE_BASE_DIR ${TOP_DIR}/hcomm/src/)
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
        ${HCCL_BASE_DIR}/../pkg_inc
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/hccl_tbe_task
        ${CMAKE_CURRENT_SOURCE_DIR}/graph_ctx_mgr
        ${CMAKE_CURRENT_SOURCE_DIR}/common

        # runtime头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/include/base/
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
        ${ASCEND_CANN_PACKAGE_PATH}/include/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/include/external/
        ${ASCEND_CANN_PACKAGE_PATH}/include/platform/

        # driver头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/

        # mmpa头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/include/mmpa/

        # 包间接口
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/

        # metadef头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        ${ASCEND_CANN_PACKAGE_PATH}/include/external/

        # 第三方头文件
        ${THIRD_PARTY_NLOHMANN_PATH}
    )
    target_link_directories(hccl_legacy PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/lib64
    )
else()
    set(tbe_include_list
        ${HCCL_BASE_DIR}/../inc
        ${HCCL_BASE_DIR}/../inc/hccl/
        ${HCCL_BASE_DIR}/../inc/hccl/hccl
        ${HCCL_BASE_DIR}/../pkg_inc

        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/runtime/pkg_inc/
        ${TOP_DIR}/runtime/pkg_inc/runtime/
        ${TOP_DIR}/runtime/pkg_inc/profiling/
        ${TOP_DIR}/runtime/pkg_inc/trace/
        ${TOP_DIR}/runtime/pkg_inc/base/
        ${TOP_DIR}/runtime/pkg_inc/aicpu/
        ${TOP_DIR}/runtime/include/external/
        ${TOP_DIR}/runtime/include/driver/
        ${TOP_DIR}/runtime/src/runtime/platform/inc/
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
    $<$<CONFIG:Debug>:-g>
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
        acl_rt
        metadef
        opp_registry
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
        -Wl,--no-as-needed
        c_sec
        unified_dlog
        mmpa
        runtime
        acl_rt
        metadef
        opp_registry
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )
endif()

add_subdirectory(common)
add_subdirectory(graph_ctx_mgr)
add_subdirectory(hccl_tbe_task)


install(TARGETS  hccl_legacy
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}  OPTIONAL
)