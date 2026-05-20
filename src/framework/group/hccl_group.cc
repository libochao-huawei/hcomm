/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <mutex>
#include <thread>
#include <algorithm>

#include <acl/acl.h>
#include "hccl_group.h"
#include <thread>
#include <chrono>

using namespace hccl;

thread_local s32 hcclGroupDepth = 0;
thread_local std::deque<std::shared_ptr<struct hcclAsyncJob>> hcclInitJobs;
thread_local std::vector<HcclComm> hcclGroupCommList;

HcclResult HcclGroupStart()
{
    hcclGroupDepth++;
    HCCL_INFO("[HcclGroupStart] hcclGroupDepth=[%d]", hcclGroupDepth);
    return HCCL_SUCCESS;
}

namespace hccl{

HcclResult initGroupPlanner(HcclComm comm) {
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    u32 rankSize = INVALID_VALUE_RANKSIZE;
    hcclComm->GetRankSize(rankSize);
    planner->rankSize = rankSize;
    HCCL_DEBUG("[initGroupPlanner] ranksize: %d", rankSize);
    
    planner->nTasksP2p = 0;
    planner->nTasksColl = 0;
    return HCCL_SUCCESS;
}

HcclResult taskAppend(HcclComm comm, hcclOpInfo& info) {
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    if(planner->nTasksP2p == -1){
        initGroupPlanner(comm);
    }

    HcclResult ret = HCCL_SUCCESS;
    if (info.coll == HcclCMDType::HCCL_CMD_SEND || info.coll == HcclCMDType::HCCL_CMD_RECEIVE) {
        hcclComm->SetGroupMode(true);
        bool isSendOp = (info.coll == HcclCMDType::HCCL_CMD_SEND);
        HcclSendRecvItem item;
        item.sendRecvType = isSendOp ? HcclSendRecvType::HCCL_SEND : HcclSendRecvType::HCCL_RECV;
        item.buf = const_cast<void*>(isSendOp ? info.sendbuff : info.recvbuff);
        item.count  = isSendOp ? info.sendCount : info.recvCount;
        item.dataType = isSendOp ? info.sendType : info.recvType;
        item.remoteRank = info.root;

        planner->sendRecvInfo.push_back(item);

        if (planner->sendRecvMainStream == nullptr) { // 用第一条用户流作为主流
            planner->sendRecvMainStream = info.stream;
            HCCL_INFO("[TaskAppend] planner->sendRecvMainStream[%p]", planner->sendRecvMainStream);
        }
        
        planner->nTasksP2p += 1;
    }
    else {
        hcclOpInfo task = info;
        planner->collTaskQueue.push_back(task);
        planner->nTasksColl += 1;
        /*记录stream到planner*/
        planner->collStreams.insert(info.stream);
    }

    auto itComm = std::find(hcclGroupCommList.begin(), hcclGroupCommList.end(), comm);
    if (itComm == hcclGroupCommList.end()) {
        hcclGroupCommList.push_back(comm);
    }
    return ret;
}

HcclResult commInitTaskAppend(std::shared_ptr<struct hcclAsyncJob> job, HcclResult (*func)(struct hcclAsyncJob *), HcclComm* comm)
{
    HCCL_INFO("[hcclAsyncJobEnqueue] add item to queue");
    /*hcclAsyncLaunch只是将job放入队列，并不等待执行完成。groupLaunch->asyncJobLaunch中给每个job起一个线程去执行*/
    job->func = func;
    job->comm = comm;
    job->state = hcclGroupJobRunning;
    hcclInitJobs.push_back(job);
    return HCCL_SUCCESS;
}
}// namespace hccl

void *hcclAsyncJobMain(void *arg)
{
    struct hcclAsyncJob *job = (struct hcclAsyncJob *)arg;
    job->result = job->func(job); /*func是上层asyncjob里面设置的函数*/
    if (job->result == HCCL_SUCCESS) {
        HCCL_INFO("Function launch success");
    }
    /*加锁修改job->state为hcclGroupJobDone*/
    std::unique_lock<std::mutex> lock(job->mtx);
    job->state = hcclGroupJobDone;
    return arg;
}

