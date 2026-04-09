# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# =============================================================================
# Function: compare_versions (CMake版本比较)
#
# 比较两个版本号
# Usage:
#   compare_versions(RESULT VERSION1 OPERATOR VERSION2)
#   OPERATOR: >=, >, <=, <, =
# =============================================================================
function(compare_versions RESULT VERSION1 OP VERSION2)
    # 将版本号分割为列表
    string(REPLACE "." ";" v1_parts "${VERSION1}")
    string(REPLACE "." ";" v2_parts "${VERSION2}")

    # 补齐长度
    list(LENGTH v1_parts len1)
    list(LENGTH v2_parts len2)
    if(len1 LESS len2)
        math(EXPR max_len "${len2} - 1")
        foreach(i RANGE ${len1} ${max_len})
            list(APPEND v1_parts "0")
        endforeach()
    elseif(len2 LESS len1)
        math(EXPR max_len "${len1} - 1")
        foreach(i RANGE ${len2} ${max_len})
            list(APPEND v2_parts "0")
        endforeach()
    endif()

    # 逐段比较
    set(compare_result "UNKNOWN")
    set(v1_len 0)
    set(v2_len 0)
    list(LENGTH v1_parts v1_len)
    list(LENGTH v2_parts v2_len)

    math(EXPR max_idx "${v1_len} - 1")
    foreach(idx RANGE ${max_idx})
        list(GET v1_parts ${idx} v1_part)
        list(GET v2_parts ${idx} v2_part)

        if(v1_part GREATER v2_part)
            set(compare_result "GREATER")
            break()
        elseif(v1_part LESS v2_part)
            set(compare_result "LESS")
            break()
        endif()
    endforeach()

    if("${compare_result}" STREQUAL "UNKNOWN")
        set(compare_result "EQUAL")
    endif()

    # 根据操作符判断结果
    set(result FALSE)
    if("${OP}" STREQUAL ">=")
        if("${compare_result}" STREQUAL "GREATER" OR "${compare_result}" STREQUAL "EQUAL")
            set(result TRUE)
        endif()
    elseif("${OP}" STREQUAL ">")
        if("${compare_result}" STREQUAL "GREATER")
            set(result TRUE)
        endif()
    elseif("${OP}" STREQUAL "<=")
        if("${compare_result}" STREQUAL "LESS" OR "${compare_result}" STREQUAL "EQUAL")
            set(result TRUE)
        endif()
    elseif("${OP}" STREQUAL "<")
        if("${compare_result}" STREQUAL "LESS")
            set(result TRUE)
        endif()
    elseif("${OP}" STREQUAL "=" OR "${OP}" STREQUAL "==")
        if("${compare_result}" STREQUAL "EQUAL")
            set(result TRUE)
        endif()
    else()
        set(result TRUE)
    endif()

    set(${RESULT} ${result} PARENT_SCOPE)
endfunction()


