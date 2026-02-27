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

    void MainSlaveStreamInfo::Clear() {
        isBackup = false;
        opBaseOpId = 0;
        mainTailSqeTaskId = 0;
        mainFlipNum = 0;
        slaveTailSqeTaskIds.clear();
        slaveFlipNums.clear();
        return;
    }

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

    HcclResult AicpuAsyncUnfolder::UpdateAsyncUnfoldFlag(const HcclWorkflowMode workflowMode, const OpParam &param,
        const HcclTopoInfo& topoinfo)
    {
        // 注意: 需要与hccl_communicator_host.cc::UpdateAsyncUnfoldFlag保持一致

        // 检查约束条件, 确认是否需要关闭局部重执行
        // 注意: 如果已经关闭局部重执行, 则无需检查约束
        if (!asyncUnfoldEnable_) {
            return HCCL_SUCCESS;
        }

        // 检查图模式 (GE静态图)
        if (workflowMode == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OPS_KERNEL_INFO_LIB) {
            asyncUnfoldEnable_ = false;
            HCCL_RUN_INFO("[AicpuAsyncUnfolder][UpdateAsyncUnfoldFlag] workflowMode[%d]: disable async unfold",
                workflowMode);
            return HCCL_SUCCESS;
        }

        // 检查图下沉 (aclgraph)
        if (param.isCapture) {
            asyncUnfoldEnable_ = false;
            HCCL_RUN_INFO("[AicpuAsyncUnfolder][UpdateAsyncUnfoldFlag] isCapture[%d]: disable async unfold",
                param.isCapture);
            return HCCL_SUCCESS;
        }

        // 检查RDMA
        const std::unordered_map<u32, bool>& isUsedRdmaMap = topoinfo.isUsedRdmaMap;
        for (std::unordered_map<u32, bool>::const_iterator map_iter = isUsedRdmaMap.cbegin();
             map_iter != isUsedRdmaMap.end(); ++map_iter) {
            if (map_iter->second) {
                asyncUnfoldEnable_ = false;
                HCCL_RUN_INFO("[AicpuAsyncUnfolder][UpdateAsyncUnfoldFlag] rank[%u] uses rdma: disable async unfold",
                    map_iter->first);
                return HCCL_SUCCESS;
            }
        }

        CHK_PRT_RET(!asyncUnfoldEnable_,
            HCCL_ERROR("[AicpuAsyncUnfolder][UpdateAsyncUnfoldFlag] asyncUnfoldEnable_ is false."),
            HCCL_E_INTERNAL);
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
        
        HCCL_INFO("[AicpuAsyncUnfolder][GetOpAsyncUnfoldInfo] tail[%llu] head[%llu] opH2DRingBufferSize[%u]",
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
        HCCL_INFO("[AicpuAsyncUnfolder][ConsumeOpAsyncUnfoldInfo] consume head[%llu]", head_);

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

    HcclResult AicpuAsyncUnfolder::BackupStreamInfo(const uint64_t curOpBaseOpIdx,
        Stream &mainStream, std::vector<Stream> &slaveStreams) {
        // 备份当前算子索引
        mainSlaveStreamInfo_.isBackup = true;
        mainSlaveStreamInfo_.opBaseOpId = curOpBaseOpIdx;

        // 备份main stream的tailSqeTaskId
        HcclSqeContext* mainSqeContextPtr = mainStream.GetSqeContextPtr();
        CHK_PTR_NULL(mainSqeContextPtr);
        mainSlaveStreamInfo_.mainTailSqeTaskId = mainSqeContextPtr->buffer.tailSqeTaskId;
        mainSlaveStreamInfo_.mainFlipNum = mainSqeContextPtr->buffer.flipNum;

        // 备份每个slave stream的tailSqeTaskId
        const size_t slaveStreamCnt = slaveStreams.size();
        mainSlaveStreamInfo_.slaveTailSqeTaskIds.resize(slaveStreamCnt, 0);
        mainSlaveStreamInfo_.slaveFlipNums.resize(slaveStreamCnt, 0);
        for (size_t i = 0; i < slaveStreamCnt; ++i) {
            Stream& curSlaveStream = slaveStreams[i];
            HcclSqeContext* slaveSqeContextPtr = curSlaveStream.GetSqeContextPtr();
            CHK_PTR_NULL(slaveSqeContextPtr);
            mainSlaveStreamInfo_.slaveTailSqeTaskIds[i] = slaveSqeContextPtr->buffer.tailSqeTaskId;
            mainSlaveStreamInfo_.slaveFlipNums[i] = slaveSqeContextPtr->buffer.flipNum;
        }

        HCCL_INFO("[AicpuAsyncUnfolder][BackupStreamInfo] isBackup[%d] curOpBaseOpIdx[%llu] mainTailSqeTaskId[%u] "
            "mainFlipNum[%u] slaveStreamCnt[%u]",
            mainSlaveStreamInfo_.isBackup, curOpBaseOpIdx, mainSlaveStreamInfo_.mainTailSqeTaskId,
            mainSlaveStreamInfo_.mainFlipNum, slaveStreamCnt);

        return HCCL_SUCCESS;
    }

    HcclResult AicpuAsyncUnfolder::RestoreStreamInfo(const uint64_t curOpBaseOpIdx,
        Stream &mainStream, std::vector<Stream> &slaveStreams) {
        // 检查是否需要恢复备份信息
        if (!mainSlaveStreamInfo_.isBackup || mainSlaveStreamInfo_.opBaseOpId != curOpBaseOpIdx) {
            HCCL_INFO("[AicpuAsyncUnfolder][RestoreStreamInfo] isBackup[%d] or opBaseOpId[%llu] != curOpBaseOpIdx[%llu] "
                "-> no need to restore stream info",
                mainSlaveStreamInfo_.isBackup, mainSlaveStreamInfo_.opBaseOpId, curOpBaseOpIdx);
            return HCCL_SUCCESS;
        }

        // 恢复主流信息
        HcclSqeContext* mainSqeContextPtr = mainStream.GetSqeContextPtr();
        CHK_PTR_NULL(mainSqeContextPtr);
        mainSqeContextPtr->buffer.tailSqeTaskId = mainSlaveStreamInfo_.mainTailSqeTaskId;
        mainSqeContextPtr->buffer.flipNum = mainSlaveStreamInfo_.mainFlipNum;

        // 恢复从流信息
        const size_t slaveStreamCnt = slaveStreams.size();
        CHK_PRT_RET(mainSlaveStreamInfo_.slaveTailSqeTaskIds.size() != slaveStreamCnt,
            HCCL_ERROR("[AicpuAsyncUnfolder][RestoreStreamInfo] slaveTailSqeTaskIds.size[%u] != slaveStreamCnt[%u]",
                mainSlaveStreamInfo_.slaveTailSqeTaskIds.size(), slaveStreamCnt),
            HCCL_E_INTERNAL);
        CHK_PRT_RET(mainSlaveStreamInfo_.slaveFlipNums.size() != slaveStreamCnt,
            HCCL_ERROR("[AicpuAsyncUnfolder][RestoreStreamInfo] slaveFlipNums.size[%u] != slaveStreamCnt[%u]",
                mainSlaveStreamInfo_.slaveFlipNums.size(), slaveStreamCnt),
            HCCL_E_INTERNAL);
        for (size_t i = 0; i < slaveStreamCnt; ++i) {
            Stream& curSlaveStream = slaveStreams[i];
            HcclSqeContext* slaveSqeContextPtr = curSlaveStream.GetSqeContextPtr();
            CHK_PTR_NULL(slaveSqeContextPtr);
            slaveSqeContextPtr->buffer.tailSqeTaskId = mainSlaveStreamInfo_.slaveTailSqeTaskIds[i];
            slaveSqeContextPtr->buffer.flipNum = mainSlaveStreamInfo_.slaveFlipNums[i];
        }

        // 重置备份信息
        mainSlaveStreamInfo_.Clear();

        return HCCL_SUCCESS;
    }

} // namespace hccl