# 指定算法库动态库target
add_library(hccl_v2_native_alg_repo SHARED ""
)
 
# 指定算法框架target
add_library(hccl_v2_alg_frame SHARED ""
)
 
# 自动找到项目内所有头文件
FILE(GLOB_RECURSE sources_list LIST_DIRECTORIES TRUE ${HCCL_V2_CODE_ROOT})
 
foreach (dir ${sources_list})
    IF ((IS_DIRECTORY ${dir}) AND (${dir} MATCHES "src/legacy"))
        target_include_directories(hccl_v2_native_alg_repo PRIVATE ${dir})
        target_include_directories(hccl_v2_alg_frame PRIVATE ${dir})
    endif ()
endforeach ()

add_subdirectory(coll_alg_factory)
add_subdirectory(alg_registry)
add_subdirectory(interface)
add_subdirectory(selector)

set(HCCL_BASE_DIR ${HCOMM_REAL_PATH})
 
# 添加项目外部头文件
target_compile_definitions(hccl_v2_native_alg_repo PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)

target_compile_definitions(hccl_v2_alg_frame PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:_GLIBCXX_USE_CXX11_ABI=0>
)
 
target_include_directories(hccl_v2_native_alg_repo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/base/inc
    ${HCCL_BASE_DIR}/src/algorithm/base/inc
    ${HCCL_BASE_DIR}/src/algorithm/pub_inc
    ${HCCL_BASE_DIR}/src/pub_inc
    ${HCCL_BASE_DIR}/src/pub_inc/hccl
    ${HCCL_BASE_DIR}/inc
    ${HCCL_BASE_DIR}/inc/adapter
    ${HCCL_BASE_DIR}/src/algorithm/impl/resource_manager
    ${HCCL_BASE_DIR}/src/platform/hccp/inc/network
    ${RDMA_CORE_INCLUDE_DIR}

    ${HCCL_BASE_DIR}/pkg_inc
    ${HCCL_BASE_DIR}/include
    ${HCCL_BASE_DIR}/include/hccl/
    ${HCCL_BASE_DIR}/inc/external/
    ${HCCL_BASE_DIR}/inc/external/hccl
    ${HCCL_BASE_DIR}/src/algorithm/pub_inc
    ${HCCL_BASE_DIR}/src/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/src/algorithm/base/communicator
    ${HCCL_BASE_DIR}/src/legacy/service/collective/alg/coll_alg_factory/alg_aiv_instruction

    ${THIRD_PARTY_NLOHMANN_PATH}
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/
    ${ASCEND_CANN_PACKAGE_PATH}/include/acl/
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/

    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/runtime
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
)

target_include_directories(hccl_v2_alg_frame PRIVATE
    ${HCCL_BASE_DIR}/pkg_inc
    ${HCCL_BASE_DIR}/include
    ${HCCL_BASE_DIR}/include/hccl/
    ${HCCL_BASE_DIR}/inc/external/
    ${HCCL_BASE_DIR}/inc/external/hccl
    ${HCCL_BASE_DIR}/src/algorithm/pub_inc
    ${HCCL_BASE_DIR}/src/algorithm/base/alg_template
    ${HCCL_BASE_DIR}/src/hccl/algorithm/base/communicator
    ${HCCL_BASE_DIR}/src/legacy/service/collective/alg/coll_alg_factory/alg_aiv_instruction
    ${HCCL_BASE_DIR}/src/platform/hccp/inc/network
    ${RDMA_CORE_INCLUDE_DIR}

    ${THIRD_PARTY_NLOHMANN_PATH}
    ${ASCEND_CANN_PACKAGE_PATH}/include/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/
    ${ASCEND_CANN_PACKAGE_PATH}/include/acl/
    ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/highlevel_api/
    ${ASCEND_CANN_PACKAGE_PATH}/include/ascendc/

    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/asc/hccl/internal/
)

target_compile_options(hccl_v2_native_alg_repo PRIVATE
        -std=c++14
        -Werror
        -Wall
        -Wextra
        -Wfloat-equal
        -Wno-error=unused-parameter
        -Wno-error=sign-compare
        -fno-common
        -fno-strict-aliasing
        -pipe
        -O3
        -fstack-protector-all
)
 
target_compile_options(hccl_v2_alg_frame PRIVATE
        -std=c++14
        -Wall
        -Wextra
        -Wfloat-equal
        -Werror
        -Wno-error=unused-parameter
        -Wno-error=sign-compare
        -fno-common
        -fno-strict-aliasing
        -pipe
        -O3
        -fstack-protector-all
)
 
target_link_options(hccl_v2_native_alg_repo PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    $<$<CONFIG:Release>:-s>
)
 
target_link_options(hccl_v2_alg_frame PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    $<$<CONFIG:Release>:-s>
)
 
target_link_libraries(hccl_v2_native_alg_repo PRIVATE
        -Wl,--no-as-needed
        unified_dlog
        hccl_headers
)
 
target_link_libraries(hccl_v2_alg_frame PRIVATE
        -Wl,--no-as-needed
        hccl_v2_native_alg_repo
        unified_dlog
        hccl_headers
)
 
# install_package(
#     PACKAGE hccl_v2_native_alg_repo
#     TARGETS hccl_v2_alg_frame hccl_v2_native_alg_repo
#     FILES   ${TOP_DIR}/ace/comop/inc/hccl/base.h
#             ${TOP_DIR}/ace/comop/inc/hccl/log.h
#     DESTINATION ${INSTALL_INCLUDE_DIR}
# )
 
# install_package(
#     PACKAGE hccl_v2_alg_frame
#     TARGETS hccl_v2_alg_frame
#     FILES   ${TOP_DIR}/ace/comop/inc/hccl/base.h
#             ${TOP_DIR}/ace/comop/inc/hccl/log.h
#     DESTINATION ${INSTALL_INCLUDE_DIR}
# )