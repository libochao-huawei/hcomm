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
#include "op_base_v2.h"
#include "host/host_cpu_roce_channel.h"
#include "hccl_comm_pub.h"
#include "op_base.h"


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

    HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
    HcclBuf dstBuf{dst, len, nullptr};
    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclLocalCopy(stream, &dstBuf, &srcBuf);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dst[0x%llx], src[0x%llx], len[%llu].", __func__, thread, dst, src, len), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommLocalReduceOnThread(ThreadHandle thread, void *dst, const void *src, uint64_t count,
    HcommDataType dataType, HcommReduceOp reduceOp)
{
    HCCL_INFO("[%s] START. thread[0x%llx], dst[0x%llx], src[0x%llx], count[%llu], dataType[%d], reduceOp[%d].", __func__, thread, dst, src, count, dataType, reduceOp);

    CHK_PTR_NULL(dst);
    CHK_PTR_NULL(src);
    CHK_PRT_RET((IsSupportReduce(dataType, reduceOp) == false), HCCL_ERROR("[HcommLocalReduceOnThread]Not support reduce, "
        "dst[%p], src[%p], count[%llu], dataType[%d], reduceOp[%d]", dst, src, count, dataType, reduceOp), HCCL_E_PARA);
    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    uint64_t len = count * SIZE_TABLE[dataType];

    HcclBuf srcBuf{const_cast<void *>(src), len, nullptr};
    HcclBuf dstBuf{dst, len, nullptr};
    HcclReduceInfo reduceInfo{static_cast<HcclDataType>(dataType), static_cast<HcclReduceOp>(reduceOp)};
    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    HcclResult ret = HcclLocalCopyReduce(stream, &dstBuf, &srcBuf, reduceInfo);
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

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    LocalNotify *notify = GetNotify(dstThread, dstNotifyIdx);
    CHK_PTR_NULL(notify);

    if (threadPtr->IsDeviceA5())
    {
        HcclResult ret = notify->Post(*stream);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstThread[0x%llx], notifyIdx[%u].",
            __func__, thread, dstThread, dstNotifyIdx), ret);
        HCCL_INFO("[%s] SUCCESS.", __func__);
        return HCCL_SUCCESS;
    }

    HcclResult ret = HcclLocalNotifyRecord(stream, notify);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], dstThread[0x%llx], notifyIdx[%u].",
        __func__, thread, dstThread, dstNotifyIdx), ret);
    HCCL_INFO("[%s] SUCCESS.", __func__);
    return HCCL_SUCCESS;
}

