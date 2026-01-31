/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库 device 侧和 host 侧共享的一些公共头文件
 * Author: caichanghua
 * Create: 2025-06-19
 */
#ifndef HCCLV2_UTILS_COMMON_INTERFACE_H
#define HCCLV2_UTILS_COMMON_INTERFACE_H

namespace Hccl {
// 在 device 和 host 目录下分别实现
bool IsAicpuMode();
}
#endif // HCCLV2_UTILS_COMMON_INTERFACE_H