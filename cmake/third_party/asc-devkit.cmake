# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

foreach(pkg
        acl_rt
        aicpu_sharder
        ascend_hal
        atrace
        error_manager
        metadef
        mmpa
        runtime
        securec
        unified_dlog)
    if(NOT TARGET ${pkg})
        find_cann_package(${pkg} MODULE REQUIRED)
    endif()
endforeach()

if(NOT TARGET tilingdata_base_objs)
    find_cann_package(tilingdata_base MODULE REQUIRED OBJECTS)
endif()

if(NOT TARGET json)
    add_cann_third_party(json)
endif()

set(ASC_DEVKIT_TAG_ID master CACHE STRING "asc-devkit git ref for mc2 components")
set(ASC_DEVKIT_COPY_FILE_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/copy_file.cmake)

if(EXISTS "${PROJECT_SOURCE_DIR}/../asc/asc-devkit")
    get_filename_component(ASC_DEVKIT_SOURCE_PATH
        "${PROJECT_SOURCE_DIR}/../asc/asc-devkit" REALPATH)
    message(STATUS "[ThirdPartyLib][asc-devkit] Find source dir: ${ASC_DEVKIT_SOURCE_PATH}")
elseif(EXISTS "${CANN_3RD_LIB_PATH}/asc-devkit")
    get_filename_component(ASC_DEVKIT_SOURCE_PATH
        "${CANN_3RD_LIB_PATH}/asc-devkit" REALPATH)
    message(STATUS "[ThirdPartyLib][asc-devkit] Find source dir: ${ASC_DEVKIT_SOURCE_PATH}")

    execute_process(
        COMMAND git fetch origin ${ASC_DEVKIT_TAG_ID} --depth=1
        WORKING_DIRECTORY ${ASC_DEVKIT_SOURCE_PATH}
        TIMEOUT 20
        RESULT_VARIABLE FETCH_RESULT
        ERROR_VARIABLE FETCH_ERROR
        OUTPUT_QUIET
    )

    if(FETCH_RESULT EQUAL 0)
        set(CHECKOUT_REF FETCH_HEAD)
        message(STATUS "[ThirdPartyLib][asc-devkit] fetched ${ASC_DEVKIT_TAG_ID}")
    else()
        set(CHECKOUT_REF ${ASC_DEVKIT_TAG_ID})
        message(WARNING "[ThirdPartyLib][asc-devkit] Git fetch failed, checkout cached ${ASC_DEVKIT_TAG_ID}: ${FETCH_ERROR}")
    endif()

    execute_process(
        COMMAND git checkout ${CHECKOUT_REF}
        WORKING_DIRECTORY ${ASC_DEVKIT_SOURCE_PATH}
        RESULT_VARIABLE CHECKOUT_RESULT
        ERROR_VARIABLE CHECKOUT_ERROR
        OUTPUT_QUIET
    )
    if(NOT CHECKOUT_RESULT EQUAL 0)
        message(FATAL_ERROR "[ThirdPartyLib][asc-devkit] Git checkout failed: ${CHECKOUT_ERROR}")
    endif()
else()
    execute_process(
        COMMAND git remote get-url origin
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REMOTE_URL
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_REMOTE_URL MATCHES "^git@|^ssh://")
        set(ASC_DEVKIT_GIT_URL "git@gitcode.com:cann/asc-devkit.git")
        message(STATUS "[ThirdPartyLib][asc-devkit] using SSH protocol: ${ASC_DEVKIT_GIT_URL}")
    else()
        set(ASC_DEVKIT_GIT_URL "https://gitcode.com/cann/asc-devkit.git")
        message(STATUS "[ThirdPartyLib][asc-devkit] using HTTPS protocol: ${ASC_DEVKIT_GIT_URL}")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        asc-devkit
        GIT_REPOSITORY ${ASC_DEVKIT_GIT_URL}
        GIT_TAG ${ASC_DEVKIT_TAG_ID}
        GIT_PROGRESS TRUE
        SOURCE_DIR ${CANN_3RD_LIB_PATH}/asc-devkit
    )
    FetchContent_Populate(asc-devkit)
    set(ASC_DEVKIT_SOURCE_PATH ${CANN_3RD_LIB_PATH}/asc-devkit)