int32_t HcommThreadNotifyWaitOnThread(ThreadHandle thread, uint32_t notifyIdx, uint32_t timeOut)
{
    HCCL_INFO("[%s] START. thread[0x%llx], notifyIdx[%u], timeOut[%u].", __func__, thread, notifyIdx, timeOut);

    AddThread(thread);

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);

    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);
    LocalNotify *notify = GetNotify(thread, notifyIdx);
    CHK_PTR_NULL(notify);

    if (threadPtr->IsDeviceA5()) {
        HcclResult ret = notify->Wait(*stream, timeOut);
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], notifyIdx[%u], timeOut[%u].",
            __func__, thread, notifyIdx, timeOut), ret);
        HCCL_INFO("[%s] SUCCESS.", __func__);
        return HCCL_SUCCESS;
    }

    HcclResult ret = HcclLocalNotifyWait(stream, notify, timeOut);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[%s] FAIL. thread[0x%llx], notifyIdx[%u], timeOut[%u].",
        __func__, thread, notifyIdx, timeOut), ret);
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
    if (devType == DevType::DEV_TYPE_910_95) {
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

int32_t HcommChannelNotifyRecordOnThread(ThreadHandle thread, ChannelHandle channel, const uint32_t remoteNotifyIdx)
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

int32_t HcommChannelNotifyRecord(ChannelHandle channel, const uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[%s] START. channel[0x%llx], remoteNotifyIdx[%u].", __func__, channel, remoteNotifyIdx);

    HcclResult ret = HCCL_SUCCESS;
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    if (devType == DevType::DEV_TYPE_910_95) {
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
    if (devType == DevType::DEV_TYPE_910_95) {
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

class Buffer {
public:
    Buffer(uintptr_t addr, std::size_t size);

    explicit Buffer(std::size_t size);

    virtual ~Buffer() = default;

    uintptr_t GetAddr() const;

    size_t GetSize() const;

    virtual std::string Describe() const;

    bool Contains(Buffer *buf) const;

    bool Contains(uintptr_t bufAddr, size_t bufSize) const;

    // "bool"运算符(可执行if(object){...}的操作判断该buffer是否为空)
    operator bool() const
    {
        return addr_ != 0;
    }

    // "=="运算符
    bool operator==(const Buffer &that) const
    {
        return (addr_ == that.GetAddr()) && (size_ == that.GetSize());
    }

    // "!="运算符
    bool operator!=(const Buffer &that) const
    {
        return (addr_ != that.GetAddr()) || (size_ != that.GetSize());
    }

protected:
    uintptr_t   addr_{0};
    std::size_t size_{0};
};//

class HcclDfxOpInfo {
public:
    std::string  opTag;
    RankId       myRank; //hcomm可填
    OpMode       opMode;
    HcclCMDType  opType = HcclCMDType::HCCL_CMD_INVALID;
    HcclReduceOp reduceType = HcclReduceOp::HCCL_REDUCE_RESERVED;
    HcclDataType dataType;
    HcclDataType outputType;
    u64          dataCount{0};
    u32          root = INVALID_VALUE_RANKID;
    u32          numBlocksLimit{0};
    bool         staticAddr{false};
    bool         staticShape{false};
    std::shared_ptr<Buffer> inputMem{nullptr};
    std::shared_ptr<Buffer> outputMem{nullptr};
    std::shared_ptr<Buffer> scratchMem{nullptr};
    std::string  tag_;
    AlgType      algType;
    u32          index_{0};
    u64          beginTime_;
    u64          endTime_;
    u32          mainStreamId_; 
    u32          notifyId; //host wait device notifyId
    union {
        struct {
            u64 dataCount;
            HcclDataType dataType;
            HcclDataType dataOutputType;
        } dataDes;
        struct {
            void* counts;
            void* displs;
            HcclDataType dataType;
        } vDataDes;
        struct {
            HcclDataType sendType;
            HcclDataType recvType;
            u64 sendCount;
            u64 recvCount;
        } all2AllDataDes;
        struct {
            HcclDataType sendType;
            HcclDataType recvType;
            void* sendCounts;
            void* recvCounts;
            void* sdispls;
            void* rdispls;
        } all2AllVDataDes;
        struct {
            HcclDataType sendType;
            HcclDataType recvType;
            void* sendCountMatrix;
        } all2AllVCDataDes;
        struct {
            HcclSendRecvItem* sendRecvItemsPtr;
            u32 itemNum;
        } batchSendRecvDataDes;
    };

public:
    std::string Describe() const
    {
        return StringFormat(
            "DfxOpInfo: [collOperator:[%s], tag:[%s], algType:[%u], index:[%u], beginTime:[%llu], endTime:[%llu]",
            CollOpToString(op_).c_str(), tag_.c_str(), algType_, index_, beginTime_, endTime_);
    }
};

void SetCollopDataDes(Hccl::CollOperator& collOp, const HcclDfxOpInfo& dfxOpInfo) {
    if (collOp.opType == Hccl::OpType::ALLGATHERV) {
        collOp.vDataDes.counts = dfxOpInfo.vDataDes.counts;
        collOp.vDataDes.displs = dfxOpInfo.vDataDes.displs;
        collOp.vDataDes.dataType = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.vDataDes.dataType);
    } else if (collOp.opType == Hccl::OpType::ALLTOALL) {
        collOp.all2AllDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllDataDes.sendType);
        collOp.all2AllDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllDataDes.recvType);
        collOp.all2AllDataDes.sendCount = dfxOpInfo.all2AllDataDes.sendCount;
        collOp.all2AllDataDes.recvCount = dfxOpInfo.all2AllDataDes.recvCount;
    } else if (collOp.opType == Hccl::OpType::ALLTOALLV) {
        collOp.all2AllVDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVDataDes.sendType);
        collOp.all2AllVDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVDataDes.recvType);
        collOp.all2AllVDataDes.sendCounts = dfxOpInfo.all2AllVDataDes.sendCounts;
        collOp.all2AllVDataDes.recvCounts = dfxOpInfo.all2AllVDataDes.recvCounts;
        collOp.all2AllVDataDes.sdispls = dfxOpInfo.all2AllVDataDes.sdispls;
        collOp.all2AllVDataDes.rdispls = dfxOpInfo.all2AllVDataDes.rdispls;
    } else if (collOp.opType == Hccl::OpType::ALLTOALLVC) {
        collOp.all2AllVCDataDes.sendType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVCDataDes.sendType);
        collOp.all2AllVCDataDes.recvType  = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.all2AllVCDataDes.recvType);
        collOp.all2AllVCDataDes.sendCountMatrix = dfxOpInfo.all2AllVCDataDes.sendCountMatrix;
    } else if (collOp.opType == Hccl::OpType::BATCHSENDRECV) {
        collOp.batchSendRecvDataDes.itemNum = dfxOpInfo.batchSendRecvDataDes.itemNum;
        collOp.batchSendRecvDataDes.sendRecvItems = static_cast<void *>(dfxOpInfo.batchSendRecvDataDes.sendRecvItemsPtr);
    } else {
        collOp.dataDes.dataCount = dfxOpInfo.dataDes.dataCount
        collOp.dataDes.dataType = dfxOpInfo.dataDes.dataType;
        collOp.dataDes.strideCount = dfxOpInfo.dataDes.strideCount;
    }
}

