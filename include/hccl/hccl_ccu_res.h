/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_CCU_RES_H
#define HCCL_CCU_RES_H

#include "hccl_types.h"
#include "ccu_types.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief 将 CCU 实例绑定到指定 HCCL 通信域。
 * @param[in] comm HCCL 通信域句柄，不可为 nullptr。
 * @param[in] insHandle CCU 实例句柄。
 * @return HcclResult。
 * @note 一个通信域最多绑定一个 CCU 实例。重复绑定相同或不同实例均返回 HCCL_E_PARA，且不修改原绑定。
 * @note 绑定成功后，实例所有权转移给通信域；调用方不得销毁该实例，也不得将同一实例绑定到其他通信域。
 *       绑定失败时，实例所有权仍归调用方。
 */
extern HcclResult HcclCommAssignCcuIns(HcclComm comm, CcuInsHandle insHandle);

/**
 * @brief 查询指定 HCCL 通信域已绑定的 CCU 实例。
 * @param[in] comm HCCL 通信域句柄，不可为 nullptr。
 * @param[out] insHandles 查询成功后返回 CCU 实例句柄数组，不可为 nullptr。
 * @param[out] insNum 查询成功后返回 CCU 实例数量，不可为 nullptr。
 * @return HcclResult。
 * @note 当前一个通信域最多返回一个 CCU 实例。查询成功时，insHandles[0] 为已绑定实例句柄，*insNum 为 1。
 *       查询结果仅供借用，不转移实例所有权。
 */
extern HcclResult HcclCommQueryCcuIns(HcclComm comm,
    CcuInsHandle *insHandles, uint32_t *insNum);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HCCL_CCU_RES_H