endif()

set(ASC_DEVKIT_MC2_HOST_BUILD_DIR ${CMAKE_BINARY_DIR}/asc-devkit-mc2-host-build)
set(ASC_DEVKIT_MC2_HOST_INSTALL_DIR ${CMAKE_BINARY_DIR}/asc-devkit-mc2-host-install)
set(ASC_DEVKIT_MC2_DEVICE_BUILD_DIR ${CMAKE_BINARY_DIR}/asc-devkit-mc2-device-build)
set(ASC_DEVKIT_MC2_DEVICE_INSTALL_DIR ${CMAKE_BINARY_DIR}/asc-devkit-mc2-device-install)
set(ASC_DEVKIT_MC2_CLIENT_FILE
    ${ASC_DEVKIT_MC2_HOST_INSTALL_DIR}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64/libmc2_client.so)
set(ASC_DEVKIT_MC2_COMPAT_FILE
    ${ASC_DEVKIT_MC2_HOST_INSTALL_DIR}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64/libmc2_compat.so)
set(ASC_DEVKIT_MC2_SERVER_JSON_FILE
    ${ASC_DEVKIT_MC2_HOST_INSTALL_DIR}/opp/built-in/op_impl/aicpu/config/libmc2_server.json)
set(ASC_DEVKIT_MC2_SERVER_TAR_FILE
    ${ASC_DEVKIT_MC2_DEVICE_INSTALL_DIR}/opp/built-in/op_impl/aicpu/kernel/mc2_server.tar.gz)
set(ASC_DEVKIT_MC2_VARS
    ASCENDC_DIR
    ASCENDC_INCLUDE_DIR
    ASCENDC_IMPL_DIR
    ASC_DEVKIT_DIR
    DEVICE_MODE
    KERNEL_MODE
    PRODUCT_SIDE
    PACKAGE_OPEN_PROJECT
    ENABLE_BUILD_DEVICE
    ENABLE_TEST
)

foreach(var IN LISTS ASC_DEVKIT_MC2_VARS)
    if(DEFINED ${var})
        set(ASC_DEVKIT_SAVED_${var}_DEFINED TRUE)
        set(ASC_DEVKIT_SAVED_${var} "${${var}}")
    else()
        set(ASC_DEVKIT_SAVED_${var}_DEFINED FALSE)
    endif()
endforeach()

set(ASCENDC_DIR ${ASC_DEVKIT_SOURCE_PATH})
set(ASCENDC_INCLUDE_DIR ${ASCENDC_DIR}/include)
set(ASCENDC_IMPL_DIR ${ASCENDC_DIR}/impl)
set(ASC_DEVKIT_DIR ${ASC_DEVKIT_SOURCE_PATH})
set(PACKAGE_OPEN_PROJECT OFF)
set(ENABLE_BUILD_DEVICE OFF)
set(ENABLE_TEST OFF)

set(DEVICE_MODE FALSE)
set(KERNEL_MODE OFF)
set(PRODUCT_SIDE host)
add_subdirectory(
    ${ASC_DEVKIT_SOURCE_PATH}/impl/adv_api/detail/hccl/cc
    ${ASC_DEVKIT_MC2_HOST_BUILD_DIR}
)