static HcclResult asyncJobLaunch()
{
    HCCL_DEBUG("[asyncJobLaunch] entered");
    HcclResult ret = HCCL_SUCCESS;
    bool jobsDone = false;

    if (!hcclInitJobs.empty()) {
        for (auto job : hcclInitJobs) {
            CHK_PRT_RET(!job, HCCL_ERROR("[asyncJobLaunch] job is nullptr"), HCCL_E_INTERNAL);
            job->thread.reset(new (std::nothrow) std::thread(&hcclAsyncJobMain, job.get()));
            CHK_PRT_RET(!job->thread, HCCL_ERROR("[asyncJobLaunch]threads reset failed "), HCCL_E_INTERNAL);
        }

        do { /*主线程轮询阻塞，等待所有线程上的asyncJob执行完成*/
            jobsDone = true;
            for (auto job : hcclInitJobs) {
                /*上面job执行线程可能并发修改state，在主线程里面要通过加线程锁来读取*/
                hcclGroupJobState_t state = hcclGroupJobJoined;
                std::unique_lock<std::mutex> lock(job->mtx);
                state = job->state;

                if (state == hcclGroupJobRunning) {
                    jobsDone = false;
                } else if (state == hcclGroupJobDone) {
                    job->thread->join();
                    job->state = hcclGroupJobJoined;
                    if (job->result != HCCL_SUCCESS && ret == HCCL_SUCCESS) {
                        ret = job->result;
                    }
                } else {
                    /* safety check */
                    CHK_PRT_RET(state != hcclGroupJobJoined,
                        HCCL_ERROR("[asyncJobLaunch] state != hcclGroupJobJoined"),
                        HCCL_E_INTERNAL);
                }
            }
            // Let preconnect threads progress.
            if (jobsDone == false)
                usleep(1);
        } while (jobsDone == false);
        hcclInitJobs.clear();
        CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[asyncJobLaunch] fail!"), ret);
    }
    return HCCL_SUCCESS;
}

