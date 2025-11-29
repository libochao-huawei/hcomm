/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <memory>
#include "hccl_common.h"
#include "stream_pub.h"
#include "rt_external.h"
#include "aicpu/aicpu_hccl_sqcqv1.h"
#include "aicpu/aicpu_hccl_sqcqv2.h"
#include "sal.h"
#include "config_plf_log.h"
#include "dlhal_function.h"
#include "adapter_hal_pub.h"
#include "dispatcher_aicpu.h"

namespace hccl {
constexpr uint64_t NANOSECOND_TO_SECOND = 1000000000U;
constexpr uint16_t TURN_LEFT_SHIFT_BIT = 16;
constexpr uint32_t HCCL_PER_LAUNCH_SQE_CNT = 128U; // 每编排N个SQE，做一次launchtask

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
HcclResult HcclDispatcherAicpuInit(HcclDispatcher *dispatcher, const u32 devPhyId, DispatcherType type)
{
    CHK_PTR_NULL(dispatcher);
    DispatcherPub *pDispatcher = nullptr;
    if (type == DispatcherType::DISPATCHER_AICPU) {
        pDispatcher = new (std::nothrow) DispatcherAiCpu(devPhyId);
    } else {
        HCCL_ERROR("[HcclCommAicpu][HcclDispatcherInit] Not support the dispatcher type[%d]", type);
        return HCCL_E_NOT_SUPPORT;
    }
    CHK_PTR_NULL(pDispatcher);
    HcclResult ret = pDispatcher->Init();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HcclCommAicpu][HcclDispatcherInit] Dispatcher init failed, type[%d]", type);
        delete pDispatcher;
        pDispatcher = nullptr;
        return ret;
    }
    *dispatcher = pDispatcher;
    return HCCL_SUCCESS;
}
#ifdef __cplusplus
}
#endif // __cplusplus

DispatcherAiCpu::DispatcherAiCpu(const u32 devPhyId) // depreated
    : DispatcherPub(INVALID_INT)
{
    aicpuInfo_.devId = devPhyId;
}

DispatcherAiCpu::~DispatcherAiCpu() {}