add_custom_command(
    OUTPUT
        ${ASC_DEVKIT_MC2_CLIENT_FILE}
        ${ASC_DEVKIT_MC2_COMPAT_FILE}
        ${ASC_DEVKIT_MC2_SERVER_JSON_FILE}
    COMMAND ${CMAKE_COMMAND}
        -DMC2_CLIENT_INPUT=$<TARGET_FILE:mc2_client>
        -DMC2_CLIENT_OUTPUT=${ASC_DEVKIT_MC2_CLIENT_FILE}
        -DMC2_COMPAT_INPUT=$<TARGET_FILE:mc2_compat>
        -DMC2_COMPAT_OUTPUT=${ASC_DEVKIT_MC2_COMPAT_FILE}
        -DMC2_SERVER_JSON_INPUT=${ASC_DEVKIT_MC2_HOST_BUILD_DIR}/src/libmc2_server.json
        -DMC2_SERVER_JSON_OUTPUT=${ASC_DEVKIT_MC2_SERVER_JSON_FILE}
        -P ${ASC_DEVKIT_COPY_FILE_SCRIPT}
    DEPENDS mc2_client mc2_compat aicpu_kernel_json
    VERBATIM
)
add_custom_target(asc_devkit_mc2_client ALL
    DEPENDS
        ${ASC_DEVKIT_MC2_CLIENT_FILE}
        ${ASC_DEVKIT_MC2_COMPAT_FILE}
        ${ASC_DEVKIT_MC2_SERVER_JSON_FILE}
)

set(DEVICE_MODE TRUE)
set(KERNEL_MODE TRUE)
set(PRODUCT_SIDE device)
include(${ASC_DEVKIT_SOURCE_PATH}/impl/adv_api/gen_kernel_tiling.cmake)
add_subdirectory(
    ${ASC_DEVKIT_SOURCE_PATH}/impl/utils
    ${ASC_DEVKIT_MC2_DEVICE_BUILD_DIR}/impl/utils
)
add_subdirectory(
    ${ASC_DEVKIT_SOURCE_PATH}/impl/aicpu_api
    ${ASC_DEVKIT_MC2_DEVICE_BUILD_DIR}/impl/aicpu_api
)
add_subdirectory(
    ${ASC_DEVKIT_SOURCE_PATH}/impl/adv_api/tiling
    ${ASC_DEVKIT_MC2_DEVICE_BUILD_DIR}/impl/adv_api/tiling
)
add_subdirectory(
    ${ASC_DEVKIT_SOURCE_PATH}/impl/adv_api/detail/hccl/cc
    ${ASC_DEVKIT_MC2_DEVICE_BUILD_DIR}/impl/adv_api/detail/hccl/cc
)

add_custom_command(
    OUTPUT ${ASC_DEVKIT_MC2_SERVER_TAR_FILE}
    COMMAND ${CMAKE_COMMAND}
        -DMC2_SERVER_TAR_INPUT=${CMAKE_BINARY_DIR}/mc2_server.tar.gz
        -DMC2_SERVER_TAR_OUTPUT=${ASC_DEVKIT_MC2_SERVER_TAR_FILE}
        -P ${ASC_DEVKIT_COPY_FILE_SCRIPT}
    DEPENDS generate_device_aicpu_package
    VERBATIM
)
add_custom_target(asc_devkit_mc2_server ALL
    DEPENDS ${ASC_DEVKIT_MC2_SERVER_TAR_FILE}
)

foreach(var IN LISTS ASC_DEVKIT_MC2_VARS)
    if(ASC_DEVKIT_SAVED_${var}_DEFINED)
        set(${var} "${ASC_DEVKIT_SAVED_${var}}")
    else()
        unset(${var})
    endif()
endforeach()

install(FILES ${ASC_DEVKIT_MC2_CLIENT_FILE}
    DESTINATION ${INSTALL_LIBRARY_DIR}
    COMPONENT hcomm
)

install(FILES ${ASC_DEVKIT_MC2_COMPAT_FILE}
    DESTINATION ${INSTALL_LIBRARY_DIR}
    COMPONENT hcomm
)

install(FILES ${ASC_DEVKIT_MC2_SERVER_TAR_FILE}
    DESTINATION opp/built-in/op_impl/aicpu/kernel
    COMPONENT hcomm
)

install(FILES ${ASC_DEVKIT_MC2_SERVER_JSON_FILE}
    DESTINATION opp/built-in/op_impl/aicpu/config
    COMPONENT hcomm
)
