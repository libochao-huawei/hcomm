/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ub_transport_lite_impl.h"
#include "binary_stream.h"
#include "ub_conn_lite.h"
#include "ub_conn_lite_mgr.h"
#include "exception_util.h"
#include "internal_exception.h"
#include "sal.h"

namespace Hccl {
constexpr u32 UB_WQE_BB_SIZE       = 64;  // 一个WQE BB是64Byte
constexpr u32 UB_WQE_MAX_SIZE      = 128; // 针对WriteWithNotify类型WQE，最大是128Byte
constexpr u32 UB_INLINE_WRITE_SIZE = 4;
constexpr u32 UB_RELAX_ORDER       = 0X01; // Relax Order表示当前SQE与后续Strong Order SQE有保序要求
constexpr u32 UB_STRONG_ORDER      = 0X02; // Strong Order表示当前SQE有保序要求，该SQE不能超越前面的Relax Order SQE
constexpr u32 UB_NO_COMPLETION     = 0;    // 表示当前报文和前面报文没有completion序要求，报文对应的CQE可以乱序上报
constexpr u32 UB_COMPLETION        = 1;    // 表示当前报文和前面报文有completion序要求，报文对应的CQE需要保序上报
UbTransportLiteImpl::UbTransportLiteImpl(
    std::vector<char> &uniqueId, std::function<void(u32 streamId, u32 taskId, const TaskParam &taskParam)> callback)
{
    callback_ = callback;
    // [header...][notifyUniqueId...][rmtNotifyUniqueId...][rmtBufferUniqueIds...]
    BinaryStream binaryStream(uniqueId);
    u32          theType;
    binaryStream >> theType;
    binaryStream >> notifyNum;
    binaryStream >> bufferNum;
    binaryStream >> connNum;

    std::vector<char> notifyUniqueIds;
    binaryStream >> notifyUniqueIds;
    ParseLocNotifyVec(notifyUniqueIds);

    std::vector<char> rmtNotifyUniqueIds;
    binaryStream >> rmtNotifyUniqueIds;
    ParseRmtBufferVec(rmtNotifyUniqueIds, rmtNotifyVec, RmaUbBufType::NOTIFY);

    std::vector<char> rmtBufferUniqueIds;
    binaryStream >> rmtBufferUniqueIds;
    ParseRmtBufferVec(rmtBufferUniqueIds, rmtBufferVec, RmaUbBufType::BUFFER);

    std::vector<char> connUniqueIds;
    binaryStream >> connUniqueIds;
    ParseConnVec(connUniqueIds);
}
UbTransportLiteImpl::UbTransportLiteImpl(std::vector<char> &uniqueId)
{
    BinaryStream binaryStream(uniqueId);
    u32          theType;
    binaryStream >> theType;
    binaryStream >> notifyNum;
    binaryStream >> bufferNum;
    binaryStream >> connNum;
 
    std::vector<char> notifyUniqueIds;
    binaryStream >> notifyUniqueIds;
    ParseLocNotifyVec(notifyUniqueIds);
 
    std::vector<char> rmtNotifyUniqueIds;
    binaryStream >> rmtNotifyUniqueIds;
    ParseRmtBufferVec(rmtNotifyUniqueIds, rmtNotifyVec, RmaUbBufType::NOTIFY);
 
    std::vector<char> locBufferUniqueIds;
    binaryStream >> locBufferUniqueIds;
    ParseLocBufferVec(locBufferUniqueIds, locBufferVec, RmaUbBufType::BUFFER);
 
    std::vector<char> rmtBufferUniqueIds;
    binaryStream >> rmtBufferUniqueIds;
    ParseRmtBufferVec(rmtBufferUniqueIds, rmtBufferVec, RmaUbBufType::BUFFER);

    std::vector<char> connUniqueIds;
    binaryStream >> connUniqueIds;
    ParseConnVec(connUniqueIds);
}

UbTransportLiteImpl::~UbTransportLiteImpl()
{
    for (auto &it : connUniqueIdVec) {
       DECTOR_TRY_CATCH("UbTransportLiteImpl",  UbConnLiteMgr::GetInstance().Clear(it));
    }
}

std::string UbTransportLiteImpl::Describe() const
{
    std::string desc = "UbTransportLiteImpl[";

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

    idx = 0;
    desc += "], connVec=[";
    for (auto &it : connVec) {
        desc += StringFormat("idx=%u, %s;", idx, it->Describe().c_str());
        idx++;
    }

    desc += "]]";
    return desc;
}

void UbTransportLiteImpl::ParseLocNotifyVec(std::vector<char> &data)
{
    if (notifyNum == 0) {
        HCCL_WARNING("UbTransportLiteImpl::ParseLocNotifyVec num is 0");
        return;
    }
    u32 notifySizePerDto = data.size() / notifyNum;

    for (u32 idx = 0; idx < notifyNum; idx++) {
        auto              start = data.begin() + idx * notifySizePerDto;
        auto              end   = start + notifySizePerDto;
        std::vector<char> dto(start, end);
        locNotifyVec.push_back(std::make_unique<NotifyLite>(dto));
        HCCL_INFO("locNotify idx=%u, %s", idx, locNotifyVec.back()->Describe().c_str());
    }
}

void UbTransportLiteImpl::ParseRmtBufferVec(std::vector<char> &data, RmtUbBufLiteVec &vec, RmaUbBufType rmtType) const
{
    u32 num = 0;
    if (rmtType == RmaUbBufType::NOTIFY) {
        num = notifyNum;
    } else {
        num = bufferNum;
    }

    if (num == 0) {
        HCCL_WARNING("UbTransportLiteImpl::ParseRmtBufferVec %s num is 0", rmtType.Describe().c_str());
        return;
    }

    u32 rmtBufferSizePerDto = data.size() / num;
    HCCL_INFO("Parse %s num=%u, sizePerDto=%u", rmtType.Describe().c_str(), num, rmtBufferSizePerDto);
    BinaryStream binaryStream(data);

    for (u32 idx = 0; idx < num; idx++) {
        RmtUbBufLite ubBufLite;
        binaryStream >> ubBufLite.addr;
        binaryStream >> ubBufLite.size;
        binaryStream >> ubBufLite.tokenId;
        binaryStream >> ubBufLite.tokenValue;
        HCCL_INFO("idx=%u, %s %s", idx, rmtType.Describe().c_str(), ubBufLite.Describe().c_str());
        vec.push_back(ubBufLite);
    }
}

void UbTransportLiteImpl::ParseLocBufferVec(std::vector<char> &data, LocUbBufLiteVec &vec, RmaUbBufType rmtType) const
{
    u32 num = 0;
    if (rmtType == RmaUbBufType::NOTIFY) {
        num = notifyNum;
    } else {
        num = bufferNum;
    }
 
    if (num == 0) {
        HCCL_WARNING("UbTransportLiteImpl::ParseLocBufferVec %s num is 0", rmtType.Describe().c_str());
        return;
    }
 
    u32 rmtBufferSizePerDto = data.size() / num;
    HCCL_INFO("Parse %s num=%u, sizePerDto=%u", rmtType.Describe().c_str(), num, rmtBufferSizePerDto);
    BinaryStream binaryStream(data);
 
    for (u32 idx = 0; idx < num; idx++) {
        LocUbBufLite ubBufLite;
        binaryStream >> ubBufLite.addr;
        binaryStream >> ubBufLite.size;
        binaryStream >> ubBufLite.tokenId;
        binaryStream >> ubBufLite.tokenValue;
        HCCL_INFO("idx=%u, %s %s", idx, rmtType.Describe().c_str(), ubBufLite.Describe().c_str());
        vec.push_back(ubBufLite);
    }
}

void UbTransportLiteImpl::ParseConnVec(std::vector<char> &data)
{
    if (connNum == 0) {
        HCCL_WARNING("UbTransportLiteImpl::ParseConnVec num is 0");
        return;
    }
    u32 connSizePerDto = data.size() / connNum;
    HCCL_INFO("Parse ConnVec num=%u, connSizePerDto=%u", connNum, connSizePerDto);
    for (u32 idx = 0; idx < connNum; idx++) {
        auto              start = data.begin() + idx * connSizePerDto;
        auto              end   = start + connSizePerDto;
        std::vector<char> connUniqueId(start, end);
        connUniqueIdVec.push_back(connUniqueId);
        // connLite的复用由 ubConnLiteMgr管理
        auto lite = UbConnLiteMgr::GetInstance().Get(connUniqueId);
        connVec.push_back(lite);
        HCCL_INFO("[%s]idx=%u, %s", __func__, idx, lite->Describe().c_str());
    }
}

void UbTransportLiteImpl::BuildUbDbSendTask(const StreamLite &stream, const UbJettyLiteId &jettyLiteId, u32 pi)
{
    stream.GetRtsq()->UbDbSend(jettyLiteId, pi);
}

void UbTransportLiteImpl::BuildNotifyWaitTask(const StreamLite &stream, u32 notifyId)
{
    stream.GetRtsq()->NotifyWait(notifyId);
}

Buffer UbTransportLiteImpl::GetRmtBuffer(u32 index)
{
    if (UNLIKELY(index >= rmtBufferVec.size())) {
        THROW<InternalException>(StringFormat("UbTransportLiteImpl::GetRmtBuffer out-of-bounds. index=%u, size=%u",
            index, rmtBufferVec.size()));
    }
    return Buffer(rmtBufferVec[index].addr, rmtBufferVec[index].size);
}

RmtRmaBufSliceLite UbTransportLiteImpl::GetRmtNotifySliceLite(u32 index)
{
    RmtUbBufLite &lite = rmtNotifyVec[index];
    // ub conn lite 不关心rkey , rkey 设定为0
    return RmtRmaBufSliceLite(lite.addr, lite.size, 0, lite.tokenId, lite.tokenValue);
}

RmtRmaBufSliceLite UbTransportLiteImpl::GetRmtRmaBufSliceLite(const Buffer &rmtBuf)
{
    for (auto &it : rmtBufferVec) {
        Buffer buf(it.addr, it.size);
        if (buf.Contains(rmtBuf.GetAddr(), rmtBuf.GetSize())) {
            // ub conn lite 不关心rkey , rkey 设定为0
            return RmtRmaBufSliceLite(rmtBuf.GetAddr(), rmtBuf.GetSize(), 0, it.tokenId, it.tokenValue);
        }
    }
    MACRO_THROW(InternalException, StringFormat("%s is not in current transport", rmtBuf.Describe().c_str()));
}

RmtRmaBufSliceLite UbTransportLiteImpl::GetRmtRmaBufSliceLite(const RmaBufferLite &lite) const
{
    return RmtRmaBufSliceLite(lite.GetAddr(), lite.GetSize(), 0, lite.GetTokenId() , lite.GetTokenValue());
}

HcclResult UbTransportLiteImpl::BuildLocRmaBufferLite(const uintptr_t addr, const size_t size, RmaBufferLite &rmaBufferLite) const
{
    HCCL_INFO("[UbTransportLiteImpl::%s] start to find addr[0x%llx], size[0x%llx] in locBufferVec, whose size is %zu. ",
        __func__, addr, size, locBufferVec.size());
    if (locBufferVec.empty()) {
        HCCL_ERROR("[UbTransportLiteImpl::%s] locBufferVec is empty.", __func__);
        return HCCL_E_INTERNAL;
    }

    bool isAddrInRange = false;
    for (auto &it : locBufferVec) {
        Buffer iterBuf(it.addr, it.size);
        HCCL_INFO("[UbTransportLiteImpl::%s] comparing to locBuffer: %s.", __func__, iterBuf.Describe().c_str());
        if (iterBuf.Contains(addr, size)) {
            rmaBufferLite = RmaBufferLite(addr, size, it.tokenId, it.tokenValue);
            isAddrInRange = true;
            break;
        }
    }

    if (!isAddrInRange) {
        HCCL_WARNING("[UbTransportLiteImpl::%s] addr[0x%llx], size[0x%llx] not in any range of locBufferVec. The token of the first locBuffer is used.",
            __func__, addr, size);
        rmaBufferLite = RmaBufferLite(addr, size, locBufferVec[0].tokenId, locBufferVec[0].tokenValue);
        return HCCL_SUCCESS;
    }

    return HCCL_SUCCESS;
}

void UbTransportLiteImpl::ClearConnOut()
{
    wqeData.clear();
    wqeData.resize(UB_WQE_MAX_SIZE);
    connOut.data     = (u8 *)wqeData.data();
    connOut.dataSize = sizeof(wqeData);
}

// 检查connection不能为空
void UbTransportLiteImpl::CheckConnVec(const std::string &desc)
{
    if (UNLIKELY(connVec.size() == 0)) {
        THROW<InternalException>(StringFormat("connVec size is 0 %s", desc.c_str()));
    }

    u32 idx = 0;
    for (auto &it : connVec) {
        if (UNLIKELY(it == nullptr)) {
            THROW<InternalException>(StringFormat("connVec[%u] is null %s", idx, desc.c_str()));
        }
        idx++;
    }
}

RmaBufSliceLite UbTransportLiteImpl::GetRmaBufSlicelite(const RmaBufferLite &lite) const
{
    // ub conn lite 不关心rkey , rkey 设定为0
    return RmaBufSliceLite(lite.GetAddr(), lite.GetSize(), 0, lite.GetTokenId());
}

void UbTransportLiteImpl::Post(u32 index, const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    if (index == 1) { // PostFin场景
        cfg.cqeEn     = true;
        cfg.placeOdr  = UB_STRONG_ORDER;
        cfg.compOrder = UB_COMPLETION;
    }
    u32           inlineData = 1;
    CheckConnVec("UbTransportLiteImpl::Post"); // 待修改优化, 检查connection
    auto taskId = stream.GetRtsq()->GetTaskId();
    // 当前使用1个connection，下标为0
    auto rmtBuffSliceLite = GetRmtNotifySliceLite(index);
    connVec[0]->InlineWrite(reinterpret_cast<u8 *>(&inlineData), UB_INLINE_WRITE_SIZE, rmtBuffSliceLite,
                            cfg, stream, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    HCCL_INFO("UbTransportLiteImpl::Post notifyId[0x%llx], pi=%u", rmtBuffSliceLite.GetAddr(), connOut.pi);
 
    if (callback_ == nullptr)
    {
        HCCL_WARNING("[UbTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType                 = TaskParamType::TASK_UB_INLINE_WRITE;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.DMA.dst         = reinterpret_cast<void*>(rmtBuffSliceLite.GetAddr());
    taskParam.taskPara.DMA.size        = rmtBuffSliceLite.GetSize();
    taskParam.taskPara.DMA.notifyID    = rmtBuffSliceLite.GetAddr();
    taskParam.taskPara.DMA.notifyValue = 1;
    taskParam.taskPara.DMA.linkType    = DfxLinkType::UB;
    taskParam.taskPara.DMA.dmaOp       = DmaOp::HCCL_DMA_WRITE;
    taskParam.taskPara.DMA.locEid      = GetLocEid();
    taskParam.taskPara.DMA.rmtEid      = GetRmtEid();

    callback_(stream.GetSqId(), taskId, taskParam);
}

void UbTransportLiteImpl::Wait(u32 index, const StreamLite &stream)
{
    auto taskId   = stream.GetRtsq()->GetTaskId();
    auto notifyId = locNotifyVec[index]->GetId();
    BuildNotifyWaitTask(stream, notifyId);

    if (callback_ == nullptr)
    {
        HCCL_WARNING("[UbTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType                 = TaskParamType::TASK_NOTIFY_WAIT;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Notify.notifyID = notifyId;
    taskParam.taskPara.Notify.value    = 1;
    callback_(stream.GetSqId(), taskId, taskParam);
}

void UbTransportLiteImpl::ProfilingProcess(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, const StreamLite &stream,
                                           DmaOp dmaOp, u32 taskId)
{
    if (callback_ == nullptr)
    {
        HCCL_WARNING("[UbTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType = TaskParamType::TASK_UB;
    taskParam.beginTime = ProfGetCurCpuTimestamp();
    taskParam.taskPara.DMA.src      = reinterpret_cast<void *>(loc.GetAddr());
    taskParam.taskPara.DMA.dst      = reinterpret_cast<void *>(rmt.GetAddr());
    taskParam.taskPara.DMA.size     = loc.GetSize();
    taskParam.taskPara.DMA.notifyID = INVALID_VALUE_NOTIFYID;
    taskParam.taskPara.DMA.notifyValue = 0xffffffff;
    taskParam.taskPara.DMA.linkType = DfxLinkType::UB;
    taskParam.taskPara.DMA.dmaOp    = dmaOp;
    taskParam.taskPara.DMA.locEid = GetLocEid();
    taskParam.taskPara.DMA.rmtEid = GetRmtEid();
    callback_(stream.GetSqId(), taskId, taskParam);
}

void UbTransportLiteImpl::Read(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    auto taskId = stream.GetRtsq()->GetTaskId();
    CheckConnVec("UbTransportLiteImpl::Read"); // 待修改优化, 检查connection
    // 当前使用1个connection,下标为0
    auto locRmaBufSlicelite = GetRmaBufSlicelite(loc);
 	auto rmtRmaBufSlicelite = GetRmtRmaBufSliceLite(rmt);
    connVec[0]->Read(locRmaBufSlicelite, rmtRmaBufSlicelite, cfg, stream, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    ProfilingProcess(locRmaBufSlicelite, rmtRmaBufSlicelite, stream, DmaOp::HCCL_DMA_READ, taskId);
}

void UbTransportLiteImpl::Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    auto taskId = stream.GetRtsq()->GetTaskId();
    CheckConnVec("UbTransportLiteImpl::Write"); // 待修改优化, 检查connection
    // 当前使用1个connection，下标为0
    auto locRmaBufSlicelite = GetRmaBufSlicelite(loc);
 	auto rmtRmaBufSlicelite = GetRmtRmaBufSliceLite(rmt);
    connVec[0]->Write(locRmaBufSlicelite, rmtRmaBufSlicelite, cfg, stream, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    ProfilingProcess(locRmaBufSlicelite, rmtRmaBufSlicelite, stream, DmaOp::HCCL_DMA_WRITE, taskId);
}

void UbTransportLiteImpl::ReadReduce(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                                     const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    auto taskId = stream.GetRtsq()->GetTaskId();
    CheckConnVec("UbTransportLiteImpl::ReadReduce"); // 待修改优化, 检查connection
    // 当前使用1个connection，下标为0
    auto locRmaBufSlicelite = GetRmaBufSlicelite(loc);
 	auto rmtRmaBufSlicelite = GetRmtRmaBufSliceLite(rmt);
    connVec[0]->ReadReduce(reduceIn, locRmaBufSlicelite, rmtRmaBufSlicelite, stream, cfg, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    ReduceProfilingProcess(locRmaBufSlicelite, rmtRmaBufSlicelite, reduceIn, stream, taskId);
}

void UbTransportLiteImpl::ReduceProfilingProcess(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt,
                                                 const ReduceIn &reduceIn, const StreamLite &stream, u32 taskId)
{
    if (callback_ == nullptr)
    {
        HCCL_WARNING("[UbTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam {};
    taskParam.taskType = TaskParamType::TASK_UB_REDUCE_INLINE;
    taskParam.beginTime = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Reduce.src = reinterpret_cast<void *>(loc.GetAddr());
    taskParam.taskPara.Reduce.dst = reinterpret_cast<void *>(rmt.GetAddr());
    taskParam.taskPara.Reduce.size = loc.GetSize();
    taskParam.taskPara.Reduce.notifyID = INVALID_VALUE_NOTIFYID;
    taskParam.taskPara.Reduce.notifyValue = 1;
    taskParam.taskPara.Reduce.linkType = DfxLinkType::UB;
    taskParam.taskPara.Reduce.reduceOp = ConvertReduceOpToHcclReduceOp(reduceIn.reduceOp);
    taskParam.taskPara.Reduce.dataType = DataTypeToHcclDataType(reduceIn.dataType);
    taskParam.taskPara.Reduce.locEid   = GetLocEid();
 	taskParam.taskPara.Reduce.rmtEid   = GetRmtEid();
    callback_(stream.GetSqId(), taskId, taskParam);
}

void UbTransportLiteImpl::WriteReduce(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                                      const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    auto taskId = stream.GetRtsq()->GetTaskId();
    CheckConnVec("UbTransportLiteImpl::WriteReduce"); // 待修改优化, 检查connection
    // 当前使用1个connection，下标为0
    auto locRmaBufSlicelite = GetRmaBufSlicelite(loc);
 	auto rmtRmaBufSlicelite = GetRmtRmaBufSliceLite(rmt);
    connVec[0]->WriteReduce(reduceIn.dataType, reduceIn.reduceOp, locRmaBufSlicelite, stream,
                            rmtRmaBufSlicelite, cfg, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    ReduceProfilingProcess(locRmaBufSlicelite, rmtRmaBufSlicelite, reduceIn, stream, taskId);
}

void UbTransportLiteImpl::BatchTransfer(const std::vector<RmaBufferLite> &loc, const std::vector<Buffer> &rmt,
    const std::vector<BaseTransportLiteImpl::TransferOp> &transferOp, const StreamLite &stream)
{
    if (UNLIKELY(loc.empty())) {
        return;
    }
    ClearConnOut();
    SqeConfigLite cfg;
    auto taskId = stream.GetRtsq()->GetTaskId();
    CheckConnVec("UbTransportLiteImpl::BatchTransfer"); // 待修改优化, 检查connection
    u32 insNum = loc.size();
    for (u32 i = 0; i < insNum; i++) {
        cfg.cqeEn     = (i == insNum - 1) ? true : false; // 返回最后一个sqe的cqe
        cfg.placeOdr  = UB_RELAX_ORDER;
        cfg.compOrder = UB_NO_COMPLETION;

        auto localBuffer  = GetRmaBufSlicelite(loc[i]);
        auto remoteBuffer = GetRmtRmaBufSliceLite(rmt[i]);

        if (transferOp[i].transType == TransferType::WRITE) {
            if (transferOp[i].reduceIn.reduceOp == ReduceOp::INVALID) {
                connVec[0]->Write(localBuffer, remoteBuffer, cfg, stream, connOut); // 当前只有一个connection，对应一个jetty
            } else {
                connVec[0]->WriteReduce(transferOp[i].reduceIn.dataType, transferOp[i].reduceIn.reduceOp, localBuffer,
                    stream, remoteBuffer, cfg, connOut);
            }
        } else if (transferOp[i].transType == TransferType::READ) {
            if (transferOp[i].reduceIn.reduceOp == ReduceOp::INVALID) {
                connVec[0]->Read(localBuffer, remoteBuffer, cfg, stream, connOut); // 当前只有一个connection，对应一个jetty
            } else {
                connVec[0]->ReadReduce(transferOp[i].reduceIn, localBuffer, remoteBuffer, stream, cfg, connOut);
            }
        }
    }
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    if (transferOp[insNum - 1].reduceIn.reduceOp == ReduceOp::INVALID) {
        DmaOp dmaOp = DmaOp::HCCL_DMA_WRITE;
        if (transferOp[insNum - 1].transType == TransferType::READ) {
            dmaOp = DmaOp::HCCL_DMA_READ;
        }
        ProfilingProcess(GetRmaBufSlicelite(loc[insNum - 1]), GetRmtRmaBufSliceLite(rmt[insNum - 1]), stream, dmaOp, taskId);
    } else {
        ReduceProfilingProcess(GetRmaBufSlicelite(loc[insNum - 1]), GetRmtRmaBufSliceLite(rmt[insNum - 1]), transferOp[insNum - 1].reduceIn, stream, taskId);
    }
}

void UbTransportLiteImpl::WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const WithNotifyIn &withNotify,
                                          const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    u64           notifyData = 1; // 普通notify，固定1
    auto taskId = stream.GetRtsq()->GetTaskId();
    CheckConnVec("UbTransportLiteImpl::WriteWithNotify"); // 待修改优化, 检查connection
    // 当前使用1个connection，下标为0
    auto locRmaBufSlicelite = GetRmaBufSlicelite(loc);
 	auto rmtRmaBufSlicelite = GetRmtRmaBufSliceLite(rmt);
 	auto rmtNotifySliceLite = GetRmtNotifySliceLite(withNotify.index_);
    connVec[0]->WriteWithNotify(locRmaBufSlicelite, rmtRmaBufSlicelite, cfg, connOut,
                                rmtNotifySliceLite, stream, notifyData);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    if (callback_ == nullptr)
    {
        HCCL_WARNING("[UbTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType              = TaskParamType::TASK_WRITE_WITH_NOTIFY;
    taskParam.beginTime             = ProfGetCurCpuTimestamp();
    taskParam.taskPara.DMA.src      = reinterpret_cast<void *>(locRmaBufSlicelite.GetAddr());
    taskParam.taskPara.DMA.dst      = reinterpret_cast<void *>(rmtRmaBufSlicelite.GetAddr());
    taskParam.taskPara.DMA.size     = locRmaBufSlicelite.GetSize();
    taskParam.taskPara.DMA.notifyID = rmtNotifySliceLite.GetAddr();
    taskParam.taskPara.DMA.notifyValue = 1;
    taskParam.taskPara.DMA.linkType = DfxLinkType::UB;
    taskParam.taskPara.DMA.dmaOp    = DmaOp::HCCL_DMA_WRITE;
    taskParam.taskPara.DMA.locEid = GetLocEid();
    taskParam.taskPara.DMA.rmtEid = GetRmtEid();
    callback_(stream.GetSqId(), taskId, taskParam);
}

void UbTransportLiteImpl::WriteReduceWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                                                const WithNotifyIn &withNotify, const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    u64           notifyData = 1;                               // 普通notify，固定1
    CheckConnVec("UbTransportLiteImpl::WriteReduceWithNotify"); // 待修改优化, 检查connection
    auto taskId = stream.GetRtsq()->GetTaskId();
    // 当前使用1个connection，下标为0
    auto locRmaBufSlicelite = GetRmaBufSlicelite(loc);
 	auto rmtRmaBufSlicelite = GetRmtRmaBufSliceLite(rmt);
 	auto rmtNotifySliceLite = GetRmtNotifySliceLite(withNotify.index_);
    connVec[0]->WriteReduceWithNotify(reduceIn.dataType, reduceIn.reduceOp, locRmaBufSlicelite,
                                      rmtRmaBufSlicelite, cfg, stream, connOut, rmtNotifySliceLite,
                                      notifyData);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);

    if (callback_ == nullptr)
    {
        HCCL_WARNING("[UbTransportLiteImpl] callback_ is nullptr.");
        return;
    }

    TaskParam taskParam{};
    taskParam.taskType                 = TaskParamType::TASK_WRITE_REDUCE_WITH_NOTIFY;
    taskParam.beginTime                = ProfGetCurCpuTimestamp();
    taskParam.taskPara.Reduce.src      = reinterpret_cast<void *>(locRmaBufSlicelite.GetAddr());
    taskParam.taskPara.Reduce.dst      = reinterpret_cast<void *>(rmtRmaBufSlicelite.GetAddr());
    taskParam.taskPara.Reduce.size     = locRmaBufSlicelite.GetSize();
    taskParam.taskPara.Reduce.notifyID = rmtNotifySliceLite.GetAddr();
    taskParam.taskPara.Reduce.notifyValue = 1;
    taskParam.taskPara.Reduce.linkType = DfxLinkType::UB;
    taskParam.taskPara.Reduce.reduceOp = ConvertReduceOpToHcclReduceOp(reduceIn.reduceOp);
    taskParam.taskPara.Reduce.dataType = DataTypeToHcclDataType(reduceIn.dataType);
    taskParam.taskPara.Reduce.locEid   = GetLocEid();
    taskParam.taskPara.Reduce.rmtEid   = GetRmtEid();
    callback_(stream.GetSqId(), taskId, taskParam);
}

void UbTransportLiteImpl::BatchOneSidedRead(const vector<RmaBufSliceLite> &loc, const vector<RmtRmaBufSliceLite> &rmt,
        const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    CheckConnVec("UbTransportLiteImpl::BatchOneSidedRead");
    // 当前使用1个connection，下标为0
    connVec[0]->BatchOneSidedRead(loc, rmt, cfg, stream, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);
}

void UbTransportLiteImpl::BatchOneSidedWrite(const vector<RmaBufSliceLite> &loc, const vector<RmtRmaBufSliceLite> &rmt,
        const StreamLite &stream)
{
    ClearConnOut();
    SqeConfigLite cfg;
    CheckConnVec("UbTransportLiteImpl::BatchOneSidedRead");
    // 当前使用1个connection，下标为0
    connVec[0]->BatchOneSidedWrite(loc, rmt, cfg, stream, connOut);
    BuildUbDbSendTask(stream, connVec[0]->GetUbJettyLiteId(), connOut.pi);
}

 
Eid UbTransportLiteImpl::GetLocEid() const
{
    Eid eid{};
    if (!connVec.empty()) {
        return connVec[0]->GetLocEid();
    }
    return eid;
}

Eid UbTransportLiteImpl::GetRmtEid() const
{
    Eid eid{};
    if (!connVec.empty()) {
        return connVec[0]->GetRmtEid();
    }
    return eid;
}
} // namespace Hccl
