# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
add_subdirectory(health)
add_subdirectory(debug/profiling)
add_subdirectory(debug/config)
add_subdirectory(stream)
add_subdirectory(launch_device)
add_subdirectory(error_code)

if(TARGET hcomm)
    target_include_directories(hcomm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/health
        ${CMAKE_CURRENT_SOURCE_DIR}/stream
        ${CMAKE_CURRENT_SOURCE_DIR}/launch_device
    )
endif()

if(TARGET ccl_kernel)
    target_include_directories(ccl_kernel PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/debug/
        ${CMAKE_CURRENT_SOURCE_DIR}/launch_device
    )
endif()

if(TARGET hccl_plf)
    target_sources(hccl_plf PRIVATE
        error_manager/adapter_error_manager.cc   # 相对于 CMakeLists.txt 的路径
        error_manager/adapter_error_manager_pub.cc
        error_manager/adapter_error_manager_pub_extend.cc
    )
endif()

if(TARGET ccl_kernel_plf)
    target_sources(ccl_kernel_plf PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/error_manager/adapter_error_manager_pub.cc
    )
endif()

if(TARGET ccl_kernel_plf_a)
    target_sources(ccl_kernel_plf_a PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/error_manager/adapter_error_manager_pub.cc
    )
endif()

if(TARGET hccl_v2) 
    target_sources(hccl_v2 PRIVATE 
        adapter_error_manager_pub_extend.cc 
    ) 
 endif()