# =============================================================================
# Function: check_single_dependency (检查单个依赖)
#
# 检查单个包的版本依赖
# =============================================================================
function(check_single_dependency RESULT ascend_path dep_name dep_spec)
    # 解析操作符和版本要求
    set(OP "")
    set(req_ver "")

    if(dep_spec MATCHES "^>=")
        set(OP ">=")
        string(REPLACE ">=" "" req_ver "${dep_spec}")
    elseif(dep_spec MATCHES "^>")
        set(OP ">")
        string(REPLACE ">" "" req_ver "${dep_spec}")
    elseif(dep_spec MATCHES "^<=")
        set(OP "<=")
        string(REPLACE "<=" "" req_ver "${dep_spec}")
    elseif(dep_spec MATCHES "^<")
        set(OP "<")
        string(REPLACE "<" "" req_ver "${dep_spec}")
    else()
        set(OP "=")
        set(req_ver "${dep_spec}")
    endif()

    # 读取已安装包的version.info文件
    set(ver_file "${ascend_path}/share/info/${dep_name}/version.info")
    if(NOT EXISTS "${ver_file}")
        message(WARNING "Package ${dep_name} not found at ${ver_file}")
        set(${RESULT} FALSE PARENT_SCOPE)
        return()
    endif()

    # 读取并解析版本
    file(READ "${ver_file}" ver_content)
    if(ver_content MATCHES "Version=([0-9.]+)")
        set(installed_ver "${CMAKE_MATCH_1}")
    else()
        message(WARNING "Cannot parse version from ${ver_file}")
        set(${RESULT} FALSE PARENT_SCOPE)
        return()
    endif()

    # 比较版本
    compare_versions(compare_ok "${installed_ver}" "${OP}" "${req_ver}")
    if(NOT compare_ok)
        message(STATUS "Dependency check: ${dep_name} version ${installed_ver} does NOT satisfy ${OP} ${req_ver}")
    else()
        message(STATUS "Dependency check: ${dep_name} version ${installed_ver} satisfies ${OP} ${req_ver}")
    endif()

    set(${RESULT} ${compare_ok} PARENT_SCOPE)
endfunction()


# =============================================================================
# Function: pack_targets_and_files
#
# Packs targets and/or files into a flat tar.gz archive (no directory structure).
# Optionally generates a SHA256 manifest file and includes it in the archive.
#
# Usage:
#   pack_targets_and_files(
#       OUTPUT <output.tar.gz>           # e.g., "cann-tsch-compat.tar.gz"
#       [TARGETS target1 [target2 ...]]
#       [FILES file1 [file2 ...]]
#       [MANIFEST <manifest_filename>]   # e.g., "aicpu_compat_bin_hash.cfg"
#       [TAR_ROOT_DIR <directory>]       # e.g., "aicpu_kernels_device" (optional, default: ".")
#   )
#
# Examples:
#   # With manifest
#   pack_targets_and_files(
#       OUTPUT cann-tsch-compat.tar.gz
#       TARGETS app server
#       FILES "LICENSE" "config/default.json"
#       MANIFEST "aicpu_compat_bin_hash.cfg"
#   )
#
#   # Without manifest
#   pack_targets_and_files(
#       OUTPUT cann-tsch-compat.tar.gz
#       TARGETS app
#       FILES "README.md"
#   )
#
#   # With custom tar root directory
#   pack_targets_and_files(
#       OUTPUT aicpu_hcomm.tar.gz
#       TARGETS ccl_kernel
#       FILES ${CCL_KERNEL_TAR_LIBS}
#       MANIFEST "bin_hash.cfg"
#       TAR_ROOT_DIR "aicpu_kernels_device"
#   )
# =============================================================================
set(_FUNC_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
function(pack_targets_and_files)
    cmake_parse_arguments(ARG
        ""
        "OUTPUT;MANIFEST;OUTPUT_TARGET;TAR_ROOT_DIR"
        "TARGETS;FILES"
        ${ARGN}
    )

    # --- Validation ---
    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "[pack_targets_and_files] OUTPUT is required")
    endif()

    if(NOT IS_ABSOLUTE "${ARG_OUTPUT}")
        set(ARG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ARG_OUTPUT}")
    endif()

    if(NOT ARG_OUTPUT_TARGET)
        message(FATAL_ERROR "[pack_targets_and_files] OUTPUT_TARGET is required")
    endif()

    # Generate safe target name
    get_filename_component(tar_basename "${ARG_OUTPUT}" NAME_WE)
    string(MAKE_C_IDENTIFIER "pack_${tar_basename}" safe_name)
    set(staging_dir "${CMAKE_CURRENT_BINARY_DIR}/_${safe_name}_stage")

    # --- Collect all source items (as generator expressions) ---
    set(src_items "")
    foreach(tgt IN LISTS ARG_TARGETS)
        if(NOT TARGET ${tgt})
            message(FATAL_ERROR "[pack_targets_and_files] Target '${tgt}' does not exist")
        endif()
        list(APPEND src_items "$<TARGET_FILE:${tgt}>")
    endforeach()
    list(APPEND src_items ${ARG_FILES})

    if(NOT src_items)
        message(FATAL_ERROR "[pack_targets_and_files] No targets or files specified to pack")
    endif()

    if(ARG_TAR_ROOT_DIR)
        set(staging_subdir "${staging_dir}/${ARG_TAR_ROOT_DIR}")
        set(tar_src ${ARG_TAR_ROOT_DIR})
    else()
        set(staging_subdir "${staging_dir}")
        set(tar_src ".")
    endif()

    set(manifest_arg "")
    if(ARG_MANIFEST)
        if("${ARG_MANIFEST}" STREQUAL "")
            message(FATAL_ERROR "[pack] MANIFEST filename cannot be empty")
        endif()
        if(IS_ABSOLUTE "${ARG_MANIFEST}")
            message(FATAL_ERROR "[pack] MANIFEST must be relative (e.g., 'sha256sums.cfg')")
        endif()
        set(manifest_arg -D_MANIFEST_FILE=${staging_subdir}/${ARG_MANIFEST})
    endif()

    add_custom_command(
        OUTPUT ${staging_subdir}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${staging_subdir}"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND ${CMAKE_COMMAND}
            -D _STAGING_DIR=${staging_subdir}
            ${manifest_arg}
            -D "_ITEMS=$<JOIN:${src_items},;>"
            -P "${_FUNC_CMAKE_DIR}/_pack_stage.cmake"
        COMMAND ${CMAKE_COMMAND} -E tar "czf" "${ARG_OUTPUT}" ${tar_src}
        WORKING_DIRECTORY ${staging_dir}
        DEPENDS ${ARG_TARGETS} ${staging_subdir}
        COMMENT "Packing with ${ARG_OUTPUT}"
        VERBATIM
    )

    add_custom_target(${ARG_OUTPUT_TARGET} ALL DEPENDS "${ARG_OUTPUT}")
