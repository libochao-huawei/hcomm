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
        if (tailH2DParams.buffLen == 0) {
            asyncUnfoldEnable_ = false;
            HCCL_RUN_INFO("[AicpuAsyncUnfolder][Init] tailH2DParams.buffLen == 0 -> disable async unfold");
            return HCCL_SUCCESS;
        } else if (tailH2D_ == nullptr) {
            EXECEPTION_CATCH((tailH2D_ = std::make_shared<hccl::HDCommunicate>()), return HCCL_E_PTR);
            CHK_SMART_PTR_NULL(tailH2D_);
            CHK_RET(tailH2D_->InitDevice(tailH2DParams));
            initTailH2D = true;
        }
        
        // 反序列化headD2H
        bool initHeadD2H = false;
        if (headD2HParams.buffLen == 0) {
            asyncUnfoldEnable_ = false;
            HCCL_RUN_INFO("[AicpuAsyncUnfolder][Init] headD2HParams.buffLen == 0 -> disable async unfold");
            return HCCL_SUCCESS;
        } else if (headD2H_ == nullptr) {
            EXECEPTION_CATCH((headD2H_ = std::make_shared<hccl::HDCommunicate>()), return HCCL_E_PTR);
            CHK_SMART_PTR_NULL(headD2H_);
            CHK_RET(headD2H_->InitDevice(headD2HParams));
            initHeadD2H = true;
        }

        // 逐一反序列化opH2D
        CHK_PRT_RET(opH2DRingBufferSize == 0,
            HCCL_ERROR("[AicpuAsyncUnfolder][Init] ringBufferSize[%u] should be larger than zero", opH2DRingBufferSize),
            HCCL_E_INTERNAL);
        CHK_PTR_NULL(opH2DRingBufferParams);
        uint32_t initOpH2DCnt = 0;
        opH2DRingBuffer_.resize(opH2DRingBufferSize, nullptr);
        for (size_t i = 0; i < opH2DRingBufferSize; ++i) {
            const hccl::HDCommunicateParams& opH2DParams = opH2DRingBufferParams[i];

            if (opH2DParams.buffLen == 0) {
                asyncUnfoldEnable_ = false;
                HCCL_RUN_INFO("[AicpuAsyncUnfolder][Init] opH2DRingBufferParams[%u].buffLen == 0 -> disable async unfold", i);
                return HCCL_SUCCESS;
            } else if (opH2DRingBuffer_[i] == nullptr) {
                EXECEPTION_CATCH((opH2DRingBuffer_[i] = std::make_shared<hccl::HDCommunicate>()), return HCCL_E_PTR);
                CHK_SMART_PTR_NULL(opH2DRingBuffer_[i]);
                CHK_RET(opH2DRingBuffer_[i]->InitDevice(opH2DParams));
                ++initOpH2DCnt;
            }
        }

        HCCL_RUN_INFO("[AicpuAsyncUnfolder][Init] initTailH2D[%u] initHeadD2H[%u] initOpH2DCnt[%u] "
                "opH2DRingBufferSize[%u] asyncUnfoldEnable_[%d]",
            initTailH2D, initHeadD2H, initOpH2DCnt, opH2DRingBufferSize, asyncUnfoldEnable_);
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuAsyncUnfolder::GetOpAsyncUnfoldInfo(bool& isValid, OpAsyncUnfoldInfo& opAsyncUnfoldInfo) {
        // 读取tail
        CHK_SMART_PTR_NULL(tailH2D_);
        uint64_t tail;
        CHK_RET(tailH2D_->Get(0, sizeof(uint64_t), reinterpret_cast<uint8_t *>(&tail)));

        // 校验tail
        const size_t opH2DRingBufferSize = opH2DRingBuffer_.size();
        CHK_PRT_RET(tail >= opH2DRingBufferSize,
            HCCL_ERROR("[AicpuAsyncUnfolder][GetOpAsyncUnfoldInfo] tail[%llu] should < opH2DRingBufferSize[%u]",
                tail, opH2DRingBufferSize),
            HCCL_E_INTERNAL);
        
        // 校验head
        CHK_PRT_RET(head_ >= opH2DRingBufferSize,
            HCCL_ERROR("[AicpuAsyncUnfolder][GetOpAsyncUnfoldInfo] head_[%llu] should < opH2DRingBufferSize[%u]",
                head_, opH2DRingBufferSize),
            HCCL_E_INTERNAL);
        
        HCCL_DEBUG("[AicpuAsyncUnfolder][GetOpAsyncUnfoldInfo] tail[%llu] head[%llu] opH2DRingBufferSize[%u]",
            tail, head_, opH2DRingBufferSize);
        
        // 判断是否存在尚未异步展开的OpAsyncUnfoldInfo
        if (head_ == tail) { // OpRingBuffer为空
            // 注意: isValid设置为false后, HcclCommAicpu不会再使用opAsyncUnfoldInfo, 因此无需memset
            isValid = false;
            return HCCL_SUCCESS;
        } else { // 至少存在一个OpAsyncUnfoldInfo
            isValid = true;
            CHK_SAFETY_FUNC_RET(memset_s(&opAsyncUnfoldInfo, sizeof(OpAsyncUnfoldInfo), 0, sizeof(OpAsyncUnfoldInfo)));
        }

        // 获取head位置的OpAsyncUnfoldInfo
        std::shared_ptr<hccl::HDCommunicate>& opH2D = opH2DRingBuffer_[head_];
        CHK_SMART_PTR_NULL(opH2D);
        CHK_RET(opH2D->Get(0, sizeof(OpAsyncUnfoldInfo), reinterpret_cast<uint8_t *>(&opAsyncUnfoldInfo)));

        // TODO: 校验获取的OpAsyncUnfoldInfo
        
        return HCCL_SUCCESS;
    }

    HcclResult AicpuAsyncUnfolder::ConsumeOpAsyncUnfoldInfo() {
        // 更新head
        // 注意: 先更新head_, 避免device重复消费OpAsyncUnfoldInfo
        head_ = (head_ + 1) % opH2DRingBuffer_.size();

        // 校验head
        CHK_PRT_RET(head_ >= opH2DRingBuffer_.size(),
            HCCL_ERROR("[AicpuAsyncUnfolder][ConsumeOpAsyncUnfoldInfo] head_[%llu] should < opH2DRingBuffer_.size[%u]",
                head_, opH2DRingBuffer_.size()),
            HCCL_E_INTERNAL);

        // 写入更新后的head
        // 注意: head_更新后, 确保device不会再消费原head位置的算子信息后, host再感知该消费并生产新的OpAsyncUnfoldInfo
        CHK_SMART_PTR_NULL(headD2H_);
        CHK_RET(headD2H_->Put(0, sizeof(uint64_t), reinterpret_cast<uint8_t *>(&head_)));

        return HCCL_SUCCESS;
    }

    HcclResult AicpuAsyncUnfolder::BackupTailSqeTaskIds(Stream &mainStream, std::vector<Stream> &slaveStreams) {
        // 备份main stream的tailSqeTaskId
        HcclSqeContext* mainSqeContextPtr = mainStream.GetSqeContextPtr();
        CHK_PTR_NULL(mainSqeContextPtr);
        mainTailSqeTaskId_ = mainSqeContextPtr->buffer.tailSqeTaskId;

        // 备份每个slave stream的tailSqeTaskId
        const size_t slaveStreamCnt = slaveStreams.size();
        slaveTailSqeTaskIds_.resize(slaveStreamCnt, 0);
        for (size_t i = 0; i < slaveStreamCnt; ++i) {
            Stream& curSlaveStream = slaveStreams[i];
            HcclSqeContext* slaveSqeContextPtr = curSlaveStream.GetSqeContextPtr();
            CHK_PTR_NULL(slaveSqeContextPtr);
            slaveTailSqeTaskIds_[i] = slaveSqeContextPtr->buffer.tailSqeTaskId;
        }

        return HCCL_SUCCESS;
    }

} // namespace hccl