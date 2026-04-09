# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# makeself_built_in.cmake - 基于CMake 3.19 JSON解析的打包脚本 (无Python依赖)
# -----------------------------------------------------------------------------------------------------------

# 设置 makeself 路径
set(MAKESELF_EXE ${CPACK_3RD_LIB_PATH}/makeself/makeself.sh)
set(MAKESELF_HEADER_EXE ${CPACK_3RD_LIB_PATH}/makeself/makeself-header.sh)
if(NOT MAKESELF_EXE)
    message(FATAL_ERROR "makeself not found")
endif()

# 创建临时安装目录
set(STAGING_DIR "${CPACK_CMAKE_BINARY_DIR}/_CPack_Packages/makeself_staging")
file(MAKE_DIRECTORY "${STAGING_DIR}")

# 执行安装到临时目录
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${CPACK_CMAKE_BINARY_DIR}" --prefix "${STAGING_DIR}"
    RESULT_VARIABLE INSTALL_RESULT
)

if(NOT INSTALL_RESULT EQUAL 0)
    message(STATUS "Installation to staging directory failed: ${INSTALL_RESULT}")
    message(STATUS "Attempting to continue with existing files...")
endif()

# ===== 使用JSON配置生成filelist.csv (不需要Python!) =====
# 使用CPACK_CMAKE_SOURCE_DIR而不是CMAKE_SOURCE_DIR，因为这是在CPack上下文中
set(JSON_CONFIG_FILE "${CPACK_CMAKE_SOURCE_DIR}/scripts/package/hcomm/hcomm_files.json")
set(CSV_OUTPUT "${CPACK_CMAKE_BINARY_DIR}/filelist.csv")

# 检查JSON文件是否存在
if(NOT EXISTS "${JSON_CONFIG_FILE}")
    message(FATAL_ERROR "JSON config file not found: ${JSON_CONFIG_FILE}")
    message(STATUS "CPACK_CMAKE_SOURCE_DIR=${CPACK_CMAKE_SOURCE_DIR}")
endif()

# 包含JSON解析器
include(${CPACK_CMAKE_SOURCE_DIR}/cmake/json_parser.cmake)

# 设置版本变量 (使用CANN_VERSION_hcomm_VERSION)
if(NOT DEFINED PROJECT_VERSION)
    if(DEFINED CANN_VERSION_hcomm_VERSION)
        set(PROJECT_VERSION ${CANN_VERSION_hcomm_VERSION})
        message(STATUS "Using CANN version: ${PROJECT_VERSION}")
    else()
        set(PROJECT_VERSION "9.0.0")
        message(WARNING "CANN_VERSION_hcomm_VERSION not set, using default: ${PROJECT_VERSION}")
    endif()
endif()

# 设置时间戳 (格式: YYYYmmdd_HHMMSS)
# CMake 3.20+ 支持 %f (毫秒)，这里使用简化格式
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M%S")

# 设置TARGET_ENV变量 (用于JSON配置中的路径替换)
if(CPACK_ARCH STREQUAL "aarch64")
    set(TARGET_ENV "aarch64-linux")
else()
    set(TARGET_ENV "x86_64-linux")
endif()
message(STATUS "TARGET_ENV set to: ${TARGET_ENV}")

# 生成filelist.csv
generate_filelist_csv("${JSON_CONFIG_FILE}" "${CSV_OUTPUT}")

if(NOT EXISTS "${CSV_OUTPUT}")
    message(FATAL_ERROR "Failed to generate filelist.csv: ${CSV_OUTPUT}")
endif()

message(STATUS "Filelist generated successfully: ${CSV_OUTPUT}")

# ===== 生成动态信息文件 =====

# scene.info
set(SCENE_OUTPUT "${CPACK_CMAKE_BINARY_DIR}/scene.info")
configure_file(
    ${CPACK_CMAKE_SOURCE_DIR}/cmake/templates/scene.info.in
    ${SCENE_OUTPUT}
    @ONLY
)

# hcomm_version.h
set(VERSION_OUTPUT "${CPACK_CMAKE_BINARY_DIR}/hcomm_version.h")
configure_file(
    ${CPACK_CMAKE_SOURCE_DIR}/cmake/templates/hcomm_version.h.in
    ${VERSION_OUTPUT}
    @ONLY
)

# version.info
set(VERSION_INFO_OUTPUT "${CPACK_CMAKE_BINARY_DIR}/version.info")
configure_file(
    ${CPACK_CMAKE_SOURCE_DIR}/cmake/templates/version.info.in
    ${VERSION_INFO_OUTPUT}
    @ONLY
)

# ===== 复制生成的文件到staging目录 =====

# 确保目录存在
file(MAKE_DIRECTORY "${STAGING_DIR}/share/info/hcomm/script")
file(MAKE_DIRECTORY "${STAGING_DIR}/share/info/hcomm")

# 复制filelist.csv
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CSV_OUTPUT}
        ${STAGING_DIR}/share/info/hcomm/script/filelist.csv
    RESULT_VARIABLE COPY_RESULT
)

if(COPY_RESULT)
    message(WARNING "Failed to copy filelist.csv, continuing...")
endif()

# 复制scene.info
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
        ${SCENE_OUTPUT}
        ${STAGING_DIR}/share/info/hcomm/scene.info
)

# 复制hcomm_version.h
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
        ${VERSION_OUTPUT}
        ${STAGING_DIR}/share/info/hcomm/hcomm_version.h
)

# 复制version.info
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
        ${VERSION_INFO_OUTPUT}
        ${STAGING_DIR}/share/info/hcomm/version.info
)

# ===== 组装makeself参数 =====

# 获取包名信息
string(TOLOWER ${CMAKE_SYSTEM_NAME} OS_NAME)
string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} ARCH_NAME)

set(PACKAGE_NAME "cann-hcomm-${PROJECT_VERSION}-${OS_NAME}-${ARCH_NAME}.run")

# makeself参数
set(MAKESELF_ARGS "--pigz")
list(APPEND MAKESELF_ARGS "--complevel")
list(APPEND MAKESELF_ARGS "4")
list(APPEND MAKESELF_ARGS "--nomd5")
list(APPEND MAKESELF_ARGS "--sha256")
list(APPEND MAKESELF_ARGS "--nooverwrite")
list(APPEND MAKESELF_ARGS "--chown")
list(APPEND MAKESELF_ARGS "--tar-format")
list(APPEND MAKESELF_ARGS "ustar")
list(APPEND MAKESELF_ARGS "--tar-extra")
list(APPEND MAKESELF_ARGS "--numeric-owner")
list(APPEND MAKESELF_ARGS "--tar-quietly")

# ===== makeself打包 =====
message(STATUS "Packaging with makeself...")
message(STATUS "Package name: ${PACKAGE_NAME}")
message(STATUS "Staging dir: ${STAGING_DIR}")

execute_process(COMMAND bash ${MAKESELF_EXE}
    --header ${MAKESELF_HEADER_EXE}
    --help-header share/info/hcomm/script/help.info
    ${MAKESELF_ARGS}
    "${STAGING_DIR}"
    "${PACKAGE_NAME}"
    "HCCL Package"
    share/info/hcomm/script/install.sh
    WORKING_DIRECTORY ${STAGING_DIR}
    RESULT_VARIABLE EXEC_RESULT
    ERROR_VARIABLE EXEC_ERROR
    OUTPUT_VARIABLE EXEC_OUTPUT
)

if(NOT EXEC_RESULT EQUAL 0)
    message(FATAL_ERROR "makeself packaging failed: ${EXEC_ERROR}")
endif()

message(STATUS "Package created successfully: ${PACKAGE_NAME}")

# ===== 移动最终包到输出目录 =====

execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CPACK_PACKAGE_DIRECTORY}
    RESULT_VARIABLE MKDIR_RESULT
)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
        ${STAGING_DIR}/${PACKAGE_NAME}
        ${CPACK_PACKAGE_DIRECTORY}/${PACKAGE_NAME}
    RESULT_VARIABLE MOVE_RESULT
)

if(MOVE_RESULT)
    message(FATAL_ERROR "Failed to move package to ${CPACK_PACKAGE_DIRECTORY}")
endif()

message(STATUS "Package moved to: ${CPACK_PACKAGE_DIRECTORY}/${PACKAGE_NAME}")