static HcclResult dispatchDirect(hcclOpInfo& taskColl)
{
    switch (taskColl.coll) {
        case HcclCMDType::HCCL_CMD_ALLGATHER:
            HcclAllGatherInner(const_cast<void *>(taskColl.sendbuff), const_cast<void *>(taskColl.recvbuff), taskColl.sendCount,
                            taskColl.sendType, taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER:
            HcclReduceScatterInner(const_cast<void *>(taskColl.sendbuff), const_cast<void *>(taskColl.recvbuff), taskColl.recvCount,
                        taskColl.recvType, taskColl.op, taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_ALLREDUCE:
            HcclAllReduceInner(const_cast<void *>(taskColl.sendbuff), const_cast<void *>(taskColl.recvbuff), taskColl.sendCount, taskColl.sendType,
                            taskColl.op, taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_BROADCAST:
            HcclBroadcastInner(const_cast<void *>(taskColl.sendbuff), taskColl.sendCount, taskColl.sendType, taskColl.root, taskColl.comm,
                            taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_ALLTOALL:
            HcclAlltoAllInner(taskColl.sendbuff, taskColl.sendCount, taskColl.sendType, taskColl.recvbuff, taskColl.recvCount, taskColl.recvType,
                            taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_ALLTOALLV:
            HcclAlltoAllVInner(taskColl.sendbuff, taskColl.sendCounts, taskColl.sdispls, taskColl.sendType,
                            taskColl.recvbuff, taskColl.recvCounts, taskColl.rdispls, taskColl.recvType,
                            taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_ALLTOALLVC:
            HcclAlltoAllVCInner(taskColl.sendbuff, taskColl.sendCounts, taskColl.sendType, taskColl.recvbuff, taskColl.recvType,
                            taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_REDUCE:
            HcclReduceInner(const_cast<void *>(taskColl.sendbuff), const_cast<void *>(taskColl.recvbuff), taskColl.recvCount, taskColl.recvType, taskColl.op, taskColl.root,
                            taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_SCATTER:
            HcclScatterInner(const_cast<void *>(taskColl.sendbuff), const_cast<void *>(taskColl.recvbuff), taskColl.recvCount, taskColl.recvType, taskColl.root, taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_ALLGATHER_V:
            HcclAllGatherVInner(const_cast<void *>(taskColl.sendbuff), taskColl.sendCount, const_cast<void *>(taskColl.recvbuff), taskColl.recvCounts, taskColl.rdispls,
                            taskColl.recvType, taskColl.comm, taskColl.stream);
            break;
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V:
            HcclReduceScatterVInner(const_cast<void *>(taskColl.sendbuff), taskColl.sendCounts, taskColl.sdispls, const_cast<void *>(taskColl.recvbuff), taskColl.recvCount,
                                taskColl.sendType, taskColl.op, taskColl.comm, taskColl.stream);
            break;
        default:
            HCCL_ERROR("[doLaunches] not supported hcclFunc!");
            break;
    }
    return HCCL_SUCCESS;
}

static HcclResult flushFusionBatch(
    std::vector<hcclOpInfo>& batchQ,
    HcclCMDType opType,
    u32 gatherSplitNum, u32 scatterSplitNum,
    void* inBuffer, void* outBuffer,
    HcclDataType dataType, HcclReduceOp reduceOp,
    const std::string& tag,
    hccl::hcclComm* hcclComm)
{
    if (batchQ.empty()) return HCCL_SUCCESS;
    u32 n = static_cast<u32>(batchQ.size());
    std::vector<void*> sendBufs(n);
    std::vector<void*> recvBufs(n);
    std::vector<u64> counts(n);
    u64 totalCount = 0;
    HcclRtStream stream = batchQ[0].stream;
    for (u32 i = 0; i < n; ++i) {
        sendBufs[i] = const_cast<void*>(batchQ[i].sendbuff);
        recvBufs[i] = const_cast<void*>(batchQ[i].recvbuff);
        counts[i] = (opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER) ? batchQ[i].recvCount : batchQ[i].sendCount;
        totalCount += counts[i];
    }

    CHK_RET(hcclComm->LocalGather(tag, sendBufs.data(), counts.data(), n,
        inBuffer, gatherSplitNum, dataType, stream));
    // LocalGather已使用CCLIn后半段，后续集合通信算子只能使用CCL前半段
    hcclComm->SetHalfCclBuffer(true);
    bool isDebug = false;
    // [DEBUG] Checkpoint 1: dump inBuffer after LocalGather
    if (isDebug)
    {
        CHK_RET(hcclStreamSynchronize(stream));
        u32 rankId = 0;
        hcclComm->GetUserRank(rankId);
        u32 unitSize = SIZE_TABLE[dataType];
        u64 inBytes = totalCount * unitSize;
        void* hostBuf = nullptr;
        aclrtMallocHost(&hostBuf, inBytes);
        aclrtMemcpy(hostBuf, inBytes, inBuffer, inBytes, ACL_MEMCPY_DEVICE_TO_HOST);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        HCCL_INFO("[RANK_%u][CKPT1_AfterLocalGather] totalCount=%llu inBuffer\n", rankId, totalCount);
        int* f = (int*)hostBuf;
        std::string dump;
        for (u32 i = 0; i < totalCount; i++) {
            dump += " " + std::to_string(f[i]);
        }
        HCCL_INFO("CCLIN: %s", dump.c_str());
        aclrtFreeHost(hostBuf);
    }
    switch (opType) {
        case HcclCMDType::HCCL_CMD_ALLREDUCE:
            CHK_RET(HcclAllReduceInner(inBuffer, outBuffer, totalCount,
                dataType, reduceOp, hcclComm, stream));
            break;
        case HcclCMDType::HCCL_CMD_REDUCE_SCATTER:
            CHK_RET(HcclReduceScatterInner(inBuffer, outBuffer, totalCount,
                dataType, reduceOp, hcclComm, stream));
            break;
        case HcclCMDType::HCCL_CMD_ALLGATHER:
            CHK_RET(HcclAllGatherInner(inBuffer, outBuffer, totalCount,
                dataType, hcclComm, stream));
            break;
        default:
            break;
    }

    // [DEBUG] Checkpoint 2: dump outBuffer after Collectives
    if (isDebug)
    {
        CHK_RET(hcclStreamSynchronize(stream));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        u32 rankId = 0;
        hcclComm->GetUserRank(rankId);
        u32 unitSize = SIZE_TABLE[dataType];
        u32 rankSize = 0;
        hcclComm->GetRankSize(rankSize);
        u64 outBytes = totalCount * unitSize * scatterSplitNum * rankSize;
        void* hostBuf = nullptr;
        aclrtMallocHost(&hostBuf, outBytes);
        aclrtMemcpy(hostBuf, outBytes, outBuffer, outBytes, ACL_MEMCPY_DEVICE_TO_HOST);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        HCCL_INFO("[RANK_%u][CKPT2_AfterCollectives] totalCount=%llu outBuffer\n", rankId, totalCount);
        int* f = (int*)hostBuf;
        std::string dump;
        for (u32 r = 0; r < rankSize; r++) {
            std::string dump;
            for (u32 i = 0; i < totalCount * scatterSplitNum; i++) {
                dump += " " + std::to_string(f[r * totalCount * scatterSplitNum + i]);
            }
            HCCL_INFO("CCLOut: [rank=%u] %s", r, dump.c_str());
        }
        aclrtFreeHost(hostBuf);
    }

    CHK_RET(hcclComm->LocalScatter(tag, recvBufs.data(), counts.data(), n,
        outBuffer, scatterSplitNum, dataType, stream));
    // 恢复CCL buffer为全量
    hcclComm->SetHalfCclBuffer(false);

    // [DEBUG] Checkpoint 3: dump each recvBuf after LocalScatter, one line per split
    if (isDebug)
    {
        CHK_RET(hcclStreamSynchronize(stream));
        u32 rankId = 0;
        hcclComm->GetUserRank(rankId);
        u32 unitSize = SIZE_TABLE[dataType];
        for (u32 i = 0; i < n; i++) {
            u64 bufBytes = counts[i] * unitSize * scatterSplitNum;
            void* hostBuf = nullptr;
            aclrtMallocHost(&hostBuf, bufBytes);
            aclrtMemcpy(hostBuf, bufBytes, recvBufs[i], bufBytes, ACL_MEMCPY_DEVICE_TO_HOST);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            HCCL_INFO("[RANK_%u][CKPT3_AfterLocalScatter] recvBuf[%u] count=%llu splits=%u\n",
                rankId, i, counts[i], scatterSplitNum);
            int* f = (int*)hostBuf;
            for (u32 s = 0; s < scatterSplitNum; s++) {
                std::string dump;
                for (u32 j = 0; j < counts[i]; j++) {
                    dump += " " + std::to_string(f[s * counts[i] + j]);
                }
                HCCL_INFO("CCLRecv: [buf=%u][split=%u] %s", i, s, dump.c_str());
            }
            aclrtFreeHost(hostBuf);
        }
    }

    batchQ.clear();
    return HCCL_SUCCESS;
}

static HcclResult processFusionGroup(
    std::deque<hcclOpInfo>& tasks,
    HcclCMDType opType,
    u32 gatherSplitNum, u32 scatterSplitNum,
    u64 perTaskMultiplier,
    hccl::hcclComm* hcclComm)
{
    if (tasks.empty()) return HCCL_SUCCESS;

    HcclDataType dataType = tasks.front().sendType;
    HcclReduceOp reduceOp = tasks.front().op;

    // Verify homogeneity (same dataType, same reduction op for AR/RS)
    bool homogeneous = true;
    for (auto& t : tasks) {
        if (t.sendType != dataType) { homogeneous = false; break; }
        if (opType != HcclCMDType::HCCL_CMD_ALLGATHER && t.op != reduceOp) { homogeneous = false; break; }
    }
    if (!homogeneous) {
        while (!tasks.empty()) {
            hcclOpInfo t = tasks.front(); tasks.pop_front();
            CHK_RET(dispatchDirect(t));
        }
        return HCCL_SUCCESS;
    }

    // Sort by count descending
    std::sort(tasks.begin(), tasks.end(), [opType](const hcclOpInfo& a, const hcclOpInfo& b) {
        u64 cntA = (opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER) ? a.recvCount : a.sendCount;
        u64 cntB = (opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER) ? b.recvCount : b.sendCount;
        return cntA > cntB;
    });

    u32 unitSize = SIZE_TABLE[dataType];

    CHK_RET(hcclComm->CreateCommCCLbuffer());
    void* inBuffer = nullptr;
    void* outBuffer = nullptr;
    u64 bufSize = 0;
    CHK_RET(hcclComm->GetInCCLbuffer(inBuffer, bufSize));
    CHK_RET(hcclComm->GetOutCCLbuffer(outBuffer, bufSize));
    bufSize = bufSize / 2;
    inBuffer = static_cast<char*>(inBuffer) + bufSize;
    outBuffer = static_cast<char*>(outBuffer) + bufSize;

    const std::string tag = "FusedColl_" + hcclComm->GetIdentifier();

    std::vector<hcclOpInfo> batchQ;
    u64 remainingSize = bufSize;

    constexpr u64 ONE_MEGABYTE = 1 * 1024 * 1024;

    while (!tasks.empty()) {
        hcclOpInfo task = tasks.front();
        tasks.pop_front();
        u64 count = (opType == HcclCMDType::HCCL_CMD_REDUCE_SCATTER) ? task.recvCount : task.sendCount;
        u64 taskBytes = count * unitSize * perTaskMultiplier;

        if (taskBytes > ONE_MEGABYTE) {
            CHK_RET(flushFusionBatch(batchQ, opType, gatherSplitNum, scatterSplitNum,
                inBuffer, outBuffer, dataType, reduceOp, tag, hcclComm));
            remainingSize = bufSize;
            CHK_RET(dispatchDirect(task));
        } else if (taskBytes <= remainingSize) {
            batchQ.push_back(task);
            remainingSize -= taskBytes;
        } else {
            CHK_RET(flushFusionBatch(batchQ, opType, gatherSplitNum, scatterSplitNum,
                inBuffer, outBuffer, dataType, reduceOp, tag, hcclComm));
            remainingSize = bufSize;
            batchQ.push_back(task);
            remainingSize -= taskBytes;
        }
    }
    CHK_RET(flushFusionBatch(batchQ, opType, gatherSplitNum, scatterSplitNum,
        inBuffer, outBuffer, dataType, reduceOp, tag, hcclComm));
    return HCCL_SUCCESS;
}

static HcclResult doLaunches(HcclComm comm)
{
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    HcclUs startutime = TIME_NOW();
    if (planner->nTasksP2p != 0) {
        HCCL_INFO("HcclBatchSendRecvGroup, sendRecvInfo.size()[%u]", static_cast<u32>(planner->sendRecvInfo.size()));
        CHK_RET(HcclBatchSendRecvGroup(planner->sendRecvInfo.data(), planner->sendRecvInfo.size(), comm, planner->sendRecvMainStream));
    }
    HCCL_INFO("[doLaunches] take time [%lld]us.", DURATION_US(TIME_NOW() - startutime));
    if (planner->nTasksColl != 0) {
        u32 rankSize = INVALID_VALUE_RANKSIZE;
        hcclComm->GetRankSize(rankSize);

        // Separate tasks into fusion-capable groups and others
        std::deque<hcclOpInfo> arTasks, rsTasks, agTasks, otherTasks;
        while (!planner->collTaskQueue.empty()) {
            hcclOpInfo taskColl = planner->collTaskQueue.front();
            planner->collTaskQueue.pop_front();
            switch (taskColl.coll) {
                case HcclCMDType::HCCL_CMD_ALLREDUCE:
                    arTasks.push_back(taskColl);
                    break;
                case HcclCMDType::HCCL_CMD_REDUCE_SCATTER:
                    rsTasks.push_back(taskColl);
                    break;
                case HcclCMDType::HCCL_CMD_ALLGATHER:
                    agTasks.push_back(taskColl);
                    break;
                default:
                    otherTasks.push_back(taskColl);
                    break;
            }
        }

        // Dispatch non-fusion tasks directly
        while (!otherTasks.empty()) {
            hcclOpInfo taskColl = otherTasks.front();
            otherTasks.pop_front();
            CHK_RET(dispatchDirect(taskColl));
        }

        // Process each fusion group
        CHK_RET(processFusionGroup(arTasks, HcclCMDType::HCCL_CMD_ALLREDUCE,
            1, 1, 1, hcclComm));
        CHK_RET(processFusionGroup(rsTasks, HcclCMDType::HCCL_CMD_REDUCE_SCATTER,
            rankSize, 1, rankSize, hcclComm));
        CHK_RET(processFusionGroup(agTasks, HcclCMDType::HCCL_CMD_ALLGATHER,
            1, rankSize, rankSize, hcclComm));
    }
    return HCCL_SUCCESS;
}

static HcclResult groupLaunch()
{  // 将各种通信域初始化/destroy的asyncJobs，在这里触发放到背景线程执行
    HCCL_INFO("[groupLaunch] entered");

    asyncJobLaunch();
    HCCL_DEBUG("[groupLaunch] asyncJobLaunch done");
    for (HcclComm comm : hcclGroupCommList) {
        doLaunches(comm);
    }
    HCCL_INFO("[groupLaunch] doLaunches done");
    //流同步
    for (HcclComm comm : hcclGroupCommList){
        hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
        std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
        for (auto it : planner->collStreams){
            CHK_RET(hcclStreamSynchronize(it));
        }
    }
    HCCL_INFO("groupLauch Done!");
    return HCCL_SUCCESS;
}

inline void groupLocalResetJobState()
{
    // hcclcomm中group相关的变量
    for (HcclComm comm : hcclGroupCommList) {
        hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
        hcclComm->planner = std::make_shared<hcclKernelPlanner>();
        hcclComm->SetGroupMode(false);
    }
    hcclGroupCommList.clear();

    return;
}

HcclResult HcclGroupEnd()
{
    if (hcclGroupDepth == 0) {
        HCCL_ERROR("HcclGroupEnd: not in a group call. Didn't call HcclGroupStart before.");
        return HCCL_E_NOT_SUPPORT;
    }
    if (--hcclGroupDepth > 0) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[HcclGroupEnd] hcclGroupDepth=[%d]", hcclGroupDepth);
    /*遇到最后一个HcclGroupEnd才处理group内的所有任务*/

    groupLaunch();
    HCCL_INFO("[GroupEnd] done groupLaunch");
    groupLocalResetJobState();
    HCCL_INFO("[GroupEnd] to the end");
    return HCCL_SUCCESS;
}
