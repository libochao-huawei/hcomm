# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16.0)
option(BUILD_OPEN_PROJECT "Build open hcomm project." ON)
message(STATUS "DEVICE_MODE=${DEVICE_MODE}")
message(STATUS "BUILD_OPEN_PROJECT=${BUILD_OPEN_PROJECT}")
message(STATUS "AARCH_MODE=${AARCH_MODE}")

find_program(CCAHE_PROGRAM ccache)
if(CCAHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCAHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCAHE_PROGRAM})
endif()

if((BUILD_OPEN_PROJECT AND DEVICE_MODE) OR AARCH_MODE)
    # need to switch
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    #Change the path to the absolute path of the cross compilation tool after the toolkit package is decompressed
    # set cross compile toolchain dir
    set(TOOLCHAIN_DIR ${CUSTOM_ASCEND_CANN_PACKAGE_PATH}/toolkit/toolchain/hcc/)
    message(STATUS "TOOLCHAIN_DIR: ${TOOLCHAIN_DIR}")
    if (NOT "x${TOOLCHAIN_DIR}" STREQUAL "x")
        if (NOT IS_DIRECTORY ${TOOLCHAIN_DIR})
            message(FATAL_ERROR "specify cross compile toolchain directory(${TOOLCHAIN_DIR}) is not exist")
        endif()
    endif()
    # endif()
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/aarch64-target-linux-gnu-g++)
    set(CMAKE_C_COMPILER   ${TOOLCHAIN_DIR}/bin/aarch64-target-linux-gnu-gcc)
endif()

# 获取host架构
execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE SYSTEM_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "当前系统架构: ${SYSTEM_ARCH}")
project(hcomm)

# 获取 CPU 核心数
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    execute_process(
        COMMAND nproc
        OUTPUT_VARIABLE CPU_CORES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    execute_process(
        COMMAND wmic cpu get NumberOfCores /value
        OUTPUT_VARIABLE CPU_CORES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # 解析输出
    string(REGEX REPLACE "NumberOfCores=([0-9]+)" "\\1" CPU_CORES "${CPU_CORES}")
else()
    # 默认设置为 1
    set(CPU_CORES 1)
endif()

math(EXPR JOBS "${CPU_CORES} * 2")
message(STATUS "Detected JOBS: ${JOBS}")
set(CMAKE_JOB_POOL_COMPILE "${JOBS}")
set(CMAKE_JOB_POOL_LINK "${JOBS}")

if(DEVICE_MODE)
    set(HCCP_TAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_device/tar_pkg/)
    set(CCL_KERNEL_TAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_device/ccl_kernel_tar_pkg/)
    set(HCCD_TAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_hccd/hccd_tar_pkg/)

    add_custom_target(generate_device_hccp_package
        COMMAND cd ${HCCP_TAR_DIR} && ls && tar czf ../hcomm_device.tar.gz *.so *.bin hccp_compat_bin_hash.cfg
        DEPENDS hccp_service.bin
        COMMENT "Creating hcomm_device.tar.gz"
    )
    add_dependencies(generate_device_hccp_package rs)

    add_custom_target(generate_device_hccd_package
        COMMAND cd ${HCCD_TAR_DIR} && ls && tar czf ../hcomm_hccd.tar.gz lib64
        DEPENDS hccd
        COMMENT "Creating hcomm_hccd.tar.gz"
    )
    add_dependencies(generate_device_hccd_package hccd)

    if(KERNEL_MODE)
        add_custom_target(generate_device_aicpu_package
            COMMAND cd ${CCL_KERNEL_TAR_DIR} && ls && tar czf ../aicpu_hcomm.tar.gz aicpu_kernels_device
            DEPENDS ccl_kernel
            COMMENT "Creating aicpu_hcomm.tar.gz"
        )
        add_dependencies(generate_device_aicpu_package ccl_kernel)
    endif()
    message(STATUS "Detected ENABLE_SIGN: ${ENABLE_SIGN}")
    if(ENABLE_SIGN)
        message(STATUS "Detected CUSTOM_SIGN_SCRIPT: ${CUSTOM_SIGN_SCRIPT}")
        add_custom_target(sign_hcomm_device
            COMMAND bash ${CUSTOM_SIGN_SCRIPT} ${HCCP_TAR_DIR}/../hcomm_device.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml
            DEPENDS generate_device_hccp_package
            COMMENT "Sign hcomm_device.tar.gz"
        )
        add_dependencies(sign_hcomm_device generate_device_hccp_package)
        add_custom_target(sign_hcomm_hccd
            COMMAND bash ${CUSTOM_SIGN_SCRIPT} ${HCCD_TAR_DIR}/../hcomm_hccd.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml
            DEPENDS generate_device_hccd_package
            COMMENT "Sign hcomm_hccd.tar.gz"
        )
        add_dependencies(sign_hcomm_hccd generate_device_hccd_package)
        if(KERNEL_MODE)
            add_custom_target(sign_aicpu_hcomm
                COMMAND bash ${CUSTOM_SIGN_SCRIPT} ${CCL_KERNEL_TAR_DIR}/../aicpu_hcomm.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml
                DEPENDS generate_device_aicpu_package
                COMMENT "Sign aicpu_hcomm.tar.gz"
            )
            add_dependencies(sign_aicpu_hcomm generate_device_aicpu_package)
        endif()
    endif()
endif()

if(BUILD_OPEN_PROJECT)
    include(cmake/third_party/openssl.cmake)
    include(cmake/third_party/json.cmake)
    include(cmake/utils.cmake)
    include(cmake/config.cmake)
    include(cmake/package.cmake)
    pack_built_in()
    add_subdirectory(src)
    add_subdirectory(python)

    if(DEFINED ENABLE_TEST)
        add_subdirectory(test)
    endif()

    set(HCCL_HEAD
        inc/hccl/hccl.h
        inc/hccl/hccl_types.h
        inc/hccl/hccl_api.h
        inc/hccl/hccl_ctrl_plane.h
        inc/hccl/hccl_ops.h
        src/pub_inc/hccn_rping.h
        inc/hccl/hccl_api_data.h
        inc/hccl/hccl_mem_defs.h
        inc/hccl/hccl_rank_graph.h
    )

    set(HCCL_JSON
        cmake/scripts/ccl_kernel.json
    )
    if (NOT DEVICE_MODE)
        message(STATUS "Detected INSTALL_LIBRARY_DIR: ${INSTALL_LIBRARY_DIR}")

        set(BUILD_DEVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_device/)
        set(BUILD_HCCD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_hccd/)
        add_custom_target(install_tar DEPENDS
            ${BUILD_DEVICE_DIR}/hcomm_device.tar.gz
            ${BUILD_DEVICE_DIR}/aicpu_hcomm.tar.gz
            ${BUILD_HCCD_DIR}/hcomm_hccd.tar.gz
            ${BUILD_DEVICE_DIR}/aicpu_custom/libaicpu_custom.so
        )
        install(FILES ${BUILD_DEVICE_DIR}/hcomm_device.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/aicpu_hcomm.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
        )
        install(FILES ${BUILD_HCCD_DIR}/hcomm_hccd.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/aicpu_custom/libaicpu_custom.so
            DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR} OPTIONAL
        )
        add_dependencies(install_tar hccl_fwk)
    endif()

    install(FILES
        ${HCCL_HEAD}
        ${HCCL_JSON}
        DESTINATION ${INSTALL_INCLUDE_DIR}/hccl/ OPTIONAL
    )

    set(HCCL_PKG_HEAD
        pkg_inc/hccl/base.h
        pkg_inc/hccl/hccl_ex.h
        pkg_inc/hccl/hccl_one_sided_services.h
        pkg_inc/hccl/hcom.h
        pkg_inc/hccl/dtype_common.h
        pkg_inc/hccl/workflow.h
    )
    install(FILES
        ${HCCL_PKG_HEAD}
        DESTINATION ${INSTALL_PKG_INCLUDE_DIR}/hccl/ OPTIONAL
    )
endif()

find_program(SYSTEM_CCACHE_PROGRAM ccache)
if (SYSTEM_CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${SYSTEM_CCACHE_PROGRAM} CACHE PATH "cache Compiler")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${SYSTEM_CCACHE_PROGRAM} CACHE PATH "cache Compiler")
endif()
