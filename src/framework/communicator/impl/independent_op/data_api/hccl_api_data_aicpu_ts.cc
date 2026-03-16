/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_data.h"
#include "dispatcher.h"
#include "new/hccl_primitive_local.h"
#include "new/hccl_primitive_remote.h"
#include "thread.h"
#include "launch_context.h"

#include "ub_transport_lite_impl.h"
#include "device/framework/aicpu_hccl_process.h"
#include "coll_comm_aicpu_mgr.h"
#include "aicpu_indop_process.h"
#include "hcclCommDfxLite.h"
#include "hcclCommProfilingLite.h"
#include "profiling_handler_lite.h"
#include "hcclCommOp.h"
#include "hcomm_diag.h"
#include "hccl_api_data_aicpu_ts.h"

using namespace hccl;
thread_local LaunchContext g_threadLaunchCtx;

bool IsBatchLaunchMode() {
    return g_threadLaunchCtx.IsBatchLaunchMode();
}

void AddThread(ThreadHandle thread) {
    g_threadLaunchCtx.AddThread(thread);
}

bool IsSupportReduce(HcommDataType dataType, HcommReduceOp op)
{
    bool checkDataType =
        (dataType == HCOMM_DATA_TYPE_FP32 || dataType == HCOMM_DATA_TYPE_FP16 || dataType == HCOMM_DATA_TYPE_INT8 ||
        dataType == HCOMM_DATA_TYPE_INT16 || dataType == HCOMM_DATA_TYPE_INT32 || dataType == HCOMM_DATA_TYPE_BFP16);
    bool checkReduceType = (op == HCOMM_REDUCE_SUM || op == HCOMM_REDUCE_MAX || op == HCOMM_REDUCE_MIN);
    return checkDataType && checkReduceType;
}
 

int32_t HcommLocalCopyOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, thread, dst, src, len);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalCopy(dst, src, len), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf dstBuf{dst, len, nullptr};
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        ret = HcclLocalCopy(stream, &dstBuf, &srcBuf);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
            __func__, thread, dst, src, len), ret);
    return HCCL_SUCCESS;
}

