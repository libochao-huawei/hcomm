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
#include "adapter_hal_pub.h"
#include "adapter_rts.h"
#include "op_base_v2.h"
#include "host/host_cpu_roce_channel.h"
#include "hccl_comm_pub.h"
#include "op_base.h"
#include "dlprof_function.h"
#include "hcomm_c_adpt.h"
#include "hcclCommOp.h"
#include "adapter_prof.h"
#include "task_info.h"
#include "hccl_diag.h"

using namespace hccl;
thread_local LaunchContext g_threadLaunchCtx;

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

    if (threadPtr->IsDeviceA5()) {
        CHK_RET(threadPtr->LocalCopy(dst, src, len));
    } else {
        HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf dstBuf{dst, len, nullptr};
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        HcclResult ret = HcclLocalCopy(stream, &dstBuf, &srcBuf);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
            __func__, thread, dst, src, len), ret);
    }
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommLocalReduceOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t count,
    HcommDataType dataType, HcommReduceOp reduceOp)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, dst, src, count, dataType, reduceOp);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[HcommLocalReduceOnThread]Not support reduce, "
        "dst[%p], src[%p], count[%llu], dataType[%d], reduceOp[%d]", dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    if (threadPtr->IsDeviceA5()) {
        CHK_RET(threadPtr->LocalReduce(dst, src, len, dataType, reduceOp));
    } else {
        HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
        HcclBuf dstBuf{dst, len, nullptr};
        HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        HcclResult ret = HcclLocalCopyReduce(stream, &dstBuf, &srcBuf, reduceInfo);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[%s] FAIL. thread[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
            __func__, thread, dst, src, count, dataType, reduceOp), ret);
    }
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyRecordOnThread(ThreadHandle thread, ThreadHandle dstThread, uint32_t dstNotifyIdx)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dstThread[0x%llx], dstNotifyIdx[%u].", __func__, thread, dstThread, dstNotifyIdx);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    if (threadPtr->IsDeviceA5()) {
        HcclResult ret = threadPtr->LocalNotifyRecord(dstThread, dstNotifyIdx);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstThread[0x%llx], notifyIdx[%u].",
            __func__, thread, dstThread, dstNotifyIdx), ret);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);

        LocalNotify *notify = GetNotify(dstThread, dstNotifyIdx);
        CHK_PTR_NULL(notify);

        HcclResult ret = HcclLocalNotifyRecord(stream, notify);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstThread[0x%llx], notifyIdx[%u].",
            __func__, thread, dstThread, dstNotifyIdx), ret);
    }

    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyWaitOnThread(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeOut)
{
    HCCL_INFO("[%s] START. thread[0x%llx], notifyIdx[%u], timeOut[%u].", __func__, thread, notifyIdx, timeOut);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    if (threadPtr->IsDeviceA5()) {
        HcclResult ret = threadPtr->LocalNotifyWait(notifyIdx, timeOut);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], notifyIdx[%u], timeOut[%u].",
            __func__, thread, notifyIdx, timeOut), ret);
    } else {
        Stream *stream = GetStream(thread);
        CHK_PTR_NULL(stream);
        LocalNotify *notify = GetNotify(thread, notifyIdx);
        CHK_PTR_NULL(notify);

        HcclResult ret = HcclLocalNotifyWait(stream, notify, timeOut);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], notifyIdx[%u], timeOut[%u].",
            __func__, thread, notifyIdx, timeOut), ret);
    }
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommAclrtNotifyRecordOnThread(ThreadHandle thread, uint64_t dstNotifyId)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dstNotifyId[%u].", __func__, thread, dstNotifyId);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclLocalBareNotifyRecord(stream, dstNotifyId);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstNotifyId[%u].", __func__, thread, dstNotifyId), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommAclrtNotifyWaitOnThread(ThreadHandle thread, uint64_t notifyId, uint32_t timeOut)
{
    HCCL_INFO("[%s] START. thread[0x%llx], notifyId[%llu], timeOut[%u].", __func__, thread, notifyId, timeOut);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclLocalBareNotifyWait(stream, notifyId, timeOut);
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

    std::vector<hccl::Stream> streams;
    for (uint32_t i = 0; i < threadNum; i++) {
        hccl::Stream *stream = GetStream(threads[i]);
        CHK_PTR_NULL(stream);
        streams.push_back(*stream);
    }

    return HcclTaskLaunch(streams.data(), threadNum);
}

int32_t HcommWriteOnThread(ThreadHandle thread, ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, thread, channel, dst, src, len);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
    HcclBuf rmtBuf{dst, len, nullptr};

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteWrite(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf);
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
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[HcommWriteReduceOnThread]Not support reduce, "
        "dst[%p], src[%p], count[%llu], dataType[%d], reduceOp[%d]", dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
    HcclBuf rmtBuf{dst, len, nullptr};
    HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteWriteReduce(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf, reduceInfo);
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
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[CommWriteReduceWithNotify]Not support reduce, "
        "dst[%p], src[%p], count[%llu], dataType[%d], reduceOp[%d]", dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    AddThread(thread);
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

    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    HcclBuf locBuf{const_cast<void *>(src), len, nullptr};
    HcclBuf rmtBuf{dst, len, nullptr};

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteWriteWithNotify(stream, reinterpret_cast<void *>(channel), &rmtBuf, &locBuf, remoteNotifyIdx);
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

    ret = HCCL_E_NOT_SUPPORT;
    (void)len;

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

    HcclBuf locBuf{dst, len, nullptr};
    HcclBuf rmtBuf{const_cast<void *>(src), len, nullptr};

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteRead(stream, reinterpret_cast<void *>(channel), &locBuf, &rmtBuf);
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
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[HcommReadReduceOnThread]Not support reduce, "
        "dst[%p], src[%p], count[%llu], dataType[%d], reduceOp[%d]", dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclBuf locBuf{dst, len, nullptr};
    HcclBuf rmtBuf{const_cast<void *>(src), len, nullptr};
    HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteReadReduce(stream, reinterpret_cast<void *>(channel), &locBuf, &rmtBuf, reduceInfo);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].",
        __func__, thread, channel, dst, src, count, dataType, reduceOp), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[%s] START. channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, channel, dst, src, len);

    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);

    auto *const hostCpuRoceChannelPtr = reinterpret_cast<hcomm::HostCpuRoceChannel *>(channel);
    CHK_PTR_NULL(hostCpuRoceChannelPtr);

    HcclResult ret = hostCpuRoceChannelPtr->Write(dst, src, len);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, channel, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommWriteWithNotifyNbi(ChannelHandle channel, void *dst, const void *src,
    uint64_t len, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, channel, dst, src, len, remoteNotifyIdx);

    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);

    HcclResult ret = HCCL_SUCCESS;
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    if (devType == DevType::DEV_TYPE_950) {
        auto *const hostCpuRoceChannelPtr = reinterpret_cast<hcomm::HostCpuRoceChannel *>(channel);
        CHK_PTR_NULL(hostCpuRoceChannelPtr);
        ret = hostCpuRoceChannelPtr->WriteWithNotify(dst, src, len, remoteNotifyIdx);
    } else {
        ret = HCCL_E_NOT_SUPPORT;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu], remoteNotifyIdx[%u].",
        __func__, channel, dst, src, len, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommReadNbi(ChannelHandle channel, void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[%s] START. channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, channel, dst, src, len);

    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);

    auto *const hostCpuRoceChannelPtr = reinterpret_cast<hcomm::HostCpuRoceChannel *>(channel);
    CHK_PTR_NULL(hostCpuRoceChannelPtr);

    HcclResult ret = hostCpuRoceChannelPtr->Read(dst, src, len);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s] FAIL. channel[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].",
        __func__, channel, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyRecordOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], remoteNotifyIdx[%u].", __func__, thread, channel, remoteNotifyIdx);

    AddThread(thread);

    Thread *threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteNotifyRecord(stream, reinterpret_cast<void *>(channel), remoteNotifyIdx);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], remoteNotifyIdx[%u].", __func__, thread, channel, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return ret;
}

int32_t HcommChannelNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. channel[0x%llx], remoteNotifyIdx[%u].", __func__, channel, remoteNotifyIdx);

    HcclResult ret = HCCL_SUCCESS;
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    if (devType == DevType::DEV_TYPE_950) {
        auto *const hostCpuRoceChannelPtr = reinterpret_cast<hcomm::HostCpuRoceChannel *>(channel);
        CHK_PTR_NULL(hostCpuRoceChannelPtr);
        ret = hostCpuRoceChannelPtr->NotifyRecord(remoteNotifyIdx);
    } else {
        ret = HCCL_E_NOT_SUPPORT;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. channel[0x%llx], remoteNotifyIdx[%u].", __func__, channel, remoteNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyWaitOnThread(ThreadHandle thread, ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout)
{
    HCCL_INFO("[%s] START. thread[0x%llx], channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, thread, channel, localNotifyIdx, timeout);

    AddThread(thread);

    Thread *threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclRemoteNotifyWait(stream, reinterpret_cast<void *>(channel), localNotifyIdx, timeout);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, thread, channel, localNotifyIdx, timeout), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t timeout)
{
    HCCL_INFO("[%s] START. channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, channel, localNotifyIdx, timeout);

    HcclResult ret = HCCL_SUCCESS;
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    if (devType == DevType::DEV_TYPE_950) {
        auto *const hostCpuRoceChannelPtr = reinterpret_cast<hcomm::HostCpuRoceChannel *>(channel);
        CHK_PTR_NULL(hostCpuRoceChannelPtr);
        ret = hostCpuRoceChannelPtr->NotifyWait(localNotifyIdx, timeout);
    } else {
        ret = HCCL_E_NOT_SUPPORT;
    }
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. channel[0x%llx], localNotifyIdx[%u], timeout[%u].", __func__, channel, localNotifyIdx, timeout), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
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

int32_t HcommThreadRegisterDfx(ThreadHandle thread, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback)
{
    HCCL_INFO("[HcommThreadRegisterDfx] Init begin");
    Thread *threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    CHK_RET(threadPtr->SetAddTaskInfoCallback(callback));
    HCCL_INFO("[HcommThreadRegisterDfx] Init success");
    return HCCL_SUCCESS;
}

int32_t HcommAcquireComm(const char* commId)
{
    CHK_PTR_NULL(commId);
    std::shared_ptr<hccl::hcclComm> hcclComm;
    HcclGetCommHandle(commId, hcclComm);
    CHK_PRT_RET(hcclComm == nullptr, HCCL_ERROR("%s hcclComm is null, commId[%s]", __func__, commId), HCCL_E_PTR);
    CHK_PRT(hcclComm->SetCommDispatcherCtx());// 待优化
    return HCCL_SUCCESS;
}

int32_t HcommReleaseComm(const char* commId)
{
    CHK_PTR_NULL(commId);
    HCCL_INFO("%s not support, commId[%s], do nothing", __func__, commId);
    return HCCL_SUCCESS;
}

int32_t HcommFlush()
{
    HCCL_INFO("[%s] START.", __func__);
    HcclResult ret = HcommFlushV2();
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL.", __func__), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommChannelFence(ChannelHandle channel)
{
    HCCL_INFO("[%s] START. channel[0x%llx].", __func__, channel);

    auto *const hostCpuRoceChannelPtr = reinterpret_cast<hcomm::HostCpuRoceChannel *>(channel);
    CHK_PTR_NULL(hostCpuRoceChannelPtr);

    HcclResult ret = hostCpuRoceChannelPtr->ChannelFence();
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. channel[0x%llx].", __func__, channel), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

HcclResult HcclDfxRegOpInfo(HcclComm comm, void* hcclDfxOpInfo)
{
    bool l0State = Hccl::ProfilingHandler::GetInstance().GetHcclL0State();
    bool l1State = Hccl::ProfilingHandler::GetInstance().GetHcclL1State();
    if (l0State == false || l1State == false) {
        HCCL_INFO("[HcclDfxRegOpInfo] profiling State is down l0State %d l1State %d", l0State, l1State);
    }
    u64 beginTime = Hccl::DlProfFunction::GetInstance().dlMsprofSysCycleTime();

    CHK_PRT_RET(hcclDfxOpInfo == nullptr,  HCCL_ERROR("[%s] hcclDfxOpInfo is null", __func__), HCCL_E_PTR);
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    HcclDfxOpInfo *dfxOpInfo = static_cast<HcclDfxOpInfo*>(hcclDfxOpInfo);
    CHK_PTR_NULL(dfxOpInfo);
    auto hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    dfxOpInfo->beginTime = hrtMsprofSysCycleTime();

    if (dfxOpInfo->engine == COMM_ENGINE_AICPU_TS) {
        LocalNotify *notify = GetNotify(dfxOpInfo->cpuTsThread, dfxOpInfo->cpuWaitAicpuNotifyIdx);
        CHK_PRT_RET(!notify, HCCL_ERROR("[%s]GetNotify null, thread[%llu], notifyIdx[%u]",
            __func__, dfxOpInfo->cpuTsThread, dfxOpInfo->cpuWaitAicpuNotifyIdx), HCCL_E_PTR);
        dfxOpInfo->cpuWaitAicpuNotifyId = notify->notifyId_;

        Stream *cpuTsStream = GetStream(dfxOpInfo->cpuTsThread);
        CHK_PTR_NULL(cpuTsStream);
        collComm->RegisterAicpuTaskExceptionCallback(cpuTsStream->id());
    }

    //HcclDfxOpInfo转为DfxOpInfo
    auto dfxOpInfoOnce = ConvertToDfxOpInfo(*dfxOpInfo);
    dfxOpInfoOnce->comm_ = static_cast<void*>(collComm);
    dfxOpInfoOnce->isIndop_ = true;
    dfxOpInfoOnce->groupName_ = collComm->GetCommId(); 
    dfxOpInfoOnce->opIndex_ = collComm->UpdateIndex();
    dfxOpInfoOnce->rankSize_ = collComm->GetRankSize();
    //单算子模式，暂时覆盖opTag
    dfxOpInfoOnce->op_.opTag = collComm->GetCommId();
    dfxOpInfoOnce->op_.myRank = static_cast<Hccl::RankId>(collComm->GetMyRankId());

    HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
    CHK_PTR_NULL(hcclCommDfx);
    CHK_RET(hcclCommDfx->UpdateProfStat());
    Hccl::MirrorTaskManager* mirrorTaskManage = hcclCommDfx->GetMirrorTaskManager();
    CHK_PTR_NULL(mirrorTaskManage);
    mirrorTaskManage->SetCurrDfxOpInfo(dfxOpInfoOnce);
   
    // 下发device侧
    CHK_RET(HcommDfxKernelLaunch(hcclComm->GetIdentifier(),hcclComm->GetBinHandle(), *dfxOpInfo));
    const std::string KernelName = "RunAicpuDfxOpInfoInitV2";
    CHK_RET(hcclCommDfx->ReportKernel(beginTime, hcclComm->GetIdentifier(), KernelName, SalGetTid()));
    return HCCL_SUCCESS;
}

HcclResult HcclProfilingReportOp(HcclComm comm, uint64_t beginTime)
{
    HCCL_INFO("[%s] START, comm[%p].", __func__, comm);
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
    CHK_PTR_NULL(hcclCommDfx);
    HCCL_INFO("[%s] Report All Tasks Info, comm[%p], hcclCommDfx[%p] GetMirrorTaskManager[%p].",
        __func__, comm, hcclCommDfx, hcclCommDfx->GetMirrorTaskManager());
    //单算子模式暂时默认true
    CHK_RET(hcclCommDfx->ReportAllTasks(true));
    CHK_RET(hcclCommDfx->ReportOp(beginTime, true, true));
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

HcclResult HcclReportAicpuKernel(HcclComm comm, uint64_t beginTime, char* kernelName)
{
    HCCL_INFO("[%s] START, comm[%p].", __func__, comm);
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    CHK_PRT_RET(kernelName == nullptr,  HCCL_ERROR("[%s] kernelName is null", __func__), HCCL_E_PTR);
    //填入remoteRankId
    auto hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__);
        return HCCL_E_NOT_SUPPORT;
    }
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
    CHK_PTR_NULL(hcclCommDfx);

    std::string kernelNameStr(kernelName);
    uint32_t threadId = SalGetTid();
    CHK_RET(hcclCommDfx->ReportKernel(beginTime, collComm->GetCommId(), kernelNameStr, threadId));
    HCCL_INFO("[HcclReportAicpuKernel] HcclReportAicpuKernel sucess");
    return HCCL_SUCCESS;
}
