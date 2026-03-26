/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_blocklist_manager.h"

#include "log.h"

namespace hccl {
    HcclResult AicpuBlocklistManager::EnablePartialOpretry(const std::string& algName, const OpParam &param,
        bool& isEnablePartialOpretry) {
        // 只支持non-inline non-inplace的算子
        // 注意: 暂时只考虑alltoall类算子, 其他算子后续再考虑
        const HcclCMDType opType = param.opType;
        bool isValidOp = false;
        if (opType == HcclCMDType::HCCL_CMD_ALLTOALL || opType == HcclCMDType::HCCL_CMD_ALLTOALLV ||
            opType == HcclCMDType::HCCL_CMD_ALLTOALLVC) {
            isValidOp = true;
        }
        if (!isValidOp) {
            HCCL_INFO("opType %d is not supported for partial opretry", opType);
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        // 只支持non-inline non-inplace的算法
        // 注意: 暂时只考虑DirectFullmesh算法, 其他算法后续再考虑
        bool isValidAlg = false;
        if (algName == "RunAlltoAllDirectFullmesh") {
            isValidAlg = true;
        }
        if (!isValidAlg) {
            HCCL_INFO("algName %s is not supported for partial opretry", algName.c_str());
            isEnablePartialOpretry = false;
            return HCCL_SUCCESS;
        }

        isEnablePartialOpretry = true;
        return HCCL_SUCCESS;
    }
}