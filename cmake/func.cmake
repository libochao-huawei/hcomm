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

    if(EXISTS "${SIGN_SCRIPT}")
        get_filename_component(EXT ${SIGN_SCRIPT} EXT)  # 获取文件扩展名

        if(${EXT} STREQUAL ".sh")
            set(sign_cmd bash ${SIGN_SCRIPT} ${output_sig} ${ARG_CONFIG} ${sign_flag})
        elseif(${EXT} STREQUAL ".py")
            set(root_dir ${CMAKE_SOURCE_DIR})
            message(STATUS "Detected +++VERSION_INFO: ${VERSION_INFO}")
            set(sign_cmd python3 ${root_dir}/scripts/sign/add_header_sign.py ${signatures_dir} ${sign_flag} --bios_check_cfg=${ARG_CONFIG} --sign_script=${SIGN_SCRIPT} --version=${VERSION_INFO})
        endif()
    else()
        set(sign_cmd )
    endif()

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

# 检查构建依赖
function(check_pkg_build_deps pkg_name)
    execute_process(
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_build_dependencies.py "$ENV{ASCEND_HOME_PATH}" ${CANN_VERSION_${pkg_name}_BUILD_DEPS}
        RESULT_VARIABLE result
    )
    if(result)
        message(FATAL_ERROR "Check ${pkg_name} build dependencies failed!")
    endif()
endfunction()

# 添加生成version.info的目标
# 目标名格式为：version_${包名}_info
function(add_version_info_targets)
    if(ENABLE_DEVICE)
        set(HOST_ONLY "false")
    else()
        set(HOST_ONLY "true")
    endif()

    foreach(pkg_name ${CANN_VERSION_PACKAGES})
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/version.${pkg_name}.info
            COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_version_info.py --output ${CMAKE_BINARY_DIR}/version.${pkg_name}.info
                    "${CANN_VERSION_${pkg_name}_VERSION}" ${CANN_VERSION_${pkg_name}_RUN_DEPS}
            COMMAND ${CMAKE_COMMAND} -E echo "host_only=${HOST_ONLY}" >> ${CMAKE_BINARY_DIR}/version.${pkg_name}.info
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_version_info.py
            VERBATIM
        )
        add_custom_target(version_${pkg_name}_info ALL DEPENDS ${CMAKE_BINARY_DIR}/version.${pkg_name}.info)
    endforeach()
endfunction()

# 将 cc 源文件的相对路径，转换为绝对路径
function(__cann_to_absolute_path origin_sources origin_source_dir output_sources)
    set(sources_list)
    foreach(source_file ${${origin_sources}})
        if(NOT IS_ABSOLUTE ${source_file} AND ${source_file} MATCHES "\\.(c|cc|cpp)$")
            list(APPEND sources_list ${${origin_source_dir}}/${source_file})
        else()
            list(APPEND sources_list ${source_file})
        endif()
    endforeach()
    set(${output_sources} ${sources_list} PARENT_SCOPE)
endfunction()

# 克隆 target 的属性到新 target，IGNORE_PROP 为需要跳过的属性名列表
#
# Usage:
#   clone_cann_target(ORIGIN ccl_kernel OUTPUT aicpu_custom)
#   clone_cann_target(ORIGIN ccl_kernel OUTPUT aicpu_custom IGNORE_PROP LINK_LIBRARIES)
#   clone_cann_target(ORIGIN ccl_kernel OUTPUT aicpu_custom IGNORE_PROP SOURCES LINK_LIBRARIES)
function(clone_cann_target)
    cmake_parse_arguments(ARG
        ""
        "ORIGIN;OUTPUT"
        "IGNORE_PROP"
        ${ARGN}
    )

    if(NOT ARG_ORIGIN OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "clone_cann_target: ORIGIN or OUTPUT is required")
    endif()

    # 克隆源文件，同时将相对路径转换为绝对路径
    if(NOT "SOURCES" IN_LIST ARG_IGNORE_PROP)
        get_target_property(sourceFiles ${ARG_ORIGIN} SOURCES)
        get_target_property(sourceDir ${ARG_ORIGIN} SOURCE_DIR)
        __cann_to_absolute_path(sourceFiles sourceDir absolute_sources_files)
        target_sources(${ARG_OUTPUT} PRIVATE
            ${absolute_sources_files}
        )
    endif()

    # 克隆头文件搜索路径
    if(NOT "INCLUDE_DIRECTORIES" IN_LIST ARG_IGNORE_PROP)
        get_target_property(includeDirs ${ARG_ORIGIN} INCLUDE_DIRECTORIES)
        target_include_directories(${ARG_OUTPUT} PRIVATE
            ${includeDirs}
        )
    endif()

    # 克隆链接库
    if(NOT "LINK_LIBRARIES" IN_LIST ARG_IGNORE_PROP)
        get_target_property(linkLibs ${ARG_ORIGIN} LINK_LIBRARIES)
        target_link_libraries(${ARG_OUTPUT} PRIVATE
            ${linkLibs}
        )
    endif()

    # 克隆链接目录
    if(NOT "LINK_DIRECTORIES" IN_LIST ARG_IGNORE_PROP)
        get_target_property(linkDirs ${ARG_ORIGIN} LINK_DIRECTORIES)
        if(linkDirs)
            target_link_directories(${ARG_OUTPUT} PRIVATE
                ${linkDirs}
            )
        endif()
    endif()

    # 克隆宏定义
    if(NOT "COMPILE_DEFINITIONS" IN_LIST ARG_IGNORE_PROP)
        get_target_property(compileDefs ${ARG_ORIGIN} COMPILE_DEFINITIONS)
        if(compileDefs)
            target_compile_definitions(${ARG_OUTPUT} PRIVATE
                ${compileDefs}
            )
        endif()
    endif()

    # 克隆编译选项
    if(NOT "COMPILE_OPTIONS" IN_LIST ARG_IGNORE_PROP)
        get_target_property(compileOptions ${ARG_ORIGIN} COMPILE_OPTIONS)
        if(compileOptions)
            target_compile_options(${ARG_OUTPUT} PRIVATE
                ${compileOptions}
            )
        endif()
    endif()

    # 克隆链接选项
    if(NOT "LINK_OPTIONS" IN_LIST ARG_IGNORE_PROP)
        get_target_property(linkOpts ${ARG_ORIGIN} LINK_OPTIONS)
        if(linkOpts)
            target_link_options(${ARG_OUTPUT} PRIVATE
                ${linkOpts}
            )
        endif()
    endif()
endfunction()