/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd. All Rights Reserved.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#ifndef HCCL_SRC_HCCLDL_H
#define HCCL_SRC_HCCLDL_H
#include <dlfcn.h>
#include <functional>
#include "base.h"
 
#ifdef __cplusplus
extern "C" {
#endif
void *HcclDlopen(const char *libName, int mode);
void *HcclDlsym(void *handle, const char *funcName);
int HcclDlclose(void *handle);
#ifdef __cplusplus
}  // extern "C"
#endif
 
#endif