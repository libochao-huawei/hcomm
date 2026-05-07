# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 定义 aicpu_custom 库，在 device 侧使用
add_library(aicpu_custom SHARED)

# 克隆 ccl_kernel 属性，忽略链接库
target_clone(
    ORIGIN ccl_kernel
    NEW aicpu_custom
    IGNORE_PROP LINK_LIBRARIES
)

# 链接库
target_link_libraries(${ARG_NEW} PRIVATE
    -Wl,--no-as-needed
    ascend_hal
    c_sec
    mmpa
    -Wl,--whole-archive
    ccl_kernel_plf_a            # 链接 ccl_kernel_plf 的静态库
    -Wl,--no-whole-archive
    -Wl,--as-needed
    -lrt
    -ldl
    -lpthread
)

install(TARGETS aicpu_custom
    LIBRARY DESTINATION ${INSTALL_CCL_KERNEL_JSON_DIR}/kernel ${INSTALL_OPTIONAL}
    COMPONENT hcomm
)