endfunction()

# =============================================================================
# Function: sign_file
#
# Signs a file and places signature in a standard directory.
#
# Usage:
#   sign_file(
#       INPUT <input_file>
#       CONFIG <sign_config>
#       RESULT_VAR <output_var>     # ← returns generated sig path
#       [DEPENDS ...]
#   )
#
# =============================================================================
function(sign_file)
    cmake_parse_arguments(
        ARG
        ""
        "INPUT;CONFIG;RESULT_VAR"
        "DEPENDS"
        ${ARGN}
    )

    # --- Validation ---
    if(DEFINED CUSTOM_SIGN_SCRIPT AND NOT CUSTOM_SIGN_SCRIPT STREQUAL "")
        set(SIGN_SCRIPT ${CUSTOM_SIGN_SCRIPT})
    else()
        set(SIGN_SCRIPT)
    endif()

    if(ENABLE_SIGN)
        set(sign_flag "true")
    else()
        set(sign_flag "false")
    endif()

    foreach(var INPUT CONFIG RESULT_VAR)
        if(NOT ARG_${var})
            message(FATAL_ERROR "[sign_file] Missing required: ${var}")
        endif()
    endforeach()

    if(NOT EXISTS "${ARG_CONFIG}")
        message(FATAL_ERROR "[sign_file] Sign config not found: ${ARG_CONFIG}")
    endif()

    # Normalize input
    if(NOT IS_ABSOLUTE "${ARG_INPUT}")
        set(ARG_INPUT "${CMAKE_CURRENT_BINARY_DIR}/${ARG_INPUT}")
    endif()

    # Auto output path: ${CMAKE_CURRENT_BINARY_DIR}/signatures
    set(signatures_dir "${CMAKE_CURRENT_BINARY_DIR}/signatures")
    get_filename_component(input_name "${ARG_INPUT}" NAME)
    set(output_sig "${signatures_dir}/${input_name}")

    # Ensure dir exists
    file(MAKE_DIRECTORY "${signatures_dir}")

    # Target name
    get_filename_component(sign_basename "${ARG_INPUT}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${sign_basename}" safe_name)
    set(sign_target "sign_${safe_name}")

    add_custom_command(
        OUTPUT "${output_sig}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${signatures_dir}
        COMMAND ${CMAKE_COMMAND} -E copy ${ARG_INPUT} ${output_sig}
        COMMAND ${sign_cmd}
        DEPENDS "${ARG_INPUT}" "${SIGN_SCRIPT}" ${ARG_DEPENDS} ${ARG_CONFIG}
        COMMENT "Signing: ${ARG_INPUT} -> ${output_sig}"
        VERBATIM
    )

    add_custom_target(${sign_target} ALL DEPENDS "${output_sig}")

    # Return path via RESULT_VAR
    if(ARG_RESULT_VAR)
        set(${ARG_RESULT_VAR} "${output_sig}" PARENT_SCOPE)
    endif()
