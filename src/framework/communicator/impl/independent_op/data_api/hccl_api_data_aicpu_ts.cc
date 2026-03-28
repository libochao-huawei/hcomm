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
#include "resource_entities.h"
#include "hcomm_notify_utils.h"
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
#include "hccl_diag.h"

#include <mutex>

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
 
HcclResult HcommThreadGetNotifyId(ThreadHandle thread, uint32_t notifyIdx, uint32_t *notifyId)
{
    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    LocalNotify *const notifyPtr = threadPtr->GetNotify(notifyIdx);
    CHK_PTR_NULL(notifyPtr);
    *notifyId = notifyPtr->notifyId_;

    return HCCL_SUCCESS;
}

HcclResult HcclDfxRegOpInfoByCommId(char* commId, void* hcclDfxOpInfo)
{
    CHK_PTR_NULL(commId);
    CHK_PTR_NULL(hcclDfxOpInfo);
    HcclDfxOpInfo *aicpuDfxInfo = reinterpret_cast<HcclDfxOpInfo *>(hcclDfxOpInfo);
    CHK_RET(HcommThreadGetNotifyId(aicpuDfxInfo->cpuTsThread, aicpuDfxInfo->cpuWaitAicpuNotifyIdx, &aicpuDfxInfo->cpuWaitAicpuNotifyId));
    CHK_RET(AicpuIndopProcess::AicpuDfxOpInfoInit(aicpuDfxInfo, commId));

    return HCCL_SUCCESS;
}

