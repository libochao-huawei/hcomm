/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CCU_COMP_PUB_H
#define CCU_COMP_PUB_H

#include "hccl_types.h"

namespace hcomm {

/**
 * @brief 触发CCU Task Kill
 *
 * @param deviceLogicId device逻辑ID
 * @return HcclResult 返回HcclResult类型的结果
 * @note 该接口会处理全部die，未启用die将跳过
 */
HcclResult CcuSetTaskKill(const int32_t deviceLogicId);

/**
 * @brief 配置CCU Task Kill完成状态
 *
 * @param deviceLogicId device逻辑ID
 * @return HcclResult 返回HcclResult类型的结果
 * @note 该接口会处理全部die，未启用die将跳过
 */
HcclResult CcuSetTaskKillDone(const int32_t deviceLogicId);

/**
 * @brief 清空CCU Task Kill状态
 *
 * @param deviceLogicId device逻辑ID
 * @return HcclResult 返回HcclResult类型的结果
 * @note 该接口会处理全部die，未启用die将跳过
 */
HcclResult CcuCleanTaskKillState(const int32_t deviceLogicId);

/**
 * @brief 清理指定ioDie CCU的全部CKE资源，重置为0
 *
 * @param deviceLogicId device逻辑ID
 * @return HcclResult 返回HcclResult类型的结果
 * @note 未启用die无需清理将视为成功
 */
HcclResult CcuCleanDieCkes(const int32_t deviceLogicId, const uint8_t dieId);

}
#endif
