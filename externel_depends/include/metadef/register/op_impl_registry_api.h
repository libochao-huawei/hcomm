/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_REGISTER_OP_IMPL_REGISTRY_API_H_
#define INC_EXTERNAL_REGISTER_OP_IMPL_REGISTRY_API_H_

#include <cstdlib>
#include "register/op_impl_kernel_registry.h"

struct TypesToImpl {
  const char *op_type;
  gert::OpImplKernelRegistry::OpImplFunctions funcs;
};

struct TypesToImplV2 {
  const char *op_type;
  gert::OpImplKernelRegistry::OpImplFunctionsV2 funcs;
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define METADEF_FUNC_VISIBILITY __attribute__((visibility("default")))
#else
#define METADEF_FUNC_VISIBILITY
#endif

METADEF_FUNC_VISIBILITY size_t GetRegisteredOpNum(void);
METADEF_FUNC_VISIBILITY int32_t GetOpImplFunctions(TypesToImpl *impl, size_t impl_num);
METADEF_FUNC_VISIBILITY int32_t GetOpImplFunctionsV2(TypesToImplV2 *impl, size_t impl_num);

#ifdef __cplusplus
}
#endif

#endif  // INC_EXTERNAL_REGISTER_OP_IMPL_REGISTRY_API_H_