int32_t HcommLocalReduceOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t count,
    HcommDataType dataType, HcommReduceOp reduceOp)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].", __func__, thread, dst, src, count, dataType, reduceOp);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalReduce(dst, src, len, dataType, reduceOp), ret = HCCL_E_INTERNAL);
    } else {
        CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
            "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
        HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf dstBuf{dst, len, nullptr};
        HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        ret = HcclLocalCopyReduce(stream, &dstBuf, &srcBuf, reduceInfo);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].", __func__, thread, dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyRecordOnThread(ThreadHandle thread, ThreadHandle dstThread, uint32_t dstNotifyIdx)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dstThread[0x%llx], dstNotifyIdx[%u].", __func__, thread, dstThread, dstNotifyIdx);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    Thread *const dstThreadPtr = reinterpret_cast<Thread *>(dstThread);
    CHK_PTR_NULL(dstThreadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        LocalNotify *const notifyPtr = dstThreadPtr->GetNotify(dstNotifyIdx);
        CHK_PTR_NULL(notifyPtr);
        const uint32_t notifyId = notifyPtr->notifyId_;
        EXECEPTION_CATCH(ret = threadPtr->LocalNotifyRecord(notifyId), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        LocalNotify *notify = GetNotify(dstThread, dstNotifyIdx);
        CHK_PTR_NULL(notify);
        ret = HcclLocalNotifyRecord(stream, notify);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstThread[0x%llx], dstNotifyIdx[%u].", __func__, thread, dstThread, dstNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyWaitOnThread(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeOut)
{
    HCCL_INFO("[%s] START. thread[0x%llx], notifyIdx[%u], timeOut[%u].", __func__, thread, notifyIdx, timeOut);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        LocalNotify *const notifyPtr = threadPtr->GetNotify(notifyIdx);
        CHK_PTR_NULL(notifyPtr);
        const uint32_t notifyId = notifyPtr->notifyId_;
        EXECEPTION_CATCH(ret = threadPtr->LocalNotifyWait(notifyId), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        LocalNotify *notify = GetNotify(thread, notifyIdx);
        CHK_PTR_NULL(notify);
        ret = HcclLocalNotifyWait(stream, notify, timeOut);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], notifyIdx[%u], timeOut[%u].", __func__, thread, notifyIdx, timeOut), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommAclrtNotifyRecordOnThread(ThreadHandle thread, uint64_t dstNotifyId)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dstNotifyId[%llu].", __func__, thread, dstNotifyId);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalNotifyRecord(dstNotifyId), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        ret = HcclLocalBareNotifyRecord(stream, dstNotifyId);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstNotifyId[%llu].", __func__, thread, dstNotifyId), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommAclrtNotifyWaitOnThread(ThreadHandle thread, uint64_t notifyId, uint32_t timeOut)
{
    HCCL_INFO("[%s] START. thread[0x%llx], notifyId[%llu], timeOut[%u].", __func__, thread, notifyId, timeOut);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalNotifyWait(notifyId), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        ret = HcclLocalBareNotifyWait(stream, notifyId, timeOut);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], notifyId[%llu], timeOut[%u].", __func__, thread, notifyId, timeOut), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

HcclResult CommTaskPrepare(char *key, uint32_t keyLen) // host ffts+使用
{
    std::string keyStr = "temp_key";
    if (key != nullptr && keyLen != 0) {
        keyStr = std::string(key, keyLen);
        HCCL_DEBUG("[CommTaskPrepare]key[%s], keyLen[%u]", key, keyLen);
    } else {
        HCCL_DEBUG("[CommTaskPrepare]disable cache, key[0x%llx], keyLen[%u]", key, keyLen);
    }

    return HcclTaskPrepare(const_cast<char_t*>(keyStr.c_str()), keyStr.length());
}

HcclResult CommTaskLaunch(ThreadHandle *threads, uint32_t threadNum) // host ffts+或aicpu stars使用"
{
    CHK_PTR_NULL(threads);
    CHK_PRT_RET(threadNum < 1, HCCL_ERROR("[CommTaskLaunch]threadNum is less than 1"), HCCL_E_PARA);

    Thread *threadPtr = reinterpret_cast<Thread *>(threads[0]);
    CHK_PTR_NULL(threadPtr);

    if (threadPtr->IsDeviceA5()) {
        HCCL_INFO("[%s] Running on A5.", __func__);
        for (uint32_t i = 0; i < threadNum; i++) {
            Thread *threadPtrLoop = reinterpret_cast<Thread *>(threads[i]);
            CHK_PTR_NULL(threadPtrLoop);
            HCCL_INFO("[%s] Launching task in thread[0x%llx].", __func__, threads[i]);
            EXECEPTION_CATCH(threadPtrLoop->LaunchTask(), return HCCL_E_INTERNAL);
        }
        return HCCL_SUCCESS;
    }

    std::vector<hccl::Stream> streams;
    for (uint32_t i = 0; i < threadNum; i++) {
        hccl::Stream *stream = GetStream(threads[i]);
        CHK_PTR_NULL(stream);
        streams.push_back(*stream);
    }

    return HcclTaskLaunch(streams.data(), threadNum);
}

namespace {
// Convert hccl::HcommDataType => Hccl::DataType, hccl::HcommReduceOp => Hccl::ReduceOp

std::unordered_map<HcommDataType, Hccl::DataType> mapHcommDataTypeToA5 = {
    {HcommDataType::HCOMM_DATA_TYPE_INT8,    Hccl::DataType::INT8},
    {HcommDataType::HCOMM_DATA_TYPE_INT16,   Hccl::DataType::INT16},
    {HcommDataType::HCOMM_DATA_TYPE_INT32,   Hccl::DataType::INT32},
    {HcommDataType::HCOMM_DATA_TYPE_FP16,    Hccl::DataType::FP16},
    {HcommDataType::HCOMM_DATA_TYPE_FP32,    Hccl::DataType::FP32},
    {HcommDataType::HCOMM_DATA_TYPE_INT64,   Hccl::DataType::INT64},
    {HcommDataType::HCOMM_DATA_TYPE_UINT64,  Hccl::DataType::UINT64},
    {HcommDataType::HCOMM_DATA_TYPE_UINT8,   Hccl::DataType::UINT8},
    {HcommDataType::HCOMM_DATA_TYPE_UINT16,  Hccl::DataType::UINT16},
    {HcommDataType::HCOMM_DATA_TYPE_UINT32,  Hccl::DataType::UINT32},
    {HcommDataType::HCOMM_DATA_TYPE_FP64,    Hccl::DataType::FP64},
    {HcommDataType::HCOMM_DATA_TYPE_BFP16,   Hccl::DataType::BFP16},
    {HcommDataType::HCOMM_DATA_TYPE_INT128,  Hccl::DataType::INT128},
#ifndef OPEN_BUILD_PROJECT
    {HcommDataType::HCOMM_DATA_TYPE_HIF8,    Hccl::DataType::HIF8},
    {HcommDataType::HCOMM_DATA_TYPE_FP8E4M3, Hccl::DataType::FP8E4M3},
    {HcommDataType::HCOMM_DATA_TYPE_FP8E5M2, Hccl::DataType::FP8E5M2},
    {HcommDataType::HCOMM_DATA_TYPE_FP8E8M0, Hccl::DataType::FP8E8M0}
#endif
};

std::unordered_map<HcommReduceOp, Hccl::ReduceOp> mapHcommReduceOpToA5 = {
    {HcommReduceOp::HCOMM_REDUCE_SUM,  Hccl::ReduceOp::SUM},
    {HcommReduceOp::HCOMM_REDUCE_PROD, Hccl::ReduceOp::PROD},
    {HcommReduceOp::HCOMM_REDUCE_MAX,  Hccl::ReduceOp::MAX},
    {HcommReduceOp::HCOMM_REDUCE_MIN,  Hccl::ReduceOp::MIN}};

inline HcclResult CheckDataTypeAndReduceOp(HcommDataType dataType, HcommReduceOp reduceOp)
{
    if (mapHcommDataTypeToA5.find(dataType) == mapHcommDataTypeToA5.end()) {
        HCCL_ERROR("[%s] type[%u] is not supported.", __func__, dataType);
        return HCCL_E_PARA;
    }

    if (mapHcommReduceOpToA5.find(reduceOp) == mapHcommReduceOpToA5.end()) {
        HCCL_ERROR("[%s] op[%u] is not supported.", __func__, reduceOp);
        return HCCL_E_PARA;
    }

    return HCCL_SUCCESS;
}

} // namespace

int32_t HcommWriteOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, channel, dst, src, len);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(src), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
            __func__, thread, channel, dst, src, len), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};

        EXECEPTION_CATCH(ubTransportLitePtr->Write(locRmaBuf, rmtBuf, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf rmtBuf{dst, len, nullptr};

        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteWrite(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, channel, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, channel, dst, src, count, dataType, reduceOp);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(src), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};

        ret = CheckDataTypeAndReduceOp(dataType, reduceOp);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at CheckDataTypeAndReduceOp. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
        Hccl::ReduceIn reduceIn{mapHcommDataTypeToA5.at(dataType), mapHcommReduceOpToA5.at(reduceOp)};

        EXECEPTION_CATCH(ubTransportLitePtr->WriteReduce(locRmaBuf, rmtBuf, reduceIn, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
            "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
        HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf rmtBuf{dst, len, nullptr};
        HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteWriteReduce(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf, reduceInfo);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

HcclResult CommWriteReduceWithNotify(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp, uint32_t remoteNotifyIdx)
{
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    AddThread(thread);
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
        "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    HcclBuf locBuf{const_cast<void*>(src), count * SIZE_TABLE[dataType], nullptr};
    HcclBuf rmtBuf{dst, count * SIZE_TABLE[dataType], nullptr};
    HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    return HcclRemoteWriteReduceWithNotify(stream, reinterpret_cast<void*>(channel), &rmtBuf, &locBuf, reduceInfo,
        remoteNotifyIdx);
}

int32_t HcommWriteWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, thread, channel, dst, src, len, remoteNotifyIdx);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        HCCL_DEBUG("[%s] Running on A5.", __func__);
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(src), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
            __func__, thread, channel, dst, src, len, remoteNotifyIdx), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};

        Hccl::WithNotifyIn withNotify{Hccl::TransportNotifyType::NORMAL, remoteNotifyIdx};

        EXECEPTION_CATCH(ubTransportLitePtr->WriteWithNotify(locRmaBuf, rmtBuf, withNotify, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf rmtBuf{dst, len, nullptr};

        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteWriteWithNotify(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf, remoteNotifyIdx);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, thread, channel, dst, src, len, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteReduceWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst,
    const void *src, uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].", 
        __func__, thread, channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        HCCL_DEBUG("[%s] Running on A5.", __func__);
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(src), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
            __func__, thread, channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};
        
        ret = CheckDataTypeAndReduceOp(dataType, reduceOp);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at CheckDataTypeAndReduceOp. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
            __func__, thread, channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx), ret);
        Hccl::ReduceIn reduceIn{mapHcommDataTypeToA5.at(dataType), mapHcommReduceOpToA5.at(reduceOp)};

        Hccl::WithNotifyIn withNotify{Hccl::TransportNotifyType::NORMAL, remoteNotifyIdx};

        EXECEPTION_CATCH(ubTransportLitePtr->WriteReduceWithNotify(locRmaBuf, rmtBuf, reduceIn, withNotify, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        ret = HCCL_E_NOT_SUPPORT;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
        __func__, thread, channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommReadOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, channel, dst, src, len);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(dst), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
            __func__, thread, channel, dst, src, len), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(src), len};

        EXECEPTION_CATCH(ubTransportLitePtr->Read(locRmaBuf, rmtBuf, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf locBuf{dst, len, nullptr};
        HcclBuf rmtBuf{const_cast<void *>(src), len, nullptr};

        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteRead(stream, reinterpret_cast<void *>(channel), &locBuf, &rmtBuf);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, channel, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommReadReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, channel, dst, src, count, dataType, reduceOp);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = ubTransportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(dst), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(src), len};

        ret = CheckDataTypeAndReduceOp(dataType, reduceOp);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at CheckDataTypeAndReduceOp. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
        Hccl::ReduceIn reduceIn{mapHcommDataTypeToA5.at(dataType), mapHcommReduceOpToA5.at(reduceOp)};

        EXECEPTION_CATCH(ubTransportLitePtr->ReadReduce(locRmaBuf, rmtBuf, reduceIn, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
            "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
        HcclBuf locBuf{dst, len, nullptr};
        HcclBuf rmtBuf{const_cast<void *>(src), len, nullptr};
        HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteReadReduce(stream, reinterpret_cast<void *>(channel), &locBuf, &rmtBuf, reduceInfo);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_DEBUG("[%s] channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, channel, dst, src, len);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommWriteWithNotifyNbi(ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx)
{
    HCCL_DEBUG("[%s] channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, channel, dst, src, len, remoteNotifyIdx);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommReadNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_DEBUG("[%s] channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, channel, dst, src, len);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommChannelNotifyRecordOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], remoteNotifyIdx[%u].", __func__, thread, channel, remoteNotifyIdx);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        HCCL_DEBUG("[%s] Running on A5.", __func__);
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);
        HCCL_INFO("channel streamlite ptr %p.", streamLitePtr);

        EXECEPTION_CATCH(ubTransportLitePtr->Post(remoteNotifyIdx, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteNotifyRecord(stream, reinterpret_cast<void *>(channel), remoteNotifyIdx);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], remoteNotifyIdx[%u].", __func__, thread, channel, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx)
{
    HCCL_DEBUG("[%s] channel[0x%llx], remoteNotifyIdx[%u].", __func__, channel, remoteNotifyIdx);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommChannelNotifyWaitOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, thread, channel, localNotifyIdx, timeout);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        HCCL_DEBUG("[%s] Running on A5.", __func__);
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        (void)timeout;
        EXECEPTION_CATCH(ubTransportLitePtr->Wait(localNotifyIdx, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteNotifyWait(stream, reinterpret_cast<void *>(channel), localNotifyIdx, timeout);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, thread, channel, localNotifyIdx, timeout), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout)
{
    HCCL_DEBUG("[%s] channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, channel, localNotifyIdx, timeout);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult CommFence(ThreadHandle thread, ChannelHandle channel) // 控制前后的任务保序
{
    HCCL_DEBUG("[CommFence] thread[0x%llx], channel[0x%llx].", thread, channel);
    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    return HcclRemoteFence(stream, reinterpret_cast<void *>(channel), false);
}

int32_t HcommSetLaunchMode(const char *launchTag, HcommLaunchMode mode)
{
    HCCL_DEBUG("HcommSetLaunchMode launchTag[%s]", launchTag);
    return g_threadLaunchCtx.SetLaunchMode(launchTag, mode);
}

int32_t HcommBatchModeStart(const char *batchTag)
{
    return HcommSetLaunchMode(batchTag, HCOMM_LAUNCH_MODE_BATCH);
}

int32_t HcommBatchModeEnd(const char *batchTag)
{
    return HcommSetLaunchMode(batchTag, HCOMM_LAUNCH_MODE_EAGER);
}

int32_t HcommAcquireComm(const char* commId)
{
    CHK_PTR_NULL(commId);
    HcclCommAicpu *hcclComm = AicpuHcclProcess::AicpuGetCommbyGroup(commId);
    CHK_PRT_RET(!hcclComm, HCCL_ERROR("%s hcclComm is null, commId[%s]", __func__, commId), HCCL_E_PTR);
    DevType devType = hcclComm->GetDevType();
    if (devType != DevType::DEV_TYPE_950){
        CHK_RET(hcclComm->SetDispatcherCtxOnThread());
    }
    return HCCL_SUCCESS;
}

int32_t HcommChannelRegisterDfx(ChannelHandle channel, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback) {
    HCCL_INFO("[HcommChannelRegisterDfx] Init begin");
    auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
    CHK_PTR_NULL(ubTransportLitePtr);
    CHK_RET(ubTransportLitePtr->SetAddTaskInfoCallback(callback));
    HCCL_INFO("[HcommChannelRegisterDfx] Init success");
    return HCCL_SUCCESS;
}

int32_t HcommThreadRegisterDfx(ThreadHandle thread, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback) {
    HCCL_INFO("[HcommThreadRegisterDfx] Init begin");
    Thread *threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    CHK_RET(threadPtr->SetAddTaskInfoCallback(callback));
    HCCL_INFO("[HcommThreadRegisterDfx] Init success");
    return HCCL_SUCCESS;
}

int32_t HcommReleaseComm(const char* commId)
{
    CHK_PTR_NULL(commId);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(commId);
    HCCL_INFO("%s success, commId[%s]", __func__, commId);
    return HCCL_SUCCESS;
}

int32_t HcommFlush()
{
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommChannelFence(ChannelHandle channel)
{
    HCCL_DEBUG("[%s] channel[0x%llx].", __func__, channel);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommThreadJoin(ThreadHandle thread, uint32_t timeout)
{
    hccl::Thread *threadPtr = reinterpret_cast<hccl::Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HCCL_INFO("[%s] START. thread[0x%llx].", __func__, thread);

    if (threadPtr->IsDeviceA5()) {
        HCCL_INFO("[%s] Running on A5.", __func__);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);
        auto *const rtsqPtr = streamLitePtr->GetRtsq();
        CHK_PTR_NULL(rtsqPtr);

        uint32_t head = 0;
        uint32_t tail = 0;
        uint32_t sqId = streamLitePtr->GetSqId();
        EXECEPTION_CATCH(tail = rtsqPtr->QuerySqTail(), return HCCL_E_INTERNAL);
        HCCL_INFO("[%s] aicpu stream sqid[%u] tail[%u]", __func__, sqId, tail);

        u64 startUsec = GetCurAicpuTimestamp();
        u64 lastUsec = startUsec;
        constexpr uint64_t NANOSECOND_TO_SECOND = 1000000000U;
        const uint64_t kPrintSqInterval = 30U;
        do {
            EXECEPTION_CATCH(head = rtsqPtr->QuerySqHead(), return HCCL_E_INTERNAL);
            u64 curUsec = GetCurAicpuTimestamp();
            if (curUsec - startUsec > NANOSECOND_TO_SECOND * timeout) {
                HCCL_ERROR("[%s] timeout %us. curhead:%u, curtail:%u, sqId:%u",
                    __func__, timeout, head, tail, sqId);
                return HCCL_E_TIMEOUT;
            }

            // 等待下发阶段，每隔30s打印一次状态
            if (curUsec - lastUsec > NANOSECOND_TO_SECOND * kPrintSqInterval) {
                lastUsec = curUsec;
                HCCL_RUN_INFO("[%s]Current state. sqid:%d, head:%u, tail:%u",
                    __func__, sqId, head, tail);
            }
        } while (head != tail);
        HCCL_INFO("[%s] SUCCESS. RTSQ's head[%u] == tail[%u].", __func__, head, tail);
        return HCCL_SUCCESS;
    }

    HCCL_ERROR("[%s]Does not support this interface.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult HcommProfilingReportDeviceOp(const char* groupname) {
    HCCL_INFO("[%s] START.", __func__);
    CHK_PTR_NULL(groupname);
    CHK_RET(AicpuIndopProcess::ProfilingReportDeviceOp(groupname));
    return HCCL_SUCCESS;
}

HcclResult HcommProfilingReportKernelStartTask(uint64_t thread, const char* groupname)
{
    HCCL_INFO("[%s] START, thread [%llu], groupname[%s].", __func__, thread, groupname);
    CHK_PTR_NULL(groupname);
    CHK_RET(AicpuIndopProcess::UpdateTask(groupname));
    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
    CHK_PTR_NULL(streamLitePtr);
    Hccl::FlagTaskInfo flagTaskInfo;
    flagTaskInfo.streamId = streamLitePtr->GetId();
    flagTaskInfo.taskId = streamLitePtr->GetRtsq()->GetTaskId();
    flagTaskInfo.type = Hccl::MainStreamTaskType::HEAD;
    Hccl::ProfilingHandlerLite::GetInstance().ReportMainStreamTask(flagTaskInfo);
    HCCL_INFO("[%s] SUCCESS. TaskInfo taskId:[%u] streamId:[%u].", __func__, flagTaskInfo.taskId, flagTaskInfo.streamId);
    return HCCL_SUCCESS;
}

HcclResult HcommProfilingReportKernelEndTask(uint64_t thread, const char* groupname)
{
    HCCL_INFO("[%s] START. thread [%llu], groupname[%s].", __func__, thread, groupname);
    CHK_PTR_NULL(groupname);
    Thread *const threadPtr = reinterpret_cast<Thread*>(thread);
    CHK_PRT_RET(threadPtr == nullptr, HCCL_ERROR("[%s] threadPtr is null", __func__), HCCL_E_PTR);
    auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
    CHK_PRT_RET(streamLitePtr == nullptr, HCCL_ERROR("[%s] streamLitePtr is null", __func__), HCCL_E_PTR);
    //FlagTaskInfo Report
    Hccl::FlagTaskInfo flagTaskInfo;
    flagTaskInfo.streamId = streamLitePtr->GetId();
    flagTaskInfo.taskId = streamLitePtr->GetRtsq()->GetTaskId() - 1;
    flagTaskInfo.type = Hccl::MainStreamTaskType::TAIL;
    
    Hccl::ProfilingHandlerLite::GetInstance().ReportMainStreamTask(flagTaskInfo);
    CHK_RET(AicpuIndopProcess::ReportAllTasks(groupname));
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}
