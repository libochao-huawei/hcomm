# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.

cmake_minimum_required(VERSION 3.14)

add_subdirectory(subcmd)

# test_cmd_base
add_executable(test_cmd_base
    cmd_base_test.cc
    main.cc
)

target_sources(test_cmd_base PRIVATE
    ${CMAKE_SOURCE_DIR}/src/cmd/cmd_base.cc
)

target_include_directories(test_cmd_base PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/store
    ${CMAKE_SOURCE_DIR}/include/runnerdb
    ${CMAKE_SOURCE_DIR}/third_party/CLI11
    ${CMAKE_BINARY_DIR}/_deps/yaml-cpp-src/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)

target_link_libraries(test_cmd_base PRIVATE
    gtest
    yaml-cpp
)

set_target_properties(test_cmd_base PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin"
)

# test_cmd_base_utils
if(TARGET hccl-vm)
add_executable(test_cmd_base_utils cmd_base_utils_test.cc)
add_dependencies(test_cmd_base_utils hccl-vm)

target_include_directories(test_cmd_base_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/plugin
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/store
    ${CMAKE_SOURCE_DIR}/include/runnerdb
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/CLI11
    ${CMAKE_BINARY_DIR}/_deps/yaml-cpp-src/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)

target_link_libraries(test_cmd_base_utils PRIVATE
    gtest_main
    gtest
    ${CMAKE_BINARY_DIR}/src/CMakeFiles/hccl-vm.dir/plugin/hccl_plugin_manager.cc.o
    ${CMAKE_BINARY_DIR}/src/CMakeFiles/hccl-vm.dir/plugin/hccl_plugin.cc.o
    common
    cmd
    store
    runnerdb
    log
    yaml-cpp
    z
    sqlite3
    dl
    rt
)

set_target_properties(test_cmd_base_utils PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin"
)

add_custom_command(TARGET test_cmd_base_utils POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/asset/cluster_model"
        "${CMAKE_BINARY_DIR}/output/bin/cluster_model"
    COMMENT "Copying cluster_model YAML files to output/bin"
)
endif()

# test_cmd_table_utils
if(TARGET hccl-vm)
add_executable(test_cmd_table_utils cmd_table_utils_test.cc)
add_dependencies(test_cmd_table_utils hccl-vm)
target_include_directories(test_cmd_table_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/plugin
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/store
    ${CMAKE_SOURCE_DIR}/include/runnerdb
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/CLI11
    ${CMAKE_BINARY_DIR}/_deps/yaml-cpp-src/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)
target_link_libraries(test_cmd_table_utils PRIVATE
    gtest_main
    gtest
    ${CMAKE_BINARY_DIR}/src/CMakeFiles/hccl-vm.dir/plugin/hccl_plugin_manager.cc.o
    ${CMAKE_BINARY_DIR}/src/CMakeFiles/hccl-vm.dir/plugin/hccl_plugin.cc.o
    common
    cmd
    store
    runnerdb
    log
    yaml-cpp
    z
    sqlite3
    dl
    rt
)
set_target_properties(test_cmd_table_utils PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin"
)
endif()

# test_cmd_cluster_model_utils
if(TARGET hccl-vm)
add_executable(test_cmd_cluster_model_utils cmd_cluster_model_utils_test.cc)
add_dependencies(test_cmd_cluster_model_utils hccl-vm)
target_include_directories(test_cmd_cluster_model_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/plugin
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/src/cmd
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/store
    ${CMAKE_SOURCE_DIR}/include/runnerdb
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/CLI11
    ${CMAKE_BINARY_DIR}/_deps/yaml-cpp-src/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)
target_link_libraries(test_cmd_cluster_model_utils PRIVATE
    gtest_main
    gtest
    ${CMAKE_BINARY_DIR}/src/CMakeFiles/hccl-vm.dir/plugin/hccl_plugin_manager.cc.o
    ${CMAKE_BINARY_DIR}/src/CMakeFiles/hccl-vm.dir/plugin/hccl_plugin.cc.o
    common
    cmd
    store
    runnerdb
    log
    yaml-cpp
    z
    sqlite3
    dl
    rt
)
set_target_properties(test_cmd_cluster_model_utils PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin"
)

add_custom_command(TARGET test_cmd_cluster_model_utils POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/asset/cluster_model"
        "${CMAKE_BINARY_DIR}/output/bin/cluster_model"
    COMMENT "Copying cluster_model YAML files for test_cmd_cluster_model_utils"
)
endif()
