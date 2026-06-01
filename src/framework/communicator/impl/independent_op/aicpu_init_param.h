/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPU_INIT_PARAM_H
#define AICPU_INIT_PARAM_H

#include <memory>
#include "hccl_common.h"
#include "hdc_pub.h"
#include "log.h"

constexpr uint32_t HCOMID_MAX_SIZE = 128;

struct DevAicpuCommConfig {
    bool taskExceptionEnable{true};
    u32 notifyWaitTimeout{1836};
    // 如要新增配置类字段，在此处添加
};

// 自定义算子aicpu通信域公共初始化参数
struct CommAicpuParam {
    char hcomId[HCOMID_MAX_SIZE];
    s32 deviceLogicId;
    u32 devicePhyId;
    u32 deviceType;
    u32 userRankSize;
    u32 userRank;
    hccl::HDCommunicateParams kfcControlTransferH2DParams;
    hccl::HDCommunicateParams kfcStatusTransferD2HParams;
    DevAicpuCommConfig commConfig; // 收编通信域配置类变量
};

namespace hccl {

inline HcclResult InitHDCommunicate(
    std::shared_ptr<HDCommunicate>& h2dComm,
    std::shared_ptr<HDCommunicate>& d2hComm,
    const CommAicpuParam* commAicpuParam)
{
    if (commAicpuParam->kfcControlTransferH2DParams.buffLen != 0 && h2dComm == nullptr) {
        EXCEPTION_CATCH((h2dComm = std::make_shared<HDCommunicate>()), return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(h2dComm);
        CHK_RET(h2dComm->InitDevice(commAicpuParam->kfcControlTransferH2DParams));
    }
    
    if (commAicpuParam->kfcStatusTransferD2HParams.buffLen != 0 && d2hComm == nullptr) {
        EXCEPTION_CATCH((d2hComm = std::make_shared<HDCommunicate>()), return HCCL_E_PTR);
        CHK_SMART_PTR_NULL(d2hComm);
        CHK_RET(d2hComm->InitDevice(commAicpuParam->kfcStatusTransferD2HParams));
    }
    
    return HCCL_SUCCESS;
}

}  // namespace hccl

#endif