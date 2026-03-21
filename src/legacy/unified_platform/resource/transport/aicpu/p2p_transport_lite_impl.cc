/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cmath>
#include "p2p_transport_lite_impl.h"
#include "binary_stream.h"
#include "exception_util.h"
#include "sal.h"

namespace Hccl {
constexpr u32 NOTIFY_RECORD_WRITE_VALUE = 1;
P2PTransportLiteImpl::P2PTransportLiteImpl(
    std::vector<char> &uniqueId, std::function<void(u32 streamId, u32 taskId, const TaskParam &taskParam)> callback)
{
    callback_ = callback;
    // [header...][notifyUniqueId...][rmtNotifyUniqueId...][rmtBufferUniqueIds...]
    BinaryStream binaryStream(uniqueId);
    u32          theType;
    binaryStream >> theType;
    binaryStream >> notifyNum;
    binaryStream >> bufferNum;

    std::vector<char> notifyUniqueIds;
    binaryStream >> notifyUniqueIds;
    ParseLocNotifyVec(notifyUniqueIds);

    std::vector<char> rmtNotifyUniqueIds;
    binaryStream >> rmtNotifyUniqueIds;
    ParseRmtBufferVec(rmtNotifyUniqueIds, rmtNotifyVec, RmaP2PBufType::NOTIFY);

    std::vector<char> rmtBufferUniqueIds;
    binaryStream >> rmtBufferUniqueIds;
    ParseRmtBufferVec(rmtBufferUniqueIds, rmtBufferVec, RmaP2PBufType::BUFFER);
}

P2PTransportLiteImpl::~P2PTransportLiteImpl()
{
}

std::string P2PTransportLiteImpl::Describe() const
{
    std::string desc = "P2PTransportLiteImpl[";

    u32 idx = 0;
    desc += "locNotifyVec=[";
    for (auto &it : locNotifyVec) {
        desc += StringFormat("idx=%u, %s;", idx, it->Describe().c_str());
        idx++;
    }

    idx = 0;
    desc += "], rmtNotifyVec=[";
    for (auto &it : rmtNotifyVec) {
        desc += StringFormat("idx=%u, %s;", idx, it.Describe().c_str());
        idx++;
    }

    idx = 0;
    desc += "], rmtBufferVec=[";
    for (auto &it : rmtBufferVec) {
        desc += StringFormat("idx=%u, %s;", idx, it.Describe().c_str());
        idx++;
    }

    desc += "]]";
    return desc;
}

void P2PTransportLiteImpl::ParseLocNotifyVec(std::vector<char> &data)
{
    if (notifyNum == 0) {
        HCCL_WARNING("P2PTransportLiteImpl::ParseLocNotifyVec num is 0");
        return;
    }
    u32 notifySizePerDto = data.size() / notifyNum;

    for (u32 idx = 0; idx < notifyNum; idx++) {
        auto              start = data.begin() + idx * notifySizePerDto;
        auto              end   = start + notifySizePerDto;
        std::vector<char> dto(start, end);
        locNotifyVec.push_back(std::make_unique<NotifyLite>(dto));
        HCCL_INFO("[P2PTransportLiteImpl][ParseLocNotifyVec]locNotify idx=%u, %s", idx, locNotifyVec.back()->Describe().c_str());
    }
}

void P2PTransportLiteImpl::ParseRmtBufferVec(std::vector<char> &data, RmtP2PBufLiteVec &vec, RmaP2PBufType rmtType) const
{
    u32 num = 0;
    if (rmtType == RmaP2PBufType::NOTIFY) {
        num = notifyNum;
    } else {
        num = bufferNum;
    }

    if (num == 0) {
        HCCL_WARNING("P2PTransportLiteImpl::ParseRmtBufferVec %s num is 0", rmtType.Describe().c_str());
        return;
    }

    u32 rmtBufferSizePerDto = data.size() / num;
    HCCL_INFO("[P2PTransportLiteImpl][ParseRmtBufferVec]Parse %s num=%u, sizePerDto=%u", rmtType.Describe().c_str(), num, rmtBufferSizePerDto);
    BinaryStream binaryStream(data);

    for (u32 idx = 0; idx < num; idx++) {
        RmtP2PBufLite p2pBufLite;
        binaryStream >> p2pBufLite.addr;
        binaryStream >> p2pBufLite.size;
        HCCL_INFO("[P2PTransportLiteImpl][ParseRmtBufferVec]idx=%u, %s %s", idx, rmtType.Describe().c_str(), p2pBufLite.Describe().c_str());
        vec.push_back(p2pBufLite);
    }
}

Buffer P2PTransportLiteImpl::GetRmtBuffer(u32 index)
{
    if (UNLIKELY(index >= rmtBufferVec.size())) {
        THROW<InternalException>(StringFormat("P2PTransportLiteImpl::GetRmtBuffer out-of-bounds. index=%u, size=%u",
            index, rmtBufferVec.size()));
    }
    return Buffer(rmtBufferVec[index].addr, rmtBufferVec[index].size);
}

void P2PTransportLiteImpl::BuildNotifyRecordTask(const StreamLite &stream, u64 rmtNotifyAddr)
{
    // Post仅需向对端寄存器写入1
    stream.GetRtsq()->P2PWriteValue(rmtNotifyAddr, NOTIFY_RECORD_WRITE_VALUE);
}

void P2PTransportLiteImpl::BuildNotifyWaitTask(const StreamLite &stream, u32 notifyId)
{
    stream.GetRtsq()->NotifyWait(notifyId);
}

void P2PTransportLiteImpl::BuildP2PRead(const StreamLite &stream, const RmaBufferLite &loc, const Buffer &rmt)
{
    if (UNLIKELY(rmt.GetSize() != loc.GetSize())) {
        HCCL_ERROR("[P2PTransportLiteImpl]%s srcBuffer size[%llu] is not equal to distBuffer size[%llu], return",
        __func__, rmt.GetSize(), loc.GetSize());
        THROW<InternalException>("[P2PTransportLiteImpl]BuildP2PRead srcBuffer is not equal to distBuffer");
        return;
    }

    if (UNLIKELY(rmt.GetSize() == 0)) {
        HCCL_WARNING("[P2PTransportLiteImpl]%s srcBuffer size is 0, return", __func__);
        return;
    }

    HCCL_INFO("P2PTransportLiteImpl::Read remoteBuff[%s] localBuff[%s] reduce[%s]",
        rmt.Describe().c_str(), loc.Describe().c_str());
    // 传入数据大小不能超过 u32最大值， 需要进行切分
    u64 u32Max = UINT32_MAX;
    double countSplitingTimes = static_cast<double>(rmt.GetSize()) / static_cast<double>(u32Max);
    u64 splitingTimes = static_cast<int>(std::ceil(countSplitingTimes));
    u64 src = rmt.GetAddr();
    u64 dst = loc.GetAddr();
    u64 blockSize = u32Max;
    u64 offset = u32Max;
    for (u64 i = 0; i < splitingTimes; i++) {
        // 处理尾块数据
        if(i == splitingTimes - 1) {
            blockSize = rmt.GetSize() - u32Max * (splitingTimes - 1);
            offset = blockSize;
        }

        auto taskId = stream.GetRtsq()->GetTaskId();
        stream.GetRtsq()->SdmaCopy(src, dst, blockSize, 0);
        HCCL_INFO("P2PTransportLiteImpl::%s, srcA:0x%llx dstA:0x%llx,size=0x%llx", __func__, src, dst, blockSize);

        if (callback_) {
            TaskParam taskParam{};
            taskParam.taskType              = TaskParamType::TASK_SDMA;
            taskParam.beginTime             = ProfGetCurCpuTimestamp();
            taskParam.taskPara.DMA.src      = reinterpret_cast<void *>(src);
            taskParam.taskPara.DMA.dst      = reinterpret_cast<void *>(dst);
            taskParam.taskPara.DMA.size     = blockSize;
            taskParam.taskPara.DMA.notifyID = INVALID_VALUE_NOTIFYID;
            taskParam.taskPara.DMA.linkType = DfxLinkType::PCIE;
            taskParam.taskPara.DMA.dmaOp    = DmaOp::HCCL_DMA_READ;
            callback_(stream.GetSqId(), taskId, taskParam);
        } else {
            HCCL_WARNING("[P2PTransportLiteImpl][%s] callback_ is nullptr.", __func__);
        }
        src += offset;
        dst += offset;
    }
}

void P2PTransportLiteImpl::BuildP2PReadReduce(const StreamLite &stream, const RmaBufferLite &loc, const Buffer &rmt,
    const ReduceIn &reduceIn)
{
    if (UNLIKELY(rmt.GetSize() != loc.GetSize())) {
        HCCL_ERROR("[P2PTransportLiteImpl]%s srcBuffer size[%llu] is not equal to distBuffer size[%llu], return",
        __func__, rmt.GetSize(), loc.GetSize());
        THROW<InternalException>("[P2PTransportLiteImpl]BuildP2PReadReduce srcBuffer is not equal to distBuffer");
        return;
    }

    if (UNLIKELY(rmt.GetSize() == 0)) {
        HCCL_WARNING("[P2PTransportLiteImpl]%s srcBuffer size is 0, return", __func__);
        return;
    }

    HCCL_INFO("P2PTransportLiteImpl::ReadReduce remoteBuff[%s] localBuff[%s] reduce[%s]",
        rmt.Describe().c_str(), loc.Describe().c_str(), reduceIn.Describe().c_str());
    // 传入数据大小不能超过 u32最大值， 需要进行切分
    u64 u32Max = UINT32_MAX;
    double countSplitingTimes = static_cast<double>(rmt.GetSize()) / static_cast<double>(u32Max);
    u64 splitingTimes = static_cast<int>(std::ceil(countSplitingTimes));
    u64 src = rmt.GetAddr();
    u64 dst = loc.GetAddr();
    u64 blockSize = u32Max;
    u64 offset = u32Max;
    for (u64 i = 0; i < splitingTimes; i++) {
        // 处理尾块数据
        if(i == splitingTimes - 1) {
            blockSize = rmt.GetSize() - u32Max * (splitingTimes - 1);
            offset = blockSize;
        }

        auto taskId = stream.GetRtsq()->GetTaskId();
        stream.GetRtsq()->SdmaReduce(src, dst, blockSize, 0, reduceIn);

        HCCL_INFO("P2PTransportLiteImpl::%s, srcA:0x%llx dstA:0x%llx,size=0x%llx, reduceIn=%s",
            __func__, src, dst, blockSize, reduceIn.Describe());

        if (callback_) {
            TaskParam taskParam {};
            taskParam.taskType = TaskParamType::TASK_REDUCE_INLINE;
            taskParam.beginTime = ProfGetCurCpuTimestamp();
            taskParam.taskPara.Reduce.src = reinterpret_cast<void *>(src);
            taskParam.taskPara.Reduce.dst = reinterpret_cast<void *>(dst);
            taskParam.taskPara.Reduce.size = blockSize;
            taskParam.taskPara.Reduce.notifyID = INVALID_VALUE_NOTIFYID;
            taskParam.taskPara.Reduce.linkType = DfxLinkType::PCIE;
            taskParam.taskPara.Reduce.reduceOp = ConvertReduceOpToHcclReduceOp(reduceIn.reduceOp);
            taskParam.taskPara.Reduce.dataType = DataTypeToHcclDataType(reduceIn.dataType);
            callback_(stream.GetSqId(), taskId, taskParam);
        } else {
            HCCL_WARNING("[P2PTransportLiteImpl][%s] callback_ is nullptr.", __func__);
        }
        src += offset;
        dst += offset;
    }
}

void P2PTransportLiteImpl::Post(u32 index, const StreamLite &stream)
{
    auto taskId = stream.GetRtsq()->GetTaskId();
    auto rmtNotifyAddr = rmtNotifyVec[index].addr;
    BuildNotifyRecordTask(stream, rmtNotifyAddr);

    HCCL_INFO("P2PTransportLiteImpl::Post rmtNotifyAddr[0x%llx]", rmtNotifyAddr);

    if (callback_ == nullptr)
    {
        HCCL_WARNING("[P2PTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType                 = TaskParamType::TASK_NOTIFY_RECORD;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Notify.notifyID = rmtNotifyAddr; // 暂时用notifyAddr填充，考虑将notifyId透传给AICPU
    taskParam.taskPara.Notify.value    = 1;
    callback_(stream.GetSqId(), taskId, taskParam);
}

void P2PTransportLiteImpl::Wait(u32 index, const StreamLite &stream)
{
    auto taskId   = stream.GetRtsq()->GetTaskId();
    auto notifyId = locNotifyVec[index]->GetId();
    BuildNotifyWaitTask(stream, notifyId);

    HCCL_INFO("P2PTransportLiteImpl::Wait notifyId[%u]", notifyId);
    if (callback_ == nullptr)
    {
        HCCL_WARNING("[P2PTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType                 = TaskParamType::TASK_NOTIFY_WAIT;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Notify.notifyID = notifyId;
    taskParam.taskPara.Notify.value    = 1;
    callback_(stream.GetSqId(), taskId, taskParam);
}

void P2PTransportLiteImpl::Read(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream)
{
    BuildP2PRead(stream, loc, rmt);
}

void P2PTransportLiteImpl::ReadReduce(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                                     const StreamLite &stream)
{
    BuildP2PReadReduce(stream, loc, rmt, reduceIn);
}

void P2PTransportLiteImpl::BatchTransfer(const std::vector<RmaBufferLite> &loc, const std::vector<Buffer> &rmt,
    const std::vector<BaseTransportLiteImpl::TransferOp> &transferOp, const StreamLite &stream)
{
    if (UNLIKELY(loc.empty())) {
        return;
    }
    u32 insNum = loc.size();
    for (u32 i = 0; i < insNum; i++) {
        if (transferOp[i].transType == TransferType::WRITE) {
            HCCL_ERROR("[P2PTransportLiteImpl][BatchTransfer]does not support WRITE operation");
            THROW<InternalException>("[P2PTransportLiteImpl]BatchTransfer not support WRITE");
        } else if (transferOp[i].transType == TransferType::READ) {
            if (transferOp[i].reduceIn.reduceOp == ReduceOp::INVALID) {
                BuildP2PRead(stream, loc[i], rmt[i]);
            } else {
                BuildP2PReadReduce(stream, loc[i], rmt[i], transferOp[i].reduceIn);
            }
        }
    }
}
} // namespace Hccl