HcclResult DispatcherAiCpu::Init()
{
    CHK_RET(DlHalFunction::GetInstance().DlHalFunctionInit());
    CHK_RET(hrtHalGetDeviceType(aicpuInfo_.devId, aicpuInfo_.devType));
    CHK_RET(hrtHalGetDeviceInfo(aicpuInfo_.devId, MODULE_TYPE_SYSTEM, INFO_TYPE_PHY_CHIP_ID,
        reinterpret_cast<int64_t*>(&aicpuInfo_.chipId)));

    if (aicpuInfo_.devType == DevType::DEV_TYPE_310P1 || aicpuInfo_.devType == DevType::DEV_TYPE_310P3) {
        addOneNotifyWaitSqe_ = AddOneNotifyWaitSqeV2;
        addOneRecordSqe_ = AddOneRecordSqeV2;
        addOneWriteValueRecordSqe_ = AddOneWriteValueRecordSqeV2;
        addOneMemcpySqe_ = AddOneMemcpySqeV2;
        addOneEventResetSqe_ = AddOneEventResetSqeV2;
        addOneEventRecordSqe_ = AddOneEventRecordSqeV2;
        addOneEventWaitSqe_ = AddOneEventWaitSqeV2;
    } else {
        addOneNotifyWaitSqe_ = AddOneNotifyWaitSqeV1;
        addOneRecordSqe_ = AddOneRecordSqeV1;
        addOneWriteValueRecordSqe_ = AddOneWriteValueRecordSqeV1;
        addOneMemcpySqe_ = AddOneMemcpySqeV1;
        addOneEventResetSqe_ = AddOneEventResetSqeV1;
        addOneEventRecordSqe_ = AddOneEventRecordSqeV1;
        addOneEventWaitSqe_ = AddOneEventWaitSqeV1;
        addOneRdmaDbSendSqe_ = AddOneRdmaDbSendSqeV1;
        addOneFlipPlaceHolderSqe_ = AddOneFlipPlaceHolderSqeV1;
        CHK_PTR_NULL(addOneRdmaDbSendSqe_);
        CHK_PTR_NULL(addOneFlipPlaceHolderSqe_);
    }

    CHK_PTR_NULL(addOneNotifyWaitSqe_);
    CHK_PTR_NULL(addOneRecordSqe_);
    CHK_PTR_NULL(addOneWriteValueRecordSqe_);
    CHK_PTR_NULL(addOneMemcpySqe_);
    CHK_PTR_NULL(addOneEventResetSqe_);
    CHK_PTR_NULL(addOneEventRecordSqe_);
    CHK_PTR_NULL(addOneEventWaitSqe_);

    CHK_RET(GetNotifyMaxWaitTime());
    notifySize_ = (aicpuInfo_.devType == DevType::DEV_TYPE_910B || aicpuInfo_.devType == DevType::DEV_TYPE_910_93) ?
        4 : 8; // 和hrtGetNotifySize接口保持一致，910B和910_93的notify寄存器大小为4，其他芯片为8
    InitTimeOutConfig();
    HCCL_INFO("%s success, devId:%u, devType:%d, chipId:%lld",
        __func__, aicpuInfo_.devId, aicpuInfo_.devType, aicpuInfo_.chipId);
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::WaitValue(hccl::Stream &stream, u64 waitAddr, u64 valueAddr, bool reset)
{
    u32 turnNum= *(reinterpret_cast<u32*>(static_cast<uintptr_t>(valueAddr)));
    HCCL_DEBUG("[DispatcherAiCpu][WaitValue] turnNum %u", turnNum);
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;
    CHK_RET(stream.GetNextSqeBufferAddr(sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    const HcclComStreamInfo *streamInfo;
    CHK_RET(stream.GetStreamInfo(streamInfo));
    AddOneWaitStartSqe(streamInfo->actualStreamId, taskId, waitAddr, valueAddr,
        reset, reinterpret_cast<rtStarsCcoreWaitStartSqe_t *>(sqeBuffer), sqeTypeAddr);
    HcclSqeContext *sqeCtx = stream.GetSqeContextPtr();
    if (sqeCtx == nullptr) {
        HCCL_ERROR("[DispatcherAiCpu][WaitValue] AddCcoreWait sqeCtx is nullptr");
        return HCCL_E_INTERNAL;
    }
    sqeCtx->buffer.addInfo[taskId % hccl::HCCL_SQE_MAX_CNT] = 
        ((turnNum << TURN_LEFT_SHIFT_BIT) + static_cast<uint32_t>(reset));
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::WriteValue(hccl::Stream &stream, u64 writeAddr, u64 valueAddr)
{
    u32 turnNum= *(reinterpret_cast<u32*>(static_cast<uintptr_t>(valueAddr)));
    HCCL_DEBUG("[DispatcherAiCpu][WriteValue] turnNum %u", turnNum);
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;
    CHK_RET(stream.GetNextSqeBufferAddr(sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    const HcclComStreamInfo *streamInfo;
    CHK_RET(stream.GetStreamInfo(streamInfo));
    AddOneWriteValueStartSqe(streamInfo->actualStreamId, taskId, writeAddr, valueAddr, reinterpret_cast<rtStarsCcoreWriteValueSqe_t *>(sqeBuffer),
        sqeTypeAddr);
    HcclSqeContext *sqeCtx = stream.GetSqeContextPtr();
    if (sqeCtx == nullptr) {
        HCCL_ERROR("[DispatcherAiCpu][WriteValue] AddCcoreNotify sqeCtx is nullptr");
        return HCCL_E_INTERNAL;
    }
    sqeCtx->buffer.addInfo[taskId % hccl::HCCL_SQE_MAX_CNT] = turnNum;
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::SignalRecord(HcclRtNotify signal, hccl::Stream &stream, u32 userRank, u64 offset, s32 stage,
    bool inchip, u64 signalAddr, u32 notifyId)
{
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;
    CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr;
    dfxInfo->opRingBufferIdx = opRingBufferIdx_;
    dfxInfo->remoteRank = userRank;
    dfxInfo->notifyId = notifyId;
    if (inchip) {
        addOneRecordSqe_(streamInfo.actualStreamId, taskId, notifyId, sqeBuffer, sqeTypeAddr);
    } else {
        addOneWriteValueRecordSqe_(streamInfo.actualStreamId, taskId, signalAddr, sqeBuffer, sqeTypeAddr);
    }

    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: streamId[%d] remoteRank[%u] inchip[%d] devType[%d] notifyId[%u]",
        __func__, streamInfo.actualStreamId, userRank, inchip, aicpuInfo_.devType, notifyId);
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::SignalRecord(hccl::DeviceMem &dst, hccl::DeviceMem &src, hccl::Stream &stream,
    u32 remoteUserRank, hccl::LinkType inLinkType, u32 notifyId)
{
    // 参数有效性检查
    CHK_PRT_RET(src.size() == 0, HCCL_INFO("%s src size is 0, not need copy", __func__), HCCL_SUCCESS);
    CHK_PRT_RET(src == dst, HCCL_INFO("%s src and dst is same, not need copy", __func__), HCCL_SUCCESS);
    CHK_PRT_RET(dst.size() < src.size(),
        HCCL_ERROR("%s The size of dst is smaller than that of src. dst addr:%p, dst size:%llu, src addr:%p, "
        "src size:%llu", __func__, dst.ptr(), dst.size(), src.ptr(), src.size()),
        HCCL_E_PARA);
    CHK_PRT_RET(src.size() != notifySize_,
        HCCL_ERROR("%s src size[%llu] should be %llu in devType[%d]",
        __func__, src.size(), notifySize_, aicpuInfo_.devType), HCCL_E_PARA);

    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;
    aclrtReduceKind rtReduceOp = RK_MAP_TABLE[HCCL_REDUCE_RESERVED];
    uint8_t linkType = static_cast<uint8_t>(inLinkType);

    CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    AicpuDfxInfo * const dfxInfo = reinterpret_cast<AicpuDfxInfo * const>(sqeDfxInfoAddr);
    dfxInfo->opRingBufferIdx = opRingBufferIdx_;
    dfxInfo->remoteRank = remoteUserRank;
    dfxInfo->notifyId = notifyId;
    addOneMemcpySqe_(streamInfo.actualStreamId, taskId, src.ptr(), src.size() , ACL_FLOAT, rtReduceOp,
        dst.ptr(), 0, aicpuInfo_.ssid, aicpuInfo_.devId, aicpuInfo_.overflowAddr, linkType, sqeBuffer, sqeTypeAddr);

    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: linkType[%u] srcPtr[%p] srcSize[%llu] dstPtr[%p] taskId[%u] streamId[%u] remoteRank[%u] notifyId[%u]",
        __func__, linkType, src.ptr(), src.size(), dst.ptr(), taskId, streamInfo.actualStreamId, remoteUserRank, notifyId);
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::SignalWait(HcclRtNotify signal, Stream &stream, u32 userRank, u32 remoteUserRank, s32 stage,
    bool inchip, u32 notifyId, u32 timeOut)
{
    (void)timeOut;
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;
    CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr;
    dfxInfo->opRingBufferIdx = opRingBufferIdx_;
    dfxInfo->remoteRank = remoteUserRank;
    dfxInfo->notifyId = notifyId;

    if (inchip || (aicpuInfo_.devType != DevType::DEV_TYPE_310P1 && aicpuInfo_.devType != DevType::DEV_TYPE_310P3)) {
        addOneNotifyWaitSqe_(streamInfo.actualStreamId, taskId, notifyId, sqeBuffer, sqeTypeAddr, dfxTimeOutConfig_);
    } else {
        u32 notifyRevisedOffset = 15U; // eventid偏移15位后为1
        u32 notifyGetEventId = 0x3FFU; // 取低15位
        if ((notifyId >> notifyRevisedOffset) != 0) {
            addOneEventWaitSqe_(streamInfo.actualStreamId,
                (notifyId & notifyGetEventId), taskId, sqeBuffer, sqeTypeAddr);

            uint8_t *sqeBuffer1 = nullptr;
            uint8_t *sqeTypeAddr1 = nullptr;
            uint8_t *sqeDfxInfoAddr1 = nullptr;
            CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer1, sqeTypeAddr1, sqeDfxInfoAddr1, taskId));
            AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr1;
            dfxInfo->opRingBufferIdx = opRingBufferIdx_;
            dfxInfo->remoteRank = INVALID_VALUE_RANKID;

            u64 addr = 0;
            addOneEventResetSqe_(streamInfo.actualStreamId,
                (notifyId & notifyGetEventId), taskId, aicpuInfo_.devId, 0,
                addr, sqeBuffer1, sqeTypeAddr1);
        } else {
            HCCL_WARNING("%s SignalWait id is not event, please check %d", __func__, notifyId);
        }
    }

    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: streamId[%u] userRank[%u] remoteRank[%u] inchip[%d] devType[%d] notifyId[%u]",
        __func__, streamInfo.actualStreamId, userRank, remoteUserRank, inchip, aicpuInfo_.devType, notifyId);
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::MemcpyAsync(hccl::DeviceMem &dst, const hccl::DeviceMem &src, hccl::Stream &stream,
    u32 remoteUserRank, hccl::LinkType inLinkType)
{
    // 参数有效性检查
    if (src.size() == 0) {
        HCCL_INFO("%s src memory size is 0, not need copy.", __func__);
        return HCCL_SUCCESS;
    }

    if (src == dst) {
        HCCL_INFO("%s src memory and dst memory is same, not need copy.", __func__);
        return HCCL_SUCCESS;
    }

    if (dst.size() < src.size()) {
        HCCL_ERROR("%s The size of dst is smaller than that of src. dst addr[%p], dst size[%llu], "\
            "src addr[%p], src size[%llu]", __func__, dst.ptr(), dst.size(), src.ptr(), src.size());
        return HCCL_E_PTR;
    }
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();

    // 将数据按4GB切分循环处理
    uint64_t spiltLoop = 0;
    uint64_t addrOffset = 0;
    uint64_t countSplit = 0;
    uint64_t countSize = src.size();
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;
    HcclReduceOp redOp = HCCL_REDUCE_RESERVED;
    aclrtReduceKind rtReduceOp = RK_MAP_TABLE[redOp];

    if (countSize > HCCL_SDMA_MAX_COUNT_4GB) {
        spiltLoop = (countSize % HCCL_SDMA_MAX_COUNT_4GB) ? (countSize / HCCL_SDMA_MAX_COUNT_4GB) :
                                                            ((countSize / HCCL_SDMA_MAX_COUNT_4GB) - 1);
        HCCL_INFO("%s MemcpyAsync SDMA task countSize is bigger than 4GB"
            " and do segmentation splitloop[%llu]", __func__, spiltLoop);
    }
    uint8_t linkType = static_cast<uint8_t>(inLinkType);
    for (uint64_t index = 0; index <= spiltLoop; index++) {
        addrOffset = index * HCCL_SDMA_MAX_COUNT_4GB;
        countSplit = (index == spiltLoop) ? (countSize - index * HCCL_SDMA_MAX_COUNT_4GB) : (HCCL_SDMA_MAX_COUNT_4GB);
        void *srcSplit = static_cast<void *>(static_cast<char *>(const_cast<void *>(src.ptr())) + addrOffset);
        void *dstSplit = static_cast<void *>(static_cast<char *>(dst.ptr()) + addrOffset);

        CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
        AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr;
        dfxInfo->opRingBufferIdx = opRingBufferIdx_;
        dfxInfo->remoteRank = remoteUserRank;
        dfxInfo->notifyId = INVALID_VALUE_RANKID;
        addOneMemcpySqe_(streamInfo.actualStreamId, taskId, srcSplit, countSplit , ACL_FLOAT, rtReduceOp,
            dstSplit, 0, aicpuInfo_.ssid, aicpuInfo_.devId, aicpuInfo_.overflowAddr, linkType, sqeBuffer, sqeTypeAddr);

        PLF_CONFIG_INFO(PLF_TASK,
            "%s para: linkType[%u] srcSplit[%p] dstSplit[%p] countSplit[%llu] taskId[%u] streamId[%u] remoteRank[%u]",
            __func__, linkType, srcSplit, dstSplit, countSplit , taskId, streamInfo.actualStreamId, remoteUserRank);
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::LaunchTask(Stream &stream, bool isBlockLaunch)
{
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();
    HcclSqeContext *sqeContext = stream.GetSqeContextPtr();
    CHK_PTR_NULL(sqeContext);
    SqeRingBuffer *sqeContextBuffer = &(sqeContext->buffer);
    CHK_PTR_NULL(sqeContextBuffer);
    const auto cnt = sqeContextBuffer->sqeCnt;
    if (cnt == 0) {
        CHK_PRT_CONT(isBlockLaunch,
            HCCL_DEBUG("no sqe, streamId:%d, sqId:%u", streamInfo.actualStreamId, streamInfo.sqId));
        return HCCL_SUCCESS;
    } else if (cnt > streamInfo.sqDepth) {
        HCCL_ERROR("LaunchTask fail, cnt:%u should be less than sqDepth:%u", cnt, streamInfo.sqDepth);
        return HCCL_E_PTR;
    }

    auto &head = sqeContextBuffer->sqHead;
    auto &tail = sqeContextBuffer->sqTail;
    u32 newTail = (tail + cnt) % streamInfo.sqDepth;
    // 仅在阻塞下发场景打印，避免非阻塞场景调用时刷屏
    CHK_PRT_CONT(isBlockLaunch,
        HCCL_INFO("Before send sqid:%d cnt:%u head:%u curtail:%u newTail:%u",
        streamInfo.sqId, cnt, head, tail, newTail));

    u64 startUsec = GetCurAicpuTimestamp();
    u64 lastUsec = startUsec;
    while (((tail < head ? streamInfo.sqDepth : 0U) + tail - head + cnt >= streamInfo.sqDepth) && (tail != head)) { // 判断剩余sqe空间是否足够下发
        // 需要放在while循环进来后第一个执行
        CHK_RET(QuerySqStatusByType(aicpuInfo_.devId, streamInfo.sqId, DRV_SQCQ_PROP_SQ_HEAD, head));

        // 非阻塞下发场景，rtsq队列空间不足时直接返回
        if (isBlockLaunch == false) {
            return HCCL_SUCCESS;
        }

        // 当前流无法下发，把其他流都launch一遍，避免等待的其他流没有launch
        for (auto it = streamMap_.begin(); it != streamMap_.end(); ++it) {
            if (it->first != streamInfo.actualStreamId) {
                CHK_RET(LaunchTask(it->second, false));
            }
        }

        u64 curUsec = GetCurAicpuTimestamp();
        if (dfxTimeOutConfig_.sqFullWaitTimeOut != 0 && 
            (curUsec - startUsec > NANOSECOND_TO_SECOND * dfxTimeOutConfig_.sqFullWaitTimeOut)) {
            HCCL_ERROR("Rtsq full, timeout %lus. curhead:%u, sqId:%d", dfxTimeOutConfig_.sqFullWaitTimeOut, head,
                streamInfo.sqId);
            return HCCL_E_AGAIN;
        }

        // 等待下发阶段，每隔30s打印一次状态
        if (curUsec - lastUsec > NANOSECOND_TO_SECOND * dfx::kPrintSqInterval) {
            lastUsec = curUsec;
            HCCL_RUN_INFO("[LaunchTask][WaitLaunchWhileLoop]Current state. sqid:%d, head:%u, tail:%u, cnt:%u",
                streamInfo.sqId, head, tail, cnt);
        }

        // 下发过程中出现cqe异常
        if (checkOpExecStatusCallback_ != nullptr) {
            HcclResult opExecStatus = checkOpExecStatusCallback_();
            CHK_PRT_RET(opExecStatus != HCCL_SUCCESS,
                HCCL_ERROR("hccl aicpu stop launch for task exception or stop command, ret:%d", opExecStatus),
                opExecStatus);
        }
    }

    uint32_t left = streamInfo.sqDepth - tail;                     // sqeAddr 剩余空间
    const auto tailSqeIdx = sqeContextBuffer->tailSqeIdx;
    HCCL_INFO("cpy sqe, left:%u, tailSqeId:%u, cnt:%u, streamId:%u", left, tailSqeIdx, cnt, stream.id());
    if (cnt <= left) { // 剩余buffer放得下新增sqe
        CHK_SAFETY_FUNC_RET(memcpy_s(
            reinterpret_cast<uint8_t *>(streamInfo.sqBaseAddr) + tail * HCCL_SQE_SIZE,
            left * HCCL_SQE_SIZE,
            sqeContextBuffer->localBuff + (tailSqeIdx - cnt) * HCCL_SQE_SIZE,
            cnt * HCCL_SQE_SIZE));

        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsMirrorBuffer + tail * HCCL_SQE_SIZE,
            left * HCCL_SQE_SIZE,
            sqeContextBuffer->localBuff + (tailSqeIdx - cnt) * HCCL_SQE_SIZE,
            cnt * HCCL_SQE_SIZE));

        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsqSqeType + tail, left,
            sqeContextBuffer->sqeType + (tailSqeIdx - cnt), cnt));
        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsDfxInfo + tail, left * sizeof(AicpuDfxInfo),
            sqeContextBuffer->dfxInfo + (tailSqeIdx - cnt), cnt * sizeof(AicpuDfxInfo)));
    } else {
        CHK_SAFETY_FUNC_RET(memcpy_s(reinterpret_cast<uint8_t *>(streamInfo.sqBaseAddr) + tail * HCCL_SQE_SIZE,
            left * HCCL_SQE_SIZE,
            sqeContextBuffer->localBuff + (tailSqeIdx - cnt) * HCCL_SQE_SIZE,
            left * HCCL_SQE_SIZE));

        CHK_SAFETY_FUNC_RET(memcpy_s(reinterpret_cast<uint8_t *>(streamInfo.sqBaseAddr),
            streamInfo.sqDepth * HCCL_SQE_SIZE,
            sqeContextBuffer->localBuff + (tailSqeIdx - cnt + left) * HCCL_SQE_SIZE,
            (cnt - left) * HCCL_SQE_SIZE));

        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsMirrorBuffer + tail * HCCL_SQE_SIZE,
            left * HCCL_SQE_SIZE,
            sqeContextBuffer->localBuff + (tailSqeIdx - cnt) * HCCL_SQE_SIZE,
            left * HCCL_SQE_SIZE));

        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsMirrorBuffer,
            streamInfo.sqDepth * HCCL_SQE_SIZE,
            sqeContextBuffer->localBuff + (tailSqeIdx - cnt + left) * HCCL_SQE_SIZE,
            (cnt - left) * HCCL_SQE_SIZE));

        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsqSqeType + tail,
            left, sqeContextBuffer->sqeType + (tailSqeIdx - cnt), left));
        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsqSqeType + 0, streamInfo.sqDepth,
            sqeContextBuffer->sqeType + (tailSqeIdx - cnt + left), (cnt - left)));
        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsDfxInfo + tail,
            left * sizeof(AicpuDfxInfo), sqeContextBuffer->dfxInfo + (tailSqeIdx - cnt), left * sizeof(AicpuDfxInfo)));
        CHK_SAFETY_FUNC_RET(memcpy_s(sqeContextBuffer->rtsDfxInfo + 0, streamInfo.sqDepth * sizeof(AicpuDfxInfo),
            sqeContextBuffer->dfxInfo + (tailSqeIdx - cnt + left), (cnt - left) * sizeof(AicpuDfxInfo)));
    }
    CHK_RET(ConfigSqStatusByType(aicpuInfo_.devId, streamInfo.sqId, DRV_SQCQ_PROP_SQ_TAIL, newTail));
    tail = newTail;
    PLF_CONFIG_INFO(PLF_TASK,
        "%s success, sqid:%d, sqe_num:%u, curHead:%u, curtail:%u", __func__, streamInfo.sqId, cnt, head, tail);
    sqeContextBuffer->sqeCnt = 0;
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::LaunchTasksEx(hccl::Stream &stream, std::vector<Stream> &subStreams)
{
    /* 两阶段模式，主流待正式执行时再下 */
    /* 一阶段第一次，可以先下主流 */
    HcclResult ret = LaunchTask(stream, true);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[DispatcherAiCpu][LaunchTasksEx] "\
                   "launch task failed, sqid:%u, ret:%u", stream.sqId(), ret);
        return ret;
    }

    for (u32 index = 0; index < subStreams.size(); index++) {
        ret = LaunchTask(subStreams[index], true);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[DispatcherAiCpu][LaunchTasksEx] "\
                       "launch task failed, sqid:%u, ret:%u", subStreams[index].sqId(), ret);
            return ret;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::LaunchAllTasks()
{
    for (auto it = streamMap_.begin(); it != streamMap_.end(); ++it) {
        HcclResult ret = LaunchTask(it->second, true);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("DispatcherAiCpu][LaunchAllTasks] "\
                "launch task failed, sqid:%u, ret:%u", it->second.sqId(), ret);
            return ret;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::ReduceAsync(const void *src, void *dst, u64 dataCount, const HcclDataType datatype,
    HcclReduceOp redOp, Stream &stream, HcclReduceType reduceType)
{
    return (reduceType == HcclReduceType::HCCL_INLINE_REDUCE) ?
        InlineReduceAsync(src, dataCount, datatype, redOp, stream, dst) :
        TbeReduceAsync(src, dst, dataCount, datatype, redOp, stream, dst);
}

HcclResult DispatcherAiCpu::InlineReduceAsync(const void *src, u64 dataCount, const HcclDataType datatype,
    HcclReduceOp redOp, hccl::Stream &stream, void *dst, u32 remoteUserRank, hccl::LinkType inLinkType)
{
    // 参数有效性检查
    CHK_PTR_NULL(stream.ptr());
    if (dataCount == 0) {
        HCCL_INFO("%s src memory size is 0, not need inline reduce.", __func__);
        return HCCL_SUCCESS;
    }
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();

    aclDataType runtimeDataType = DT_MAP_TABLE[datatype];
    aclrtReduceKind rtReduceOp = RK_MAP_TABLE[redOp];

    // 将数据按4GB切分循环处理
    uint64_t spiltLoop = 0;
    uint64_t addr_offset = 0;
    uint64_t countSplit = 0;
    uint64_t countSize = dataCount * SIZE_TABLE[datatype];
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;

    if (countSize > HCCL_SDMA_MAX_COUNT_4GB) {
        spiltLoop = (countSize % HCCL_SDMA_MAX_COUNT_4GB) ? (countSize / HCCL_SDMA_MAX_COUNT_4GB) :
                                                            ((countSize / HCCL_SDMA_MAX_COUNT_4GB) - 1);
        HCCL_INFO("%s InlineReduceAsync SDMA task countSize is bigger than 4GB"
            " and do segmentation splitloop:%llu", __func__, spiltLoop);
    }
    uint8_t linkType = static_cast<uint8_t>(inLinkType);
    for (uint64_t index = 0; index <= spiltLoop; index++) {
        addr_offset = index * HCCL_SDMA_MAX_COUNT_4GB;
        countSplit = (index == spiltLoop) ? (countSize - index * HCCL_SDMA_MAX_COUNT_4GB) : (HCCL_SDMA_MAX_COUNT_4GB);
        void *srcSplit = static_cast<void *>(static_cast<char *>(const_cast<void *>(src)) + addr_offset);
        void *dstSplit = static_cast<void *>(static_cast<char *>(dst) + addr_offset);

        CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
        AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr;
        dfxInfo->opRingBufferIdx = opRingBufferIdx_;
        dfxInfo->remoteRank = remoteUserRank;
        dfxInfo->notifyId = INVALID_VALUE_RANKID;
        addOneMemcpySqe_(streamInfo.actualStreamId, taskId, srcSplit, countSplit, runtimeDataType, rtReduceOp, dstSplit, 0,
            aicpuInfo_.ssid, aicpuInfo_.devId, aicpuInfo_.overflowAddr, linkType, sqeBuffer, sqeTypeAddr);

        PLF_CONFIG_INFO(PLF_TASK,
            "%s para: linkType[%u] srcSplit[%p] dstSplit[%p] countSplit[%llu] taskId[%u] streamId[%u] remoteRank[%u] "\
            "rtDatatType[%d] rtReduceOp[%d]", __func__, linkType, srcSplit, dstSplit, countSplit, taskId,
            streamInfo.actualStreamId, remoteUserRank, runtimeDataType, rtReduceOp);
    }

    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::TbeReduceAsync(const void *src1, const void *src2, u64 count, const HcclDataType datatype,
    HcclReduceOp redOp, Stream &stream, const void *dst)
{
    HCCL_ERROR("[DispatcherAiCpu][TbeReduceAsync] aicpu do not support the tbe reduce");
    return HCCL_E_NOT_SUPPORT;
}

HcclResult DispatcherAiCpu::RdmaSend(u32 dbindex, u64 dbinfo, hccl::Stream &stream, RdmaTaskInfo &taskInfo)
{
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();

    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId = 0U;

    CHK_RET(GetStreamSqeBufferAddr(stream, sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr;
    dfxInfo->opRingBufferIdx = opRingBufferIdx_;
    dfxInfo->remoteRank = taskInfo.remoteRank;
    dfxInfo->notifyId = INVALID_UINT; // 多个wr只敲一次doorbell的情况下，一般只会有一个notify

    uint32_t wrLen = 0; // 统计wr的总数据量
    for (const WrInformation& wr : taskInfo.wrInfos) {
        wrLen += wr.wrData.memList.len;
        dfxInfo->notifyId = (wr.notifyId != INVALID_UINT) ? wr.notifyId : dfxInfo->notifyId;
    }

    u64 dbAddr = CalcDbAddr(dbindex);
    addOneRdmaDbSendSqe_(streamInfo.actualStreamId, taskId, dbinfo, dbAddr, wrLen,
        static_cast<uint8_t>(taskInfo.rdmaType), sqeBuffer, sqeTypeAddr);

    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: streamId[%u] taskId[%u] remoteRank[%u] RdmaType[%d] wrLen[%u] notifyId[%u]",
        __func__, streamInfo.actualStreamId, taskId, taskInfo.remoteRank, taskInfo.rdmaType, wrLen, dfxInfo->notifyId);

    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::RdmaRecord(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream,
    RdmaType rdmaType, u32 userRank, u64 offset, u32 notifyId)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::GetStreamSqeBufferAddr(hccl::Stream &stream, uint8_t *&sqeBufferAddr, uint8_t *&sqeTypeAddr,
    uint8_t *&sqeDfxInfoAddr, uint16_t &taskId)
{
    SaveStreamInfo(stream);
    HcclSqeContext* sqeContext = stream.GetSqeContextPtr();
    CHK_PTR_NULL(sqeContext);
    if (UNLIKELY(sqeContext->buffer.sqeCnt >= HCCL_PER_LAUNCH_SQE_CNT)) {
        HCCL_INFO("GetStreamSqeBufferAddr tailSqeIdx[%u], try to launchTask", sqeContext->buffer.tailSqeIdx);
        CHK_RET(LaunchTask(stream, true));
    }
    if (UNLIKELY(sqeContext->buffer.tailSqeIdx >= HCCL_SQE_MAX_CNT)) {
        CHK_RET(LaunchTask(stream, true));

        if (callback_ != nullptr) {
            hccl::AiCPUStreamTasks para(stream.id(), reinterpret_cast<void*>(sqeContext));
            hccl::TaskPara taskPara(TaskType::TASK_BATCH_REPORT, para);
            callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
        }
    }
    SqeRingBuffer *sqeContextBuffer = &(sqeContext->buffer);
    uint16_t flipNum = sqeContextBuffer->filpNum;
    uint16_t nextTaskId = sqeContextBuffer->tailSqeTaskId;
    // nextTaskId=0的时候下发PlaceHolder
    if (UNLIKELY(nextTaskId == 0  && flipNum != 0)) {
        CHK_RET(AddFlipTask(stream));
    }
    if (UNLIKELY(sqeContext->buffer.tailSqeIdx >= HCCL_SQE_MAX_CNT)) {
        CHK_RET(LaunchTask(stream, true));

        if (callback_ != nullptr) {
            hccl::AiCPUStreamTasks para(stream.id(), reinterpret_cast<void*>(sqeContext));
            hccl::TaskPara taskPara(TaskType::TASK_BATCH_REPORT, para);
            callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
        }
    }
    CHK_RET(stream.GetNextSqeBufferAddr(sqeBufferAddr, sqeTypeAddr, sqeDfxInfoAddr, taskId));
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::AddFlipTask(Stream &stream)
{
    HcclSqeContext *sqeContext = stream.GetSqeContextPtr();
    CHK_PTR_NULL(sqeContext);
    SqeRingBuffer *sqeContextBuffer = &(sqeContext->buffer);
    CHK_PTR_NULL(sqeContextBuffer);
    uint16_t flipNum = sqeContextBuffer->filpNum;
    uint16_t taskId = sqeContextBuffer->tailSqeTaskId;

    if (callback_ != nullptr) {
        hccl::FlipTaskPara para(stream.id(), taskId, flipNum);
        hccl::TaskPara taskPara(TaskType::TASK_FLIP, para);
        callback_(callBackUserPtr_, (void *)&taskPara, sizeof(struct TaskPara));
    }

    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();
 
    uint8_t *sqeBufferAddr = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    CHK_RET(stream.GetNextSqeBufferAddr(sqeBufferAddr, sqeTypeAddr, sqeDfxInfoAddr, taskId));
 
    AicpuDfxInfo * const dfxInfo = (AicpuDfxInfo * const)sqeDfxInfoAddr;
    dfxInfo->opRingBufferIdx = opRingBufferIdx_;
    dfxInfo->remoteRank = INVALID_VALUE_RANKID;
    dfxInfo->notifyId = INVALID_VALUE_RANKID;
    addOneFlipPlaceHolderSqe_(streamInfo.actualStreamId, flipNum, taskId, sqeBufferAddr, sqeTypeAddr);
 
    PLF_CONFIG_INFO(PLF_TASK,
        "%s para: taskId[%u] streamId[%u] flipNum[%u]", __func__, taskId, streamInfo.actualStreamId, flipNum);
    return HCCL_SUCCESS;
}

HcclResult DispatcherAiCpu::AddRetryPreamble(Stream &stream)
{
    return AddFlipTask(stream);
}

void DispatcherAiCpu::SaveStreamInfo(hccl::Stream &stream)
{
    const HcclComStreamInfo &streamInfo = stream.GetHcclStreamInfo();
    if (streamMap_.find(streamInfo.actualStreamId) == streamMap_.end()) {
        streamMap_.insert({streamInfo.actualStreamId, stream});
        HCCL_INFO("[DispatcherAiCpu][SaveStreamInfo] stream id[%d]", streamInfo.actualStreamId);
    }
    return;
}

HcclResult DispatcherAiCpu::StreamSync(Stream &stream)
{
    uint32_t head = 0;
    uint32_t tail = 0;
    const HcclComStreamInfo *streamInfo;
    u64 startUsec = GetCurAicpuTimestamp();
    u64 lastUsec = startUsec;
    CHK_RET(stream.GetStreamInfo(streamInfo));

    CHK_RET(QuerySqStatusByType(aicpuInfo_.devId, streamInfo->sqId, DRV_SQCQ_PROP_SQ_TAIL, tail));
    HCCL_INFO("StreamSync aicpu stream sqid[%d] tail[%u]", streamInfo->sqId, tail);
    do {
        CHK_RET(QuerySqStatusByType(aicpuInfo_.devId, streamInfo->sqId, DRV_SQCQ_PROP_SQ_HEAD, head));
        u64 curUsec = GetCurAicpuTimestamp();
        if (curUsec - startUsec > NANOSECOND_TO_SECOND * dfxTimeOutConfig_.sqeTimeOutTimeOut) {
            HCCL_ERROR("stream sync timeout %lus. curhead:%u, curtall:%u, sqId:%d",
                dfxTimeOutConfig_.sqeTimeOutTimeOut, head, tail, streamInfo->sqId);
            return HCCL_E_TIMEOUT;
        }

        // 等待下发阶段，每隔30s打印一次状态
        if (curUsec - lastUsec > NANOSECOND_TO_SECOND * dfx::kPrintSqInterval) {
            lastUsec = curUsec;
            HCCL_RUN_INFO("[StreamSync]Current state. sqid:%d, head:%u, tail:%u",
                streamInfo->sqId, head, tail);
        }
    } while (head != tail);

    return HCCL_SUCCESS;
}

u64 DispatcherAiCpu::CalcDbAddr(u32 dbindex)
{
    u64 dbAddr = 0;
    if (aicpuInfo_.devType == DevType::DEV_TYPE_910_93) {
        // 910_93 HCCS_SW 组网
        constexpr u64 roceBaseAddr = 0x202000000000ULL;
        constexpr u64 roceVfDbCfg0Reg = 0x230ULL;
        constexpr u64 chipAddrOffset = 0x20000000000ULL;
        constexpr u64 dieAddrOffset = 0x10000000000ULL;
        constexpr u32 dbDieIdMask = 0x00ff0000;
        constexpr u32 dbDieIdShift = 16; // 16 is dbDieIdShift
        dbAddr = roceBaseAddr + roceVfDbCfg0Reg + chipAddrOffset * aicpuInfo_.chipId +
            dieAddrOffset * ((dbindex & dbDieIdMask) >> dbDieIdShift);
    } else {
        constexpr u64 roceBaseAddr = 0x2000000000ULL;
        constexpr u64 roceVfDbCfg0Reg = 0x230ULL;
        constexpr u64 chipAddrOffset = 0x80000000000ULL;
        constexpr u64 dieAddrOffset = 0x10000000000ULL;
        constexpr u32 dbDieIdMask = 0x00ff0000;
        constexpr u32 dbDieIdShift = 16; // 16 is dbDieIdShift
        dbAddr = roceBaseAddr + roceVfDbCfg0Reg + chipAddrOffset * aicpuInfo_.chipId +
            dieAddrOffset * ((dbindex & dbDieIdMask) >> dbDieIdShift);
    }

    HCCL_DEBUG("%s dbindex:%u, devType:%u, chipId:%lld, dbAddr:%llu",
        __func__, dbindex, aicpuInfo_.devType, aicpuInfo_.chipId, dbAddr);
    return dbAddr;
}

void DispatcherAiCpu::InitTimeOutConfig()
{
    dfxTimeOutConfig_.useCredit = false;
    dfxTimeOutConfig_.sqeTimeOutTimeOut = GetMaxNotifyWaitTime();
    dfxTimeOutConfig_.sqeCreditTimeOut = RT_STARS_NEVER_TIMEOUT_KERNEL_CREDIT;
    dfxTimeOutConfig_.sqeWaitTimeOut = dfx::kKfcTimeOut;
    dfxTimeOutConfig_.sqFullWaitTimeOut = dfx::kSqFullWaitTimeOut;
    HCCL_INFO("[DispatcherAiCpu][InitTimeOutConfig]DFX timeout config init successfully with details: [%s]",
        dfxTimeOutConfig_.ToString().c_str());
}
} // namespace hccl