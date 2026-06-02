# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

include(ExternalProject)

set(ASC_DEVKIT_TAG_ID master CACHE STRING "asc-devkit git ref for mc2 components")
set(ASC_DEVKIT_COPY_FILE_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/copy_file.cmake)

if(EXISTS "${PROJECT_SOURCE_DIR}/../asc-devkit")
    get_filename_component(ASC_DEVKIT_SOURCE_PATH
        "${PROJECT_SOURCE_DIR}/../asc-devkit" REALPATH)
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
if(NOT JOBS)
    set(JOBS 8)
endif()

set(ASC_DEVKIT_MC2_HOST_CMAKE_ARGS
    -DASCEND_CANN_PACKAGE_PATH=${ASCEND_CANN_PACKAGE_PATH}
    -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_OPEN_PROJECT=ON
    -DENABLE_TEST=OFF
    -DENABLE_BUILD_DEVICE=OFF
    -DPACKAGE_OPEN_PROJECT=OFF
    -DKERNEL_MODE=OFF
    -DPRODUCT_SIDE=host
)

foreach(var CMAKE_SYSTEM_NAME CMAKE_SYSTEM_PROCESSOR CMAKE_C_COMPILER CMAKE_CXX_COMPILER
            CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)
    if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
        list(APPEND ASC_DEVKIT_MC2_HOST_CMAKE_ARGS -D${var}=${${var}})
    endif()
endforeach()

ExternalProject_Add(asc_devkit_mc2_client
    SOURCE_DIR ${ASC_DEVKIT_SOURCE_PATH}
    BINARY_DIR ${ASC_DEVKIT_MC2_HOST_BUILD_DIR}
    CMAKE_ARGS ${ASC_DEVKIT_MC2_HOST_CMAKE_ARGS}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target mc2_client --parallel ${JOBS}
    INSTALL_COMMAND ${CMAKE_COMMAND}
        -DMC2_CLIENT_INPUT=<BINARY_DIR>/impl/adv_api/detail/hccl/cc/src/libmc2_client.so
        -DMC2_CLIENT_OUTPUT=${ASC_DEVKIT_MC2_CLIENT_FILE}
        -DMC2_COMPAT_INPUT=<BINARY_DIR>/impl/adv_api/detail/hccl/cc/src/common/hcomm_dlsym/libmc2_compat.so
        -DMC2_COMPAT_OUTPUT=${ASC_DEVKIT_MC2_COMPAT_FILE}
        -DMC2_SERVER_JSON_INPUT=<BINARY_DIR>/impl/adv_api/detail/hccl/cc/src/libmc2_server.json
        -DMC2_SERVER_JSON_OUTPUT=${ASC_DEVKIT_MC2_SERVER_JSON_FILE}
        -P ${ASC_DEVKIT_COPY_FILE_SCRIPT}
    BUILD_ALWAYS TRUE
)

set(ASC_DEVKIT_MC2_DEVICE_CMAKE_ARGS
    -DTOOLCHAIN_DIR=${HCC_TOOLCHAIN_DIR}
    -DCMAKE_TOOLCHAIN_FILE=${HCOMM_DIR}/cmake/aarch64-hcc-toolchain.cmake
    -DCMAKE_C_COMPILER=${HCC_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${HCC_CXX_COMPILER}
    -DCMAKE_C_AR=${HCC_C_AR}
    -DASCEND_CANN_PACKAGE_PATH=${ASCEND_CANN_PACKAGE_PATH}
    -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_OPEN_PROJECT=ON
    -DENABLE_TEST=OFF
    -DENABLE_SIGN=${ENABLE_SIGN}
    -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT}
    -DVERSION_INFO=${VERSION_INFO}
)

foreach(var CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER)
    if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
        list(APPEND ASC_DEVKIT_MC2_DEVICE_CMAKE_ARGS -D${var}=${${var}})
    endif()
endforeach()

ExternalProject_Add(asc_devkit_mc2_server
    SOURCE_DIR ${ASC_DEVKIT_SOURCE_PATH}/cmake/device
    BINARY_DIR ${ASC_DEVKIT_MC2_DEVICE_BUILD_DIR}
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env TOOLCHAIN_DIR=${HCC_TOOLCHAIN_DIR}
        ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> ${ASC_DEVKIT_MC2_DEVICE_CMAKE_ARGS}
    BUILD_COMMAND ${CMAKE_COMMAND} -E env TOOLCHAIN_DIR=${HCC_TOOLCHAIN_DIR}
        ${CMAKE_COMMAND} --build <BINARY_DIR> --target generate_device_aicpu_package --parallel ${JOBS}
    INSTALL_COMMAND ${CMAKE_COMMAND}
        -DMC2_SERVER_TAR_INPUT=<BINARY_DIR>/mc2_server.tar.gz
        -DMC2_SERVER_TAR_OUTPUT=${ASC_DEVKIT_MC2_SERVER_TAR_FILE}
        -P ${ASC_DEVKIT_COPY_FILE_SCRIPT}
    BUILD_ALWAYS TRUE
)

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
