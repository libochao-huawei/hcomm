/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_async_unfolder.h"

namespace hccl {

    AicpuAsyncUnfolder::AicpuAsyncUnfolder() {}

    AicpuAsyncUnfolder::~AicpuAsyncUnfolder() {}

    HcclResult AicpuAsyncUnfolder::Init(const hccl::HDCommunicateParams& tailH2DParams,
        const hccl::HDCommunicateParams& headD2HParams, const hccl::HDCommunicateParams* opH2DRingBufferParams,
        const size_t opH2DRingBufferSize) {
        // 注意: HDCommunicate中的head/tailCnt是用于保证读写一致性, 而AicpuAsyncUnfolder中的head/tail是为了维护ring buffer

        // 反序列化tailH2D
        bool initTailH2D = false;
        if (tailH2DParams.buffLen != 0 && tailH2D_ == nullptr) {
            EXECEPTION_CATCH((tailH2D_ = std::make_shared<hccl::HDCommunicate>()), return HCCL_E_PTR);
            CHK_SMART_PTR_NULL(tailH2D_);
            CHK_RET(tailH2D_->InitDevice(tailH2DParams));
            initTailH2D = true;
        }
        
        // 反序列化headD2H
        bool initHeadD2H = false;
        if (headD2HParams.buffLen != 0 && headD2H_ == nullptr) {
            EXECEPTION_CATCH((headD2H_ = std::make_shared<hccl::HDCommunicate>()), return HCCL_E_PTR);
            CHK_SMART_PTR_NULL(headD2H_);
            CHK_RET(headD2H_->InitDevice(headD2HParams));
            initHeadD2H = true;
        }

        // 逐一反序列化opH2D
        CHK_PRT_RET(opH2DRingBufferSize == 0,
            HCCL_ERROR("[AicpuAsyncUnfolder][Init] ringBufferSize[%u] should be larger than zero"),
            HCCL_E_INTERNAL);
        CHK_PTR_NULL(opH2DRingBufferParams);
        uint32_t initOpH2DCnt = 0;
        opH2DRingBuffer_.resize(opH2DRingBufferSize, nullptr);
        for (size_t i = 0; i < opH2DRingBufferSize; ++i) {
            const hccl::HDCommunicateParams& opH2DParams = opH2DRingBufferParams[i];

            if (opH2DParams.buffLen != 0 && opH2DRingBuffer_[i] == nullptr) {
                EXECEPTION_CATCH((opH2DRingBuffer_[i] = std::make_shared<hccl::HDCommunicate>()), return HCCL_E_PTR);
                CHK_SMART_PTR_NULL(opH2DRingBuffer_[i]);
                CHK_RET(opH2DRingBuffer_[i]->InitDevice(opH2DParams));
                ++initOpH2DCnt;
            }
        }

        HCCL_RUN_INFO("[AicpuAsyncUnfolder][Init] initTailH2D[%u] initHeadD2H[%u] initOpH2DCnt[%u] opH2DRingBufferSize[%u]",
            initTailH2D, initHeadD2H, initOpH2DCnt, opH2DRingBufferSize);
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuAsyncUnfolder::GetOpAsyncUnfoldInfo(bool& isValid, OpAsyncUnfoldInfo& opAsyncUnfoldInfo) {
        // 读取tail
        CHK_SMART_PTR_NULL(tailH2D_);
        uint64_t tail;
        CHK_RET(tailH2D_->Get(0, sizeof(uint64_t), reinterpret_cast<uint8_t *>(&tail)));
        
        // 判断是否存在尚未异步展开的OpAsyncUnfoldInfo
        if (head_ == tail) { // OpRingBuffer为空
            isValid = false;
            return HCCL_SUCCESS;
        } else { // 至少存在一个OpAsyncUnfoldInfo
            isValid = true;
        }

        // TODO: 获取head位置的OpAsyncUnfoldInfo
        
        return HCCL_SUCCESS;
    }

} // namespace hccl