int32_t HcommLocalCopyOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t len)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dst, src, len);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalCopy(dst, src, len), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf dstBuf{dst, len, nullptr};
        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);
        ret = HcclLocalCopy(stream, &dstBuf, &srcBuf);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommLocalReduceOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t count,
    HcommDataType dataType, HcommReduceOp reduceOp)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dst, src, count, dataType, reduceOp);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
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
        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);
        ret = HcclLocalCopyReduce(stream, &dstBuf, &srcBuf, reduceInfo);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyRecordOnThread(ThreadHandle thread, ThreadHandle dstThread, uint32_t dstNotifyIdx)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    auto *const dstThreadEntityPtr = reinterpret_cast<ThreadEntity *>(dstThread);
    CHK_PTR_NULL(dstThreadEntityPtr);

    HCCL_INFO("[%s] START. thread[0x%llx][%s], dstThread[0x%llx][%s], dstNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dstThread, dstThreadEntityPtr->DescribeAttr().c_str(), dstNotifyIdx);

    int32_t ret = HCCL_SUCCESS;
    const NotifyRecordOpType notifyRecordOpType = GetNotifyRecordOpType(*threadEntityPtr, *dstThreadEntityPtr);
    switch (notifyRecordOpType) {
        case NotifyRecordOpType::AicpuTs_to_AicpuTs:
            AddThread(threadEntityPtr->threadObjAddr);
            ret = RecordAicpuTsToAicpuTs(*threadEntityPtr, *dstThreadEntityPtr, dstNotifyIdx);
            break;
        case NotifyRecordOpType::AicpuTs_to_Cpu:
            AddThread(threadEntityPtr->threadObjAddr);
            ret = RecordAicpuTsToCpu(*threadEntityPtr, *dstThreadEntityPtr, dstNotifyIdx);
            break;
        case NotifyRecordOpType::Cpu_to_AicpuTs:
            ret = RecordCpuToAicpuTs(*threadEntityPtr, *dstThreadEntityPtr, dstNotifyIdx);
            break;
        default:
            HCCL_ERROR("[%s] Not supported: thread type[%d], dstThread type[%d].", __func__, threadEntityPtr->type, dstThreadEntityPtr->type);
            ret = HCCL_E_NOT_SUPPORT;
            break;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, 
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], dstThread[0x%llx][%s], dstNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dstThread, dstThreadEntityPtr->DescribeAttr().c_str(), dstNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyWaitOnThread(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeout)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);

    HCCL_INFO("[%s] START. thread[0x%llx][%s], notifyIdx[%u], timeout[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), notifyIdx, timeout);

    if (notifyIdx >= threadEntityPtr->notifyNum) {
        HCCL_ERROR("[%s] notifyIdx[%u] is out of range, notifyNum[%u].", __func__, notifyIdx, threadEntityPtr->notifyNum);
        return HCCL_E_PARA;
    }

    int32_t ret = HCCL_SUCCESS;
    const ThreadType threadType = threadEntityPtr->type;
    switch (threadType) {
        case THREAD_TYPE_TS:
            AddThread(threadEntityPtr->threadObjAddr);
            ret = WaitAicpuTs(*threadEntityPtr, notifyIdx, timeout);
            break;
        case THREAD_TYPE_CPU:
            ret = WaitCpu(*threadEntityPtr, notifyIdx, timeout);
            break;
        default:
            HCCL_ERROR("[%s] Not supported thread type[%d].", __func__, threadType);
            ret = HCCL_E_NOT_SUPPORT;
            break;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], notifyIdx[%u], timeout[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), notifyIdx, timeout), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommAclrtNotifyRecordOnThread(ThreadHandle thread, uint64_t dstNotifyId)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], dstNotifyId[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dstNotifyId);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalNotifyRecord(dstNotifyId), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);
        ret = HcclLocalBareNotifyRecord(stream, dstNotifyId);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], dstNotifyId[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), dstNotifyId), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommAclrtNotifyWaitOnThread(ThreadHandle thread, uint64_t notifyId, uint32_t timeout)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], notifyId[%llu], timeout[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), notifyId, timeout);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        EXECEPTION_CATCH(ret = threadPtr->LocalNotifyWait(notifyId, timeout), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);
        ret = HcclLocalBareNotifyWait(stream, notifyId, timeout);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], notifyId[%llu], timeout[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), notifyId, timeout), ret);
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

    ThreadEntity *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(threads[0]);
    CHK_PTR_NULL(threadEntityPtr);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, threads[0], threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, threads[0], threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    Thread *threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    if (threadPtr->IsDeviceA5()) {
        HCCL_INFO("[%s] Running on A5.", __func__);
        for (uint32_t i = 0; i < threadNum; i++) {
            ThreadEntity *const threadEntityPtrLoop = reinterpret_cast<ThreadEntity *>(threads[i]);
            CHK_PTR_NULL(threadEntityPtrLoop);
            CHK_PRT_RET((threadEntityPtrLoop->engine != COMM_ENGINE_AICPU && threadEntityPtrLoop->engine != COMM_ENGINE_AICPU_TS),
                HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
                __func__, threadEntityPtrLoop->engine, threads[i], threadEntityPtrLoop->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
            CHK_PRT_RET(threadEntityPtrLoop->type != THREAD_TYPE_TS,
                HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
                __func__, threadEntityPtrLoop->type, threads[i], threadEntityPtrLoop->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
            Thread *threadPtrLoop = reinterpret_cast<Thread *>(threadEntityPtrLoop->threadObjAddr);
            CHK_PTR_NULL(threadPtrLoop);
            HCCL_INFO("[%s] Launching task in thread[0x%llx][%s].",
                __func__, threads[i], threadEntityPtrLoop->DescribeAttr().c_str());
            EXECEPTION_CATCH(threadPtrLoop->LaunchTask(), return HCCL_E_INTERNAL);
        }
        return HCCL_SUCCESS;
    }

    std::vector<hccl::Stream> streams;
    for (uint32_t i = 0; i < threadNum; i++) {
        ThreadEntity *const threadEntityPtrLoop = reinterpret_cast<ThreadEntity *>(threads[i]);
        CHK_PTR_NULL(threadEntityPtrLoop);
        CHK_PRT_RET((threadEntityPtrLoop->engine != COMM_ENGINE_AICPU && threadEntityPtrLoop->engine != COMM_ENGINE_AICPU_TS),
            HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
            __func__, threadEntityPtrLoop->engine, threads[i], threadEntityPtrLoop->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
        CHK_PRT_RET(threadEntityPtrLoop->type != THREAD_TYPE_TS,
            HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
            __func__, threadEntityPtrLoop->type, threads[i], threadEntityPtrLoop->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
        hccl::Stream *stream = GetStream(threadEntityPtrLoop->threadObjAddr);
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
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
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
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};

        EXECEPTION_CATCH(ubTransportLitePtr->Write(locRmaBuf, rmtBuf, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf rmtBuf{dst, len, nullptr};

        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteWrite(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
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
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};

        ret = CheckDataTypeAndReduceOp(dataType, reduceOp);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at CheckDataTypeAndReduceOp. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp), ret);
        Hccl::ReduceIn reduceIn{mapHcommDataTypeToA5.at(dataType), mapHcommReduceOpToA5.at(reduceOp)};

        EXECEPTION_CATCH(ubTransportLitePtr->WriteReduce(locRmaBuf, rmtBuf, reduceIn, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
            "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
        HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf rmtBuf{dst, len, nullptr};
        HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteWriteReduce(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf, reduceInfo);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

HcclResult CommWriteReduceWithNotify(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp, uint32_t remoteNotifyIdx)
{
    ThreadEntity *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    AddThread(threadEntityPtr->threadObjAddr);
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
        "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    HcclBuf locBuf{const_cast<void*>(src), count * SIZE_TABLE[dataType], nullptr};
    HcclBuf rmtBuf{dst, count * SIZE_TABLE[dataType], nullptr};
    HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

    Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(stream);

    return HcclRemoteWriteReduceWithNotify(stream, reinterpret_cast<void*>(channel), &rmtBuf, &locBuf, reduceInfo,
        remoteNotifyIdx);
}

int32_t HcommWriteWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len, remoteNotifyIdx);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
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
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len, remoteNotifyIdx), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};

        Hccl::WithNotifyIn withNotify{Hccl::TransportNotifyType::NORMAL, remoteNotifyIdx};

        EXECEPTION_CATCH(ubTransportLitePtr->WriteWithNotify(locRmaBuf, rmtBuf, withNotify, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf rmtBuf{dst, len, nullptr};

        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteWriteWithNotify(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf, remoteNotifyIdx);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteReduceWithNotifyOnThread(ThreadHandle thread, ChannelHandle channel, void *dst,
    const void *src, uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp, uint32_t remoteNotifyIdx)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
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
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(dst), len};
        
        ret = CheckDataTypeAndReduceOp(dataType, reduceOp);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at CheckDataTypeAndReduceOp. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx), ret);
        Hccl::ReduceIn reduceIn{mapHcommDataTypeToA5.at(dataType), mapHcommReduceOpToA5.at(reduceOp)};

        Hccl::WithNotifyIn withNotify{Hccl::TransportNotifyType::NORMAL, remoteNotifyIdx};

        EXECEPTION_CATCH(ubTransportLitePtr->WriteReduceWithNotify(locRmaBuf, rmtBuf, reduceIn, withNotify, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        ret = HCCL_E_NOT_SUPPORT;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d], remoteNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommReadOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        auto *const transportLitePtr = reinterpret_cast<Hccl::BaseTransportLiteImpl *>(channel);
        CHK_PTR_NULL(transportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = transportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(dst), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(src), len};

        EXECEPTION_CATCH(transportLitePtr->Read(locRmaBuf, rmtBuf, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        HcclBuf locBuf{dst, len, nullptr};
        HcclBuf rmtBuf{const_cast<void *>(src), len, nullptr};

        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteRead(stream, reinterpret_cast<void *>(channel), &locBuf, &rmtBuf);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommReadReduceOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t count, HcommDataType dataType, HcommReduceOp reduceOp)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        auto *const transportLitePtr = reinterpret_cast<Hccl::BaseTransportLiteImpl *>(channel);
        CHK_PTR_NULL(transportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        Hccl::RmaBufferLite locRmaBuf;
        ret = transportLitePtr->BuildLocRmaBufferLite(reinterpret_cast<uintptr_t>(dst), len, locRmaBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at BuildLocRmaBufferLite. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp), ret);
        const Hccl::Buffer rmtBuf{reinterpret_cast<uintptr_t>(src), len};

        ret = CheckDataTypeAndReduceOp(dataType, reduceOp);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL at CheckDataTypeAndReduceOp. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp), ret);
        Hccl::ReduceIn reduceIn{mapHcommDataTypeToA5.at(dataType), mapHcommReduceOpToA5.at(reduceOp)};

        EXECEPTION_CATCH(transportLitePtr->ReadReduce(locRmaBuf, rmtBuf, reduceIn, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[%s] Not support reduce, "
            "dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d]", __func__, dst, src, count, dataType, reduceOp), HCCL_E_PARA);
        HcclBuf locBuf{dst, len, nullptr};
        HcclBuf rmtBuf{const_cast<void *>(src), len, nullptr};
        HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteReadReduce(stream, reinterpret_cast<void *>(channel), &locBuf, &rmtBuf, reduceInfo);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteNbiOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_DEBUG("[%s] thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, thread, channel, dst, src, len);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommWriteNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_DEBUG("[%s] channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, channel, dst, src, len);
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommWriteWithNotifyNbiOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx)
{
    HCCL_DEBUG("[%s] thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, thread, channel, dst, src, len, remoteNotifyIdx);
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

int32_t HcommReadNbiOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_DEBUG("[%s] thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, thread, channel, dst, src, len);
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
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], remoteNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, remoteNotifyIdx);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        HCCL_DEBUG("[%s] Running on A5.", __func__);
        auto *const transportLitePtr = reinterpret_cast<Hccl::BaseTransportLiteImpl *>(channel);
        CHK_PTR_NULL(transportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);
        HCCL_INFO("channel streamlite ptr %p.", streamLitePtr);

        EXECEPTION_CATCH(transportLitePtr->Post(remoteNotifyIdx, *streamLitePtr), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteNotifyRecord(stream, reinterpret_cast<void *>(channel), remoteNotifyIdx);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], remoteNotifyIdx[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx)
{
    HCCL_DEBUG("[%s] channel[0x%llx], remoteNotifyIdx[%u].", __func__, channel, remoteNotifyIdx);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommChannelNotifyWaitOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeOut)
{
    auto *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_INFO("[%s] START. thread[0x%llx][%s], channel[0x%llx], localNotifyIdx[%u], timeOut[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, localNotifyIdx, timeOut);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);

    AddThread(threadEntityPtr->threadObjAddr);

    Thread *const threadPtr = reinterpret_cast<Thread *>(threadEntityPtr->threadObjAddr);
    CHK_PTR_NULL(threadPtr);

    HcclResult ret = HCCL_SUCCESS;
    if (threadPtr->IsDeviceA5()) {
        HCCL_DEBUG("[%s] Running on A5.", __func__);
        auto *const transportLitePtr = reinterpret_cast<Hccl::BaseTransportLiteImpl *>(channel);
        CHK_PTR_NULL(transportLitePtr);
        auto *const streamLitePtr = static_cast<Hccl::StreamLite *>(threadPtr->GetStreamLitePtr());
        CHK_PTR_NULL(streamLitePtr);

        EXECEPTION_CATCH(transportLitePtr->WaitWithTimeout(localNotifyIdx, *streamLitePtr, timeOut), ret = HCCL_E_INTERNAL);
    } else {
        Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
        CHK_PTR_NULL(stream);

        ret = HcclRemoteNotifyWait(stream, reinterpret_cast<void *>(channel), localNotifyIdx, timeOut);
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx][%s], channel[0x%llx], localNotifyIdx[%u], timeOut[%u].",
        __func__, thread, threadEntityPtr->DescribeAttr().c_str(), channel, localNotifyIdx, timeOut), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeOut)
{
    HCCL_DEBUG("[%s] channel[0x%llx], localNotifyIdx[%u], timeOut[%u].", __func__, channel, localNotifyIdx, timeOut);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult CommFence(ThreadHandle thread, ChannelHandle channel) // 控制前后的任务保序
{
    ThreadEntity *const threadEntityPtr = reinterpret_cast<ThreadEntity *>(thread);
    CHK_PTR_NULL(threadEntityPtr);
    HCCL_DEBUG("[CommFence] thread[0x%llx][%s], channel[0x%llx].",
        thread, threadEntityPtr->DescribeAttr().c_str(), channel);
    CHK_PRT_RET((threadEntityPtr->engine != COMM_ENGINE_AICPU && threadEntityPtr->engine != COMM_ENGINE_AICPU_TS),
        HCCL_ERROR("[%s] Not supported thread engine[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->engine, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    CHK_PRT_RET(threadEntityPtr->type != THREAD_TYPE_TS,
        HCCL_ERROR("[%s] Not supported thread type[%d]. thread[0x%llx][%s].",
        __func__, threadEntityPtr->type, thread, threadEntityPtr->DescribeAttr().c_str()), HCCL_E_NOT_SUPPORT);
    Stream *stream = GetStream(threadEntityPtr->threadObjAddr);
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
    DevType deviceType;
    CHK_RET(hrtGetDeviceType(deviceType));
    HCCL_INFO("[%s]comId[%s], devType[%d]", __func__, commId, deviceType);
    if (deviceType != DevType::DEV_TYPE_950) {
        HcclCommAicpu *hcclComm = AicpuHcclProcess::AicpuGetCommbyGroup(commId);
        CHK_PRT_RET(!hcclComm, HCCL_ERROR("%s AicpuGetCommbyGroup is null, commId[%s]", __func__, commId), HCCL_E_PTR);
        CHK_RET(hcclComm->SetDispatcherCtxOnThread());
    } else {
        CollCommAicpuMgr *hcclComm = AicpuIndopProcess::AicpuGetCommMgrbyGroup(commId);
        CHK_PRT_RET(!hcclComm, HCCL_ERROR("%s AicpuGetCommMgrbyGroup is null, commId[%s]", __func__, commId), HCCL_E_PTR);
    }
    return HCCL_SUCCESS;
}

int32_t HcommChannelRegisterDfx(ChannelHandle channel, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback) {
    HCCL_INFO("[HcommChannelRegisterDfx] Init begin");
    auto *const transportLitePtr = reinterpret_cast<Hccl::BaseTransportLiteImpl *>(channel);
    CHK_PTR_NULL(transportLitePtr);
    CHK_RET(transportLitePtr->SetAddTaskInfoCallback(callback));
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
    DevType deviceType;
    CHK_RET(hrtGetDeviceType(deviceType));
    HCCL_INFO("[%s]comId[%s], devType[%d]", __func__, commId, deviceType);
    if (deviceType != DevType::DEV_TYPE_950) {
        AicpuHcclProcess::AicpuReleaseCommbyGroup(commId);
    } else {
        AicpuIndopProcess::AicpuReleaseCommMgrbyGroup(commId);
    }
    return HCCL_SUCCESS;
}

int32_t HcommFenceOnThread(ThreadHandle thread)
{
    HCCL_DEBUG("[%s] thread[0x%llx].", __func__, thread);
    return HCCL_E_NOT_SUPPORT;
}

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
int32_t HcommFlush()
{
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommChannelFenceOnThread(ThreadHandle thread, ChannelHandle channel)
{
    HCCL_DEBUG("[%s] thread[0x%llx], channel[0x%llx].", __func__, thread, channel);
    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    if (threadPtr->IsDeviceA5()) {
        auto *const ubTransportLitePtr = reinterpret_cast<Hccl::UbTransportLiteImpl *>(channel);
        CHK_PTR_NULL(ubTransportLitePtr);
        CHK_RET(ubTransportLitePtr->Fence());
    }

    return HCCL_SUCCESS;
}

int32_t HcommChannelFence(ChannelHandle channel)
{
    HCCL_DEBUG("[%s] channel[0x%llx].", __func__, channel);
    return HCCL_E_NOT_SUPPORT;
}

int32_t HcommRequestServiceOnThread(ThreadHandle dstThreadHandle, ThreadServiceHandle serviceHandle, const void *args, uint64_t argsSizeByte)
{
    ThreadEntity *const dstThreadEntityPtr = reinterpret_cast<ThreadEntity *>(dstThreadHandle);
    void *const serviceHandlePtr = reinterpret_cast<void *>(serviceHandle);
    CHK_PTR_NULL(dstThreadEntityPtr);
    CHK_PTR_NULL(serviceHandlePtr);
    CHK_PTR_NULL(args);
    HCCL_INFO("[%s] START. dstThreadHandle[0x%llx][%s], serviceHandle[0x%llx], args[0x%llx], argsSizeByte[%llu].",
        __func__, dstThreadHandle, dstThreadEntityPtr->DescribeAttr().c_str(), serviceHandle, args, argsSizeByte);

    if (!(dstThreadEntityPtr->engine == COMM_ENGINE_AICPU && dstThreadEntityPtr->type == THREAD_TYPE_CPU)) {
        HCCL_ERROR("[%s] thread engine should be COMM_ENGINE_AICPU and type should be THREAD_TYPE_CPU.", __func__);
        return HCCL_E_NOT_SUPPORT;
    }

    // Serialize enqueue sequence to prevent interleaving across concurrent callers.
    static std::mutex s_requestServiceMtx;
    std::lock_guard<std::mutex> lock(s_requestServiceMtx);

    const hccl::QueueInfo &queueInfo = dstThreadEntityPtr->cpuRes.sendQueue;
    const uint64_t headIdx = *reinterpret_cast<uint64_t *>(queueInfo.headIdxAddr);
    const uint64_t tailIdx = *reinterpret_cast<uint64_t *>(queueInfo.tailIdxAddr);
    const uint64_t capacity = queueInfo.capacity;
    const uint64_t nextTail = (tailIdx + 1) % capacity;
    if (nextTail == headIdx) {
        HCCL_ERROR("[%s] FAIL. thread[0x%llx]'s send queue is full. headIdx[%llu], tailIdx[%llu], capacity[%llu].",
            __func__, dstThreadHandle, headIdx, tailIdx, capacity);
        return HCCL_E_INTERNAL;
    }
    HCCL_INFO("[%s] thread[0x%llx]'s send queue is not full. headIdx[%llu], tailIdx[%llu], capacity[%llu].",
        __func__, dstThreadHandle, headIdx, tailIdx, capacity);

    // --- DataRing: 将 args 写入环形缓冲区 ---
    const hccl::DataRingInfo &ringInfo = dstThreadEntityPtr->cpuRes.dataRing;
    uint64_t ringHead = *reinterpret_cast<uint64_t *>(ringInfo.headIdxAddr);
    uint64_t ringTail = *reinterpret_cast<uint64_t *>(ringInfo.tailIdxAddr);
    const uint32_t ringCapacity = ringInfo.capacity;
    const uint64_t alignedSize = DATA_RING_ALIGN_UP(argsSizeByte, DATA_RING_ALIGNMENT);

    // 环绕处理: 若 entry 跨越缓冲区末尾，跳过剩余空间
    uint64_t bufTailPos = ringTail % ringCapacity;
    if (bufTailPos + alignedSize > ringCapacity) {
        uint64_t gap = ringCapacity - bufTailPos;
        ringTail += gap;
    }

    // 检查可用空间
    uint64_t usedSpace = ringTail - ringHead;
    if (usedSpace + alignedSize > ringCapacity) {
        HCCL_ERROR("[%s] FAIL. DataRing full. ringHead[%llu], ringTail[%llu], alignedSize[%llu], capacity[%u].",
            __func__, ringHead, ringTail, alignedSize, ringCapacity);
        return HCCL_E_INTERNAL;
    }

    // 写入 args 到 DataRing
    bufTailPos = ringTail % ringCapacity;
    void *const dstAddr = reinterpret_cast<void *>(ringInfo.addr + bufTailPos);
    errno_t cpyRet = memcpy_s(dstAddr, ringCapacity - bufTailPos, args, argsSizeByte);
    if (cpyRet != EOK) {
        HCCL_ERROR("[%s] FAIL. memcpy_s args to DataRing failed, errno[%d].", __func__, cpyRet);
        return HCCL_E_INTERNAL;
    }

    uint64_t argsOffset = ringTail;
    ringTail += alignedSize;
    *reinterpret_cast<uint64_t *>(ringInfo.tailIdxAddr) = ringTail;

    HCCL_INFO("[%s] Wrote args to DataRing at offset[%llu], bufPos[%llu], alignedSize[%llu].",
        __func__, argsOffset, argsOffset % ringCapacity, alignedSize);

    // Push service request to thread entity's send queue.
    // s_msgId is only used as a trace id in logs; 32-bit wraparound is acceptable and does not affect queue semantics.
    static uint32_t s_msgId = 0;
    ThreadMsgEntity *const msgQueueBasePtr = reinterpret_cast<ThreadMsgEntity *>(queueInfo.addr);
    ThreadMsgEntity tempMsgEnt = {s_msgId, serviceHandle, argsOffset, argsSizeByte};
    msgQueueBasePtr[tailIdx] = tempMsgEnt;
    *reinterpret_cast<uint64_t *>(queueInfo.tailIdxAddr) = (tailIdx + 1) % capacity;  // update tail index.
    HCCL_INFO("[%s] Push service request to thread[0x%llx]'s send queue SUCCESS. msgId[%u]. New headIdx[%llu], tailIdx[%llu]",
        __func__, dstThreadHandle, s_msgId, 
        *reinterpret_cast<uint64_t *>(queueInfo.headIdxAddr), *reinterpret_cast<uint64_t *>(queueInfo.tailIdxAddr));
    s_msgId++;
    return HCCL_SUCCESS;
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
#ifdef __cplusplus
}
#endif  // __cplusplus

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
    flagTaskInfo.streamId = streamLitePtr->GetSqId();
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
    flagTaskInfo.streamId = streamLitePtr->GetSqId();
    flagTaskInfo.taskId = streamLitePtr->GetRtsq()->GetTaskId();
    flagTaskInfo.type = Hccl::MainStreamTaskType::TAIL;
    
    Hccl::ProfilingHandlerLite::GetInstance().ReportMainStreamTask(flagTaskInfo);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}