endfunction()

macro(replace_cur_major_minor_ver)
    string(REPLACE CUR_MAJOR_MINOR_VER "${CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_VERSION_MAJOR_MINOR}" depend "${depend}")
endmacro()

# 设置包和版本号
function(set_package name)
    cmake_parse_arguments(VERSION "" "VERSION" "" ${ARGN})
    set(VERSION "${VERSION_VERSION}")
    if(NOT name)
        message(FATAL_ERROR "The name parameter is not set in set_package.")
    endif()
    if(NOT VERSION)
        message(FATAL_ERROR "The VERSION parameter is not set in set_package(${name}).")
    endif()
    string(REGEX MATCH "^([0-9]+\\.[0-9]+)" VERSION_MAJOR_MINOR "${VERSION}")
    list(APPEND CANN_VERSION_PACKAGES "${name}")
    set(CANN_VERSION_PACKAGES "${CANN_VERSION_PACKAGES}" PARENT_SCOPE)
    set(CANN_VERSION_CURRENT_PACKAGE "${name}" PARENT_SCOPE)
    set(CANN_VERSION_${name}_VERSION "${VERSION}" PARENT_SCOPE)
    set(CANN_VERSION_${name}_VERSION_MAJOR_MINOR "${VERSION_MAJOR_MINOR}" PARENT_SCOPE)
    set(CANN_VERSION_${name}_BUILD_DEPS PARENT_SCOPE)
    set(CANN_VERSION_${name}_RUN_DEPS PARENT_SCOPE)
endfunction()

# 设置构建依赖
function(set_build_dependencies pkg_name depend)
    if(NOT CANN_VERSION_CURRENT_PACKAGE)
        message(FATAL_ERROR "The set_package must be invoked first.")
    endif()
    if(NOT pkg_name)
        message(FATAL_ERROR "The pkg_name parameter is not set in set_build_dependencies.")
    endif()
    if(NOT depend)
        message(FATAL_ERROR "The depend parameter is not set in set_build_dependencies.")
    endif()
    replace_cur_major_minor_ver()
    list(APPEND CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_BUILD_DEPS "${pkg_name}" "${depend}")
    set(CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_BUILD_DEPS "${CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_BUILD_DEPS}" PARENT_SCOPE)
endfunction()

# 设置运行依赖
function(set_run_dependencies pkg_name depend)
    if(NOT CANN_VERSION_CURRENT_PACKAGE)
        message(FATAL_ERROR "The set_package must be invoked first.")
    endif()
    if(NOT pkg_name)
        message(FATAL_ERROR "The pkg_name parameter is not set in set_run_dependencies.")
    endif()
    if(NOT depend)
        message(FATAL_ERROR "The depend parameter is not set in set_run_dependencies.")
    endif()
    replace_cur_major_minor_ver()
    list(APPEND CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_RUN_DEPS "${pkg_name}" "${depend}")
    set(CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_RUN_DEPS "${CANN_VERSION_${CANN_VERSION_CURRENT_PACKAGE}_RUN_DEPS}" PARENT_SCOPE)
endfunction()

# 检查构建依赖 (CMake实现，无需Python)
function(check_pkg_build_deps pkg_name)
    # 检查ASCEND_HOME_PATH环境变量
    if(NOT DEFINED ENV{ASCEND_HOME_PATH})
        message(STATUS "ASCEND_HOME_PATH not set, skip dependency check")
        return()
    endif()

    set(ascend_path "$ENV{ASCEND_HOME_PATH}")

    # 检查ascend_path是否存在
    if(NOT EXISTS "${ascend_path}")
        message(WARNING "ASCEND_HOME_PATH ${ascend_path} does not exist, skip dependency check")
        return()
    endif()

    # 获取构建依赖列表
    set(deps ${CANN_VERSION_${pkg_name}_BUILD_DEPS})
    set(all_ok TRUE)

    # 遍历依赖列表 (格式: pkg_name1 dep_spec1 pkg_name2 dep_spec2 ...)
    list(LENGTH deps deps_len)
    if(deps_len EQUAL 0)
        message(STATUS "No build dependencies defined for ${pkg_name}")
        return()
    endif()

    math(EXPR deps_len "${deps_len} - 1")
    foreach(i RANGE 0 ${deps_len} STEP 2)
        list(GET deps ${i} dep_pkg)
        math(EXPR j "${i} + 1")
        if(j LESS_EQUAL deps_len)
            list(GET deps ${j} dep_ver)
            check_single_dependency(ok "${ascend_path}" "${dep_pkg}" "${dep_ver}")
            if(NOT ok)
                set(all_ok FALSE)
            endif()
        endif()
    endforeach()

    if(NOT all_ok)
        message(FATAL_ERROR "Check ${pkg_name} build dependencies failed!")
    endif()

    message(STATUS "All build dependencies for ${pkg_name} are satisfied")
endfunction()

set(HOST_ONLY "false")
if (NOT FULL_MODE)
set(HOST_ONLY "true")
endif()

# 添加生成version.info的目标 (CMake实现，无需Python)
# 目标名格式为：version_${包名}_info
function(add_version_info_targets)
    foreach(pkg_name ${CANN_VERSION_PACKAGES})
        set(VERSION_FILE ${CMAKE_BINARY_DIR}/version.${pkg_name}.info)

        # 生成时间戳 (格式: YYYYmmdd_HHMMSS)
        string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M%S")

        # 写入基本版本信息
        file(WRITE ${VERSION_FILE}
            "Version=${CANN_VERSION_${pkg_name}_VERSION}\n"
            "version_dir=cann\n"
        )

        # 写入运行依赖信息
        set(deps ${CANN_VERSION_${pkg_name}_RUN_DEPS})
        list(LENGTH deps deps_len)
        if(deps_len GREATER 0)
            math(EXPR deps_len "${deps_len} - 1")
            foreach(i RANGE 0 ${deps_len} STEP 2)
                list(GET deps ${i} dep_pkg)
                math(EXPR j "${i} + 1")
                if(j LESS_EQUAL deps_len)
                    list(GET deps ${j} dep_ver)
                    file(APPEND ${VERSION_FILE}
                        "required_package_${dep_pkg}_version=\"${dep_ver}\"\n"
                    )
                endif()
            endforeach()
        endif()

        # 写入时间戳
        file(APPEND ${VERSION_FILE} "timestamp=${BUILD_TIMESTAMP}\n")

        # 写入host_only信息
        file(APPEND ${VERSION_FILE} "host_only=${HOST_ONLY}\n")

        add_custom_target(version_${pkg_name}_info ALL
            DEPENDS ${VERSION_FILE}
            COMMENT "Generating version.info for ${pkg_name}"
        )
    endforeach()
endfunction()