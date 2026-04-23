# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.14)
project(device_hcomm)

set(PRODUCT_SIDE device)
set(HCOMM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../" CACHE PATH "Project top directory")
include(${HCOMM_ROOT_DIR}/cmake/fetch_cann_cmake.cmake)

# 初始化
init_cann_project()

# third_party
include(${HCOMM_ROOT_DIR}/cmake/hcomm_utils.cmake)
include(${HCOMM_ROOT_DIR}/cmake/third_party/json.cmake)

add_cann_subdirectories_relative(${HCOMM_ROOT_DIR}
    ${HCOMM_ROOT_DIR}/src/hccd
    ${HCOMM_ROOT_DIR}/src/framework
    ${HCOMM_ROOT_DIR}/src/platform/hccp
)

# 打包并签名：cann-hcomm-compat.tar.gz
pack_targets_and_files(
    OUTPUT_TARGET "generate_device_hccp_package"
    OUTPUT "cann-hcomm-compat.tar.gz"
    TARGETS rs ra_adp tls_adp ascend_kms net_co hccp_service.bin
    MANIFEST "hccp_compat_bin_hash.cfg"
)
sign_file(
    INPUT "cann-hcomm-compat.tar.gz"
    CONFIG "${HCOMM_ROOT_DIR}/scripts/sign/hcomm_check_cfg.xml"
    RESULT_VAR "cann-hcomm-compat_sign_file"
    DEPENDS generate_device_hccp_package
)

# 打包并签名：cann-hccd-compat.tar.gz
pack_targets_and_files(
    OUTPUT_TARGET "generate_device_hccd_package"
    OUTPUT "cann-hccd-compat.tar.gz"
    TARGETS rs ra ra_hdc ra_peer tls_adp ascend_kms hccd
    MANIFEST "bin_hash.cfg"
    TAR_ROOT_DIR "lib64"
)
sign_file(
    INPUT "cann-hccd-compat.tar.gz"
    CONFIG "${HCOMM_ROOT_DIR}/scripts/sign/hcomm_check_cfg.xml"
    RESULT_VAR "cann-hccd-compat_sign_file"
    DEPENDS generate_device_hccd_package
)

# 打包并签名：aicpu_hcomm.tar.gz
pack_targets_and_files(
    OUTPUT_TARGET "generate_device_aicpu_package"
    OUTPUT "aicpu_hcomm.tar.gz"
    TARGETS ccl_kernel ccl_kernel_plf
    MANIFEST "bin_hash.cfg"
    TAR_ROOT_DIR "aicpu_kernels_device"
)
sign_file(
    INPUT "aicpu_hcomm.tar.gz"
    CONFIG "${HCOMM_ROOT_DIR}/scripts/sign/hcomm_check_cfg.xml"
    RESULT_VAR "aicpu_hcomm_sign_file"
    DEPENDS generate_device_aicpu_package
)

# install(FILES
#     ${tsch_compat_sign_file}
#     DESTINATION compat
#     COMPONENT npu-runtime
# )

# if(TOPLEVEL_PROJECT)
#     set_cann_subprj_package()
# endif()
