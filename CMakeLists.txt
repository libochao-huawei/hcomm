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
message(STATUS "BUILD_OPEN_PROJECT=${BUILD_OPEN_PROJECT}")
message(STATUS "DEVICE_MODE=${DEVICE_MODE}")
message(STATUS "AARCH_MODE=${AARCH_MODE}")

set(HCOMM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HCC_TOOLCHAIN_DIR "${CUSTOM_ASCEND_CANN_PACKAGE_PATH}/toolkit/toolchain/hcc/")
set(HCC_CXX_COMPILER  "${HCC_TOOLCHAIN_DIR}/bin/aarch64-target-linux-gnu-g++")
set(HCC_C_COMPILER    "${HCC_TOOLCHAIN_DIR}/bin/aarch64-target-linux-gnu-gcc")

# 交叉编译场景（x86机器上编译arm包）
if(AARCH_MODE)
    message(STATUS "TOOLCHAIN_DIR=${HCC_TOOLCHAIN_DIR}")
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(TOOLCHAIN_DIR      ${HCC_TOOLCHAIN_DIR})
    set(CMAKE_CXX_COMPILER ${HCC_CXX_COMPILER})
    set(CMAKE_C_COMPILER   ${HCC_C_COMPILER})
endif()

# 获取host架构
execute_process(
    COMMAND uname -m
    OUTPUT_VARIABLE SYSTEM_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "当前系统架构: ${SYSTEM_ARCH}")

# 引入CANN/cmake库
include(cmake/fetch_cann_cmake.cmake)

project(hcomm)

# 引入CANN/cmake库，初始化
init_cann_project()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

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

if(ENABLE_TEST)
    enable_testing()
    include(cmake/config.cmake)
    include(cmake/function.cmake)
    include(cmake/intf_pub_gccnative.cmake)
    include(cmake/third_party/json.cmake)
    include(cmake/third_party/gtest.cmake)
    include(cmake/third_party/mockcpp.cmake)
    include(cmake/third_party/rdma-core.cmake)
    add_subdirectory(test)

elseif(BUILD_OPEN_PROJECT)
    include(cmake/third_party/openssl.cmake)
    include(cmake/third_party/ubengine.cmake)
    include(cmake/third_party/rdma-core.cmake)
    include(cmake/third_party/json.cmake)
    include(cmake/hcomm_utils.cmake)
    include(cmake/config.cmake)
    include(cmake/package.cmake)
    include(cmake/func.cmake)
    include(version.cmake)
    check_pkg_build_deps("hcomm")
    add_version_info_targets()
    pack_built_in()
    add_subdirectory(src)
    add_subdirectory(python)

    # Device 侧编译构建
    if(DEVICE_MODE)
        ExternalProject_Add(hcomm_device
            SOURCE_DIR ${RUNTIME_DIR}/cmake/device
            BINARY_DIR ${CMAKE_BINARY_DIR}/device_build
            CMAKE_ARGS  
                -DKERNEL_MODE=${KERNEL_MODE}
                -DTOOLCHAIN_DIR=${HCC_TOOLCHAIN_DIR}
                -DCMAKE_TOOLCHAIN_FILE=${HCOMM_ROOT_DIR}/cmake/aarch64-hcc-toolchain.cmake
                -DCMAKE_C_COMPILER=${HCC_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${HCC_CXX_COMPILER}
                -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}
                -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/device_install
                -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH}
                -DENABLE_SIGN=${ENABLE_SIGN}
                -DCUSTOM_SIGN_SCRIPT=${CUSTOM_SIGN_SCRIPT}
                -DVERSION_INFO=${VERSION_INFO}
            INSTALL_COMMAND ${CMAKE_CPACK_COMMAND}
            BUILD_ALWAYS TRUE
        )

        # install(FILES
        #     ${CMAKE_BINARY_DIR}/device_build/device-npu-runtime.tar.gz
        #     DESTINATION .
        #     COMPONENT npu-runtime
        # )
    endif()

    set(HCCL_HEAD
        include/hccl/hccl_comm.h
        include/hccl/hccl_types.h
        src/pub_inc/hccn_rping.h
        include/hccl/hccl_rank_graph.h
        include/hccl/hccl_res.h
        include/hccl/hccl_sym_win.h
    )

    set(HCOMM_HEAD
        include/hcomm_primitives.h
    )

    set(HCCL_JSON
        cmake/scripts/ccl_kernel.json
    )

    if (NOT DEVICE_MODE)
        message(STATUS "Detected INSTALL_LIBRARY_DIR: ${INSTALL_LIBRARY_DIR}")

        set(BUILD_DEVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_device/)
        set(BUILD_HCCD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_hccd/)
        add_custom_target(install_tar DEPENDS
            ${BUILD_DEVICE_DIR}/signatures/cann-hcomm-compat.tar.gz
            ${BUILD_DEVICE_DIR}/signatures/aicpu_hcomm.tar.gz
            ${BUILD_HCCD_DIR}/signatures/cann-hccd-compat.tar.gz
            ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.so
            ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.json
        )
        install(FILES ${BUILD_DEVICE_DIR}/signatures/cann-hcomm-compat.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
            PERMISSIONS OWNER_READ GROUP_READ
            OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/signatures/aicpu_hcomm.tar.gz
            DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
        )
        install(FILES ${BUILD_HCCD_DIR}/signatures/cann-hccd-compat.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
            PERMISSIONS OWNER_READ GROUP_READ
            OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.so
            DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.json
            DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel OPTIONAL
        )

        add_custom_target(install_ccl_kernel DEPENDS
            ${BUILD_DEVICE_DIR}/src/framework/libccl_kernel.so
        )
        install(FILES ${BUILD_DEVICE_DIR}/src/framework/libccl_kernel.so
            DESTINATION ${CMAKE_SYSTEM_PROCESSOR}-linux/devlib/device OPTIONAL
        )
        add_dependencies(install_tar hcomm)
        add_dependencies(install_ccl_kernel hcomm)
    endif()

    install(FILES
        ${HCCL_HEAD}
        ${HCCL_JSON}
        DESTINATION ${INSTALL_INCLUDE_DIR}/hccl/ OPTIONAL
    )
    if(NOT BUILD_OPEN_PROJECT)
        install(FILES 
            ${HCOMM_HEAD} 
            DESTINATION ${INSTALL_INCLUDE_DIR}/ OPTIONAL 
        )
    endif()
    install(FILES
        include/hcomm_primitives.h
        DESTINATION ${INSTALL_INCLUDE_DIR}/hcomm/ OPTIONAL
    )
    install(FILES
        include/hcomm_res.h
        include/hcomm_res_defs.h
        DESTINATION ${INSTALL_INCLUDE_DIR}/hcomm/ OPTIONAL
    )

    set(HCCL_PKG_HEAD
        pkg_inc/hccl/base.h
        pkg_inc/hccl/hccl_ex.h
        pkg_inc/hccl/hccl_one_sided_services.h
        pkg_inc/hccl/hccl_res_expt.h
        pkg_inc/hccl/hcom.h
        pkg_inc/hccl/hccl_diag.h
        pkg_inc/hccl/hcomm_diag.h
        pkg_inc/hccl/dtype_common.h
        pkg_inc/hccl/workflow.h
        pkg_inc/hccl/hccl_inner.h
        pkg_inc/hccl/hccl_ctrl_plane.h
    )
    install(FILES
        ${HCCL_PKG_HEAD}
        DESTINATION ${INSTALL_PKG_INCLUDE_DIR}/hccl/ OPTIONAL
    )
    set(HCCL_PKG_CCU_HEAD
        pkg_inc/hcomm/ccu/ccu_assist_pub.h
        pkg_inc/hcomm/ccu/ccu_common.h
        pkg_inc/hcomm/ccu/ccu_condition_v1.h
        pkg_inc/hcomm/ccu/ccu_kernel_resource.h
        pkg_inc/hcomm/ccu/ccu_datatype_v1.h
        pkg_inc/hcomm/ccu/ccu_funccall_v1.h
        pkg_inc/hcomm/ccu/ccu_instr_info_v1.h
        pkg_inc/hcomm/ccu/ccu_interface_assist_v1.h
        pkg_inc/hcomm/ccu/ccu_kernel_arg.h
        pkg_inc/hcomm/ccu/ccu_kernel_signature.h
        pkg_inc/hcomm/ccu/ccu_kernel.h
        pkg_inc/hcomm/ccu/ccu_loopblock_v1.h
        pkg_inc/hcomm/ccu/ccu_loopcall_v1.h
        pkg_inc/hcomm/ccu/ccu_loopgroupcall_v1.h
        pkg_inc/hcomm/ccu/ccu_microcode_v1.h
        pkg_inc/hcomm/ccu/ccu_operator_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_arg_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_base_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_block_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_context_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_funcblock_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_funccall_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_jump_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_jumplabel_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_loopblock_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_loopcall_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_reference_manager_v1.h
        pkg_inc/hcomm/ccu/ccu_rep_type_v1.h
        pkg_inc/hcomm/ccu/ccu_repeat_v1.h
        pkg_inc/hcomm/ccu/ccu_res_repo.h
        pkg_inc/hcomm/ccu/ccu_task_arg_v1.h
        pkg_inc/hcomm/ccu/ccu_task_param_v1.h
        pkg_inc/hcomm/ccu/hccl_ccu_res.h
    )
    install(FILES
        ${HCCL_PKG_CCU_HEAD}
        DESTINATION ${INSTALL_PKG_INCLUDE_DIR}/hcomm/ccu/ OPTIONAL
    )
    set(HCOMM_PKG_HEAD
        pkg_inc/hcomm/hcomm_primitives_expt.h
    )
    install(FILES
        ${HCOMM_PKG_HEAD}
        DESTINATION ${INSTALL_PKG_INCLUDE_DIR}/hcomm/ OPTIONAL
    )
endif()
