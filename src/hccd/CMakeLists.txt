# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 hccd 动态链接库，在 device 侧使用
add_library(hccd SHARED)

# 源文件列表
target_sources(hccd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hccd.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hccd_impl_pml.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hccd_comm.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hccl_comm_conn.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hccl_comm_conn_mgr.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hccl_raw.cc

    ${HCOMM_DIR}/src/algorithm/impl/resource_manager/workspace_resource.cc
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager/workspace_resource_impl.cc
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager/workspace_mem.cc
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager/offload_stream_manager.cc
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager/ccl_buffer_manager.cc
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager/share_ccl_buffer_manager.cc

    ${HCOMM_DIR}/src/framework/common/src/config.cc
    ${HCOMM_DIR}/src/framework/common/src/json_utils.cc
    ${HCOMM_DIR}/src/framework/common/src/param_check.cc
    ${HCOMM_DIR}/src/framework/common/src/topo/topoinfo_ranktableParser.cc
    ${HCOMM_DIR}/src/framework/common/src/topo/topoinfo_ranktableConcise.cc
    ${HCOMM_DIR}/src/framework/common/src/topo/topoinfo_ranktableHeterog.cc
    ${HCOMM_DIR}/src/framework/common/src/topo/topoinfo_ranktableStandard.cc
    ${HCOMM_DIR}/src/framework/common/src/topo/topoinfo_roletableParser.cc

    ${HCOMM_DIR}/src/common/health/rank_consistentcy_checker.cc
    ${HCOMM_DIR}/src/common/health/calc_crc.cc
    ${HCOMM_DIR}/src/common/debug/config/config_log.cc
)

# 头文件搜索路径
target_include_directories(hccd PRIVATE
    ${HCOMM_DIR}/include
    ${HCOMM_DIR}/include/hccl
    ${HCOMM_DIR}/pkg_inc
    ${HCOMM_DIR}/pkg_inc/hccl
    ${HCOMM_DIR}/src/pub_inc

    ${HCOMM_DIR}/src/legacy/interface

    ${HCOMM_DIR}/src/common/health
    ${HCOMM_DIR}/src/common/debug/profiling/inc
    ${HCOMM_DIR}/src/common/debug/profiling/inc/host
    ${HCOMM_DIR}/src/common/debug/config

    ${HCOMM_DIR}/src/framework/inc
    ${HCOMM_DIR}/src/framework/
    ${HCOMM_DIR}/src/framework/common/src/
    ${HCOMM_DIR}/src/framework/common/src/config
    ${HCOMM_DIR}/src/framework/common/src/topo
    ${HCOMM_DIR}/src/framework/common/src/aicpu

    ${HCOMM_DIR}/src/platform/inc
    ${HCOMM_DIR}/src/platform/inc/adapter
    ${HCOMM_DIR}/src/platform/common/
    ${HCOMM_DIR}/src/platform/hccp/inc
    ${HCOMM_DIR}/src/platform/hccp/inc/network
    ${HCOMM_DIR}/src/platform/resource/transport/heterog
    ${HCOMM_DIR}/src/platform/resource/transport/host

    ${HCOMM_DIR}/src/algorithm/pub_inc
    ${HCOMM_DIR}/src/algorithm/impl
    ${HCOMM_DIR}/src/algorithm/impl/resource_manager

    ${HCOMM_DIR}/externel_depends/tsch/
)
# 添加 legacy 目录下的头文件搜索路径
FILE(GLOB_RECURSE LEGACY_SOURCE_LIST LIST_DIRECTORIES TRUE ${HCOMM_DIR}/src/legacy/*)
foreach(legacy_dir ${LEGACY_SOURCE_LIST})
    if(IS_DIRECTORY ${legacy_dir})
        target_include_directories(hccd PRIVATE ${legacy_dir})
    endif()
endforeach()

# 定义预处理宏
target_compile_definitions(hccd PRIVATE
    HCCD
)

# 编译选项
target_compile_options(hccd PRIVATE
    -Werror
    -Wall
    -fno-common
    -fstack-protector-strong
    -fno-strict-aliasing
    -pipe
    -O3
    -std=c++14
    $<$<CONFIG:Debug>:-g>
)

# 链接选项
target_link_options(hccd PRIVATE
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
    $<$<CONFIG:Release>:-s>
)

if(BUILD_OPEN_PROJECT)
    target_compile_definitions(hccd PRIVATE
        OPEN_BUILD_PROJECT
    )

    target_include_directories(hccd PRIVATE
        # runtime头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/
        # mmpa头文件
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/mmpa/
        # driver头文件
        ${ASCEND_CANN_PACKAGE_PATH}/include/driver/
        # 包间接口
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/aicpu/common/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/runtime/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/profiling/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/dump/
        ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/trace/
        # 三方件头文件
        ${RDMA_CORE_INCLUDE_DIR}
        ${THIRD_PARTY_NLOHMANN_PATH}
    )

    target_link_directories(hccd PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/
    )

    target_link_libraries(hccd
        -Wl,--no-as-needed
        c_sec
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )
else()
    target_include_directories(hccd PRIVATE
        ${TOP_DIR}/inc
        ${TOP_DIR}/inc/driver
        ${TOP_DIR}/inc/aicpu/
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/include/external
        ${TOP_DIR}/runtime/pkg_inc/aicpu_sched
        ${TOP_DIR}/runtime/pkg_inc/profiling
        ${TOP_DIR}/runtime/pkg_inc/runtime
        ${TOP_DIR}/abl/msprof/inc
        ${TOP_DIR}/ace/npuruntime/inc
        ${TOP_DIR}/ace/npuruntime/inc/runtime
        ${TOP_DIR}/open_source/json/include
        ${TOP_DIR}/hcomm-legacy/src/platform/legacy/inc
    )

    target_link_libraries(hccd
        $<BUILD_INTERFACE:intf_pub_cxx14>
        $<BUILD_INTERFACE:hccl_headers>
        -Wl,--no-as-needed
        c_sec
        slog
        mmpa
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
        ofed_headers
    )

    # 安装
    install(TARGETS hccd
        LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    )
endif()

# 设置依赖关系
add_dependencies(hccd json)
if(BUILD_OPEN_PROJECT)
    add_dependencies(hccd hccl_legacy)
endif()

# 复制 so 到指定目录，供打包使用
if(DEVICE_MODE)
    set(HCCD_TAR_DIR ${HCOMM_DIR}/build_hccd/hccd_tar_pkg/lib64)
    add_custom_command(
        TARGET hccd
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HCCD_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rs> ${HCCD_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ra_peer> ${HCCD_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ra_hdc> ${HCCD_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ra> ${HCCD_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:hccd> ${HCCD_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/hcomm_utils/${PRODUCT_SIDE}/lib/lib* ${HCCD_TAR_DIR}
        COMMAND chmod 750 ${HCCD_TAR_DIR}/lib*
        COMMENT "Copying libhccd.so to ${HCCD_TAR_DIR}"
    )
endif()
