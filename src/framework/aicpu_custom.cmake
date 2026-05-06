# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if(DEVICE_MODE AND KERNEL_MODE)
    set(CCL_KERNEL_TAR_DIR ${HCCL_BASE_DIR}/../build_device/ccl_kernel_tar_pkg/aicpu_kernels_device)
    add_custom_command(
        TARGET ccl_kernel
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CCL_KERNEL_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccl_kernel> ${CCL_KERNEL_TAR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ccl_kernel_plf> ${CCL_KERNEL_TAR_DIR}
        COMMAND chmod 750 ${CCL_KERNEL_TAR_DIR}/lib*
        COMMENT "Copying libccl_kernel_plf.so libccl_kernel.so to ${CCL_KERNEL_TAR_DIR}"
    )

    set(AICPU_CUSTOM_COMPILE_DEFINITIONS
        HCCD
        CCL_KERNEL_AICPU
        OPEN_BUILD_PROJECT
        -D_GLIBCXX_USE_CXX11_ABI=1
    )
    set(AICPU_CUSTOM_LINK_LIBRARIES
        -Wl,--no-as-needed
        ascend_hal
        c_sec
        mmpa
        -Wl,--whole-archive
        ccl_kernel_plf_a
        -Wl,--no-whole-archive
        -Wl,--as-needed
        -lrt
        -ldl
        -lpthread
    )

    add_library(aicpu_custom SHARED)
    get_target_property(CCL_KERNEL_ALL_SOURCES ccl_kernel SOURCES)
    if(CCL_KERNEL_ALL_SOURCES)
        list(REMOVE_DUPLICATES CCL_KERNEL_ALL_SOURCES)
        target_sources(aicpu_custom PRIVATE ${CCL_KERNEL_ALL_SOURCES})
    endif()
    target_compile_definitions(aicpu_custom PRIVATE ${AICPU_CUSTOM_COMPILE_DEFINITIONS})
    target_compile_options(aicpu_custom PRIVATE
        -Werror
        -Wfloat-equal
        -Wall
        -fno-common
        -fstack-protector-strong
        -fno-strict-aliasing
        -pipe
        -O3
        -std=c++14
    )
    target_link_options(aicpu_custom PRIVATE
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
        -s
    )
    target_include_directories(aicpu_custom PRIVATE
        ${CCL_KERNEL_INCLUDE_LIST}
        ${LEGACY_INCLUDE_LIST}
    )
    target_link_directories(aicpu_custom PRIVATE
        ${ASCEND_CANN_PACKAGE_PATH}/devlib/device/
    )
    target_link_libraries(aicpu_custom PRIVATE ${AICPU_CUSTOM_LINK_LIBRARIES})
    add_dependencies(aicpu_custom ccl_kernel ccl_kernel_plf_a aicpu_custom_json)
    set_target_properties(aicpu_custom PROPERTIES
        OUTPUT_NAME aicpu_custom
        PREFIX "lib"
        SUFFIX ".so"
    )
    install(TARGETS aicpu_custom
        LIBRARY DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libaicpu_custom.json
        DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel ${INSTALL_OPTIONAL}
        COMPONENT hcomm
    )
endif()