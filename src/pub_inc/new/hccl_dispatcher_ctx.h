/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_DISPATCHER_CTX_H
#define HCCL_DISPATCHER_CTX_H

#include "hccl/hccl_types.h"
#include "hccl/base.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

using DispatcherCtxPtr = void*;

/**
 * @brief 创建dispaatcher ctx
 * @param[in] ctx 待绑定的ctx
 * @param[out] ctx 返回已绑定的ctx
 * @return 执行状态码 HcclResult
 */
extern HcclResult CreateDispatcherCtx(DispatcherCtxPtr *ctx, u32 devPhyId);

/**
 * @brief 销毁dispaatcher ctx
 * @param[in] ctx 待销毁的ctx
 * @return 执行状态码 HcclResult
*/
extern HcclResult DestoryDispatcherCtx(DispatcherCtxPtr ctx);

/**
 * @brief 绑定dispaatcher ctx
 * @return 执行状态码 HcclResult
*/
extern HcclResult SetDispatcherCtx(const DispatcherCtxPtr ctx);

/**
 * @brief 获取绑定dispaatcher ctx
 * @return 执行状态码 HcclResult
*/
extern DispatcherCtxPtr GetDispatcherCtx();
#ifdef __cplusplus
}
#endif // __cplusplus
#endif