HcclResult HcclDfxRegOpInfo(HcclComm comm, HcclDfxOpInfo dfxOpInfo)
{
    UpdataProfStat();
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__)

        return HCCL_E_NOT_SUPPORT;
    }
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    //单算子模式，覆盖opTag
    bool opBased = true;
    if (opBased) {
        dfxOpInfo.opTag = hcclComm->GetIdentifier();
    }
    
    dfxOpInfo.tag_ = OpTypeToString(dfxOpInfo.opType);//opType
    dfxOpInfo.index_ = 0;
    dfxOpInfo.beginTime_ = Hccl::DlProfFunction::GetInstance().dlMsprofSysCycleTime();

    //HcclDfxOpInfo转为DfxOpInfo
    auto dfxOpInfoOnce = std::make_shared<Hccl::DfxOpInfo>();
    Hccl::CollOperator collOp;
    collOp.opMode = dfxOpInfo.opMode; 
    collOp.opType = Hccl::OP_TYPE_MAP.at(dfxOpInfo.opType);
    collOp.reduceOp = Hccl::REDUCE_OP_MAP.at(dfxOpInfo.reduceOp);
    collOp.dataType = Hccl::DATA_TYPE_MAP.at(dfxOpInfo.dataType);
    collOp.outputDataType = ccl::DATA_TYPE_MAP.at(dfxOpInfo.outputDataType);
    collOp.dataCount = dfxOpInfo.dataCount;
    collOp.root = dfxOpInfo.root;
    collOp.staticAddr = dfxOpInfo.staticAddr;
    collOp.staticShape = dfxOpInfo.staticShape;
    collOp.numBlocksLimit = dfxOpInfo.numBlocksLimit;
    SetCollopDataDes(collOp, dfxOpInfo);
    collOp.inputMem = std::make_shared<Hccl::Buffer>(
        dfxOpInfo.inputMem->GetAddr(),
        dfxOpInfo.inputMem->GetSize()
    );
    collOp.outputMem = std::make_shared<Hccl::Buffer>(
        dfxOpInfo.outputMem->GetAddr(),
        dfxOpInfo.outputMem->GetSize()
    );
    collOp.srcatchMem = std::make_shared<Hccl::Buffer>(
        dfxOpInfo.srcatchMem->GetAddr(),
        dfxOpInfo.srcatchMem->GetSize()
    );
    
    dfxOpInfoOnce->op_= collOp;
    dfxOpInfoOnce->tag_ = dfxOpInfo.tag_;
    dfxOpInfoOnce->algType_ = dfxOpInfo.algType_;
    dfxOpInfoOnce->index_ = dfxOpInfo.index_;
    dfxOpInfoOnce->comm_ = hcclComm->GetCommunicatorV2();
    dfxOpInfoOnce->mainStreamId_ = dfxOpInfo.mainStreamId_;
    dfxOpInfoOnce->beginTime_ = dfxOpInfo.beginTime_;
 
    Hccl::MirrorTaskManager* mirrorTaskManage = collComm->GetMirrorTaskManager();
    CHK_PTR_NULL(mirrorTaskManage);
    mirrorTaskManage->SetCurrDfxOpInfo(dfxOpInfoOnce);
    return HCCL_SUCCESS;

}



