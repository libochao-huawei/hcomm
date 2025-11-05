/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "independent_op.h"

namespace hccl {

IndependentOp::IndependentOp(){};

HcclResult IndependentOp::SetIndependentOpConfig(const CommConfig &commConfig, const RankTable_t &rankTable,
    const HcclTopoAttr &topoAttr, aclrtBinHandle binHandle)
{
    commEngine_ = commConfig.GetCommEngine();
    threadNum_ = commConfig.GetThreadNum();
    notifyNumPerThread_ = commConfig.GetNotifyNumPerThread();
    cclBufferSize_ = commConfig.GetConfigBufferSize();
    commId_ = commConfig.GetConfigCommName();
    commMemMgr_.CommSetHcclBufferSize(commConfig.GetConfigBufferSize());
    HCCL_INFO("[IndependentOp][%s] Hcom[%s] threadNum[%u], notifyPerThread[%u]", 
        __func__, commId_.c_str(), threadNum_, notifyNumPerThread_);
    CHK_PRT(engineResMgr_.Init(threadNum_, notifyNumPerThread_, commId_, binHandle));
    CHK_RET(rankgraph_.Init(rankTable, topoAttr));
    return HCCL_SUCCESS;
}


}  // namespace hccl