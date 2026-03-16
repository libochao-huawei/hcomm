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

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
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

set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

set(CMAKE_CXX_COMPILE_OBJECT
    "<CMAKE_CXX_COMPILER> <DEFINES> -D__FILE__='\"$(notdir $(abspath <SOURCE>))\"' -Wno-builtin-macro-redefined <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>"
)
set(CMAKE_C_COMPILE_OBJECT
    "<CMAKE_C_COMPILER> <DEFINES> -D__FILE__='\"$(notdir $(abspath <SOURCE>))\"' -Wno-builtin-macro-redefined <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>"
)

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
    include(cmake/config.cmake)
    include(cmake/function.cmake)
    include(cmake/intf_pub_gccnative.cmake)
    include(cmake/third_party/json.cmake)
    include(cmake/third_party/gtest.cmake)
    include(cmake/third_party/mockcpp.cmake)
    include(cmake/third_party/rdma-core.cmake)
    add_subdirectory(test)
    return()

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
    if(ENABLE_OPEN_SRC)
 	    check_pkg_build_deps("hcomm")
    endif()
    add_version_info_targets()
    pack_built_in()
    add_subdirectory(src)
    add_subdirectory(python)

    set(HCCL_HEAD
        include/hccl/hccl_comm.h
        include/hccl/hccl_types.h
        src/pub_inc/hccn_rping.h
        include/hccl/hccl_rank_graph.h
        include/hccl/hccl_res.h
        include/hccl/hcomm_primitives.h
        include/hcomm_res.h
        include/hcomm_res_defs.h
    )

    set(HCCL_JSON
        cmake/scripts/ccl_kernel.json
    )

    if(DEVICE_MODE)
        set(HCCP_TAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_device/tar_pkg/)
        set(CCL_KERNEL_TAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_device/ccl_kernel_tar_pkg/)
        set(HCCD_TAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build_hccd/hccd_tar_pkg/)

        message(STATUS "Detected CUSTOM_SIGN_SCRIPT: ${CUSTOM_SIGN_SCRIPT}")
        message(STATUS "Detected ENABLE_SIGN: ${ENABLE_SIGN}")
        pack_targets_and_files(
            OUTPUT_TARGET "generate_device_hccp_package"
            OUTPUT "cann-hcomm-compat.tar.gz"
            TARGETS hccp_service.bin
            FILES  ${HCCP_TAR_DIR}/librs.so ${HCCP_TAR_DIR}/libra_adp.so ${HCCP_TAR_DIR}/libtls_adp.so ${HCCP_TAR_DIR}/libascend_kms.so
            MANIFEST "hccp_compat_bin_hash.cfg"
        )
        add_dependencies(generate_device_hccp_package hccp_service.bin)
        sign_file(
            INPUT "cann-hcomm-compat.tar.gz"
            CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml"
            RESULT_VAR "cann-hcomm-compat_sign_file"
            DEPENDS generate_device_hccp_package
        )
        add_dependencies(sign_cann_hcomm_compat generate_device_hccp_package)

        set(HCCD_TAR_LIBS
            ${HCCD_TAR_DIR}/lib64/libascend_kms.so
            ${HCCD_TAR_DIR}/lib64/libhccd.so
            ${HCCD_TAR_DIR}/lib64/libra_hdc.so
            ${HCCD_TAR_DIR}/lib64/libra_peer.so
            ${HCCD_TAR_DIR}/lib64/libra.so
            ${HCCD_TAR_DIR}/lib64/librs.so
            ${HCCD_TAR_DIR}/lib64/libtls_adp.so
        )
        pack_targets_and_files(
            OUTPUT_TARGET "generate_device_hccd_package"
            OUTPUT "cann-hccd-compat.tar.gz"
            FILES ${HCCD_TAR_LIBS}
            MANIFEST "bin_hash.cfg"
            TAR_ROOT_DIR "lib64"
        )
        add_dependencies(generate_device_hccd_package hccd)
        sign_file(
            INPUT "cann-hccd-compat.tar.gz"
            CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml"
            RESULT_VAR "cann-hccd-compat_sign_file"
            DEPENDS generate_device_hccd_package
        )
        add_dependencies(sign_cann_hccd_compat generate_device_hccd_package)

        if(KERNEL_MODE)
            set(CCL_KERNEL_TAR_LIBS
                ${CCL_KERNEL_TAR_DIR}/aicpu_kernels_device/libccl_kernel_plf.so
                ${CCL_KERNEL_TAR_DIR}/aicpu_kernels_device/libccl_kernel.so
            )
            pack_targets_and_files(
                OUTPUT_TARGET "generate_device_aicpu_package"
                OUTPUT "aicpu_hcomm.tar.gz"
                FILES ${CCL_KERNEL_TAR_LIBS}
                MANIFEST "bin_hash.cfg"
                TAR_ROOT_DIR "aicpu_kernels_device"
            )
            add_dependencies(generate_device_aicpu_package ccl_kernel)
            sign_file(
                INPUT "aicpu_hcomm.tar.gz"
                CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sign/hcomm_check_cfg.xml"
                RESULT_VAR "aicpu_hcomm_sign_file"
                DEPENDS generate_device_aicpu_package
            )
            add_dependencies(sign_aicpu_hcomm generate_device_aicpu_package)
        endif()
    endif()

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
        )
        install(FILES ${BUILD_DEVICE_DIR}/signatures/aicpu_hcomm.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
        )
        install(FILES ${BUILD_HCCD_DIR}/signatures/cann-hccd-compat.tar.gz
            DESTINATION ${INSTALL_DEVICE_TAR_DIR} OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.so
            DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR} OPTIONAL
        )
        install(FILES ${BUILD_DEVICE_DIR}/src/framework/libaicpu_custom.json
            DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR} OPTIONAL
        )

        add_custom_target(install_ccl_kernel DEPENDS
            ${BUILD_DEVICE_DIR}/src/framework/libccl_kernel.so
        )
        install(FILES ${BUILD_DEVICE_DIR}/src/framework/libccl_kernel.so
            DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
        )
        add_dependencies(install_tar hcomm)
        add_dependencies(install_ccl_kernel hcomm)
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
endif()