HcclResult HcclProfilingReportOp(HcclComm comm, unit64_t beginTime)
{
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    HcclCommDfx* hcclCommDfx = collComm->GetHcclCommDfx();
    //单算子模式暂时默认true
    bool opbased = true;
    bool cachedReq = true;
    HcclCommDfx->ReportOp(beginTime, cachedReq, opbased);
    return HCCL_SUCCESS;
}

HcclResult HcclReportAicpuKernel(HcclComm comm, unit64_t beginTime, ThreadHandle thread)
{
    HCCL_INFO("[%s] START.", __func__);
    CHK_PRT_RET(thread == nullptr,  HCCL_ERROR("[%s] thread is null", __func__), HCCL_E_PTR);
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    u32 taskId = 0;
    u32 streamId =0 ;
    u32 remoteRankId = 0;
    //填充taskId和streamId
    HrtGetTaskIdAndStreamID(taskId, streamId);

    //填入remoteRankId
    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    CHK_PTR_NULL(hcclComm);
    if (!hcclComm->IsCommunicatorV2()) {
        HCCL_ERROR("[%s] comm is NOT_SUPPORT", __func__)

        return HCCL_E_NOT_SUPPORT;
    }
    hccl::CollComm* collComm = hcclComm->GetCollComm();
    CHK_PTR_NULL(collComm);
    remoteRankId = collComm->GetMyRankId();//todo::查表拿chanelhandle,才能拿到

    Thread *const threadPtr = reinterpret_cast<Thread *>(thread);
    CHK_PTR_NULL(threadPtr);
    auto *const streamPtr = static_cast<Hccl::Stream *>(threadPtr->GetStream());
    CHK_PRT_RET(streamPtr == nullptr,  HCCL_ERROR("[%s] streamPtr is null", __func__), HCCL_E_PTR); 
    Hccl::TaskParam taskParam {};
    taskParam.beginTime = beginTime;
    taskParam.taskType = TaskParamType::TASK_AICPU_KERNEL;
    taskParam.endTime = DlProfFunction::GetInstance().dlMsprofSysCycleTime();
 
    shared_ptr<TaskInfo> taskInfo = std::make_shared<TaskInfo>(streamId, taskId, remoteRankId, taskParam,
        comm->GetMirrorTaskManager().GetCurrDfxOpInfo(), streamPtr->GetIsMaster);
 
    comm->GetMirrorTaskManager().AddTaskInfo(taskInfo);
    HCCL_INFO("[%s] SUCCESS.", __func__);
}