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

#include "hccl_group.h"

#include "coll_alg_utils.h"
#include "acl/acl_rt.h"

using namespace hccl;

thread_local s32 hcclGroupDepth = 0;
thread_local s32 hcclP2pTaskNums = 0;
thread_local std::deque<std::shared_ptr<struct hcclAsyncJob>> hcclInitJobs;
thread_local std::vector<HcclComm> hcclGroupCommList;

constexpr s32 MAX_P2P_TASK_NUM = 2048;

HcclResult HcclGroupStart()
{
    hcclGroupDepth++;
    HCCL_INFO("[HcclGroupStart] hcclGroupDepth=[%d]", hcclGroupDepth);
    return HCCL_SUCCESS;
}

namespace {
u32 Pow2Up(u32 n) {
    u32 power = 1;
    while (power < n) { power *= 2; }
    return power;
}
}

namespace hccl{
HcclResult HcclP2pSchedulerGenerate(HcclComm comm, u32 rankSize)
{
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm*>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    u32 userRank = INVALID_VALUE_RANKID;
    hcclComm->GetUserRank(userRank);
    u32 serverNum = hcclComm->GetServerNum();
    std::vector<RankInfo> rankList = hcclComm->GetRankLists();
    u32 curlocalRank = rankList[userRank].localRank; // userRank在server内的自然排序号

    // 构建server和rank的映射
    std::map<u32, std::vector<u32>> serverToRankList;
    std::map<u32, u32> serverToRankSize;
    for (u32 rankIdx = 0; rankIdx < rankList.size(); rankIdx++) {
        u32 serverIdx = rankList[rankIdx].serverIdx;
        serverToRankList[serverIdx].emplace_back(rankIdx);
        serverToRankSize[serverIdx]++;
    }

    // 计算groupSize
    u32 groupSize = 0;
    if (serverNum == 1) {
        groupSize = rankSize;
    } else {
        groupSize = rankSize / serverNum;
        std::vector<u32> serverRankSizeList;
        for (auto& pair : serverToRankSize) {
            serverRankSizeList.emplace_back(pair.second);
        }
        u32 gcdRes = CalGCD(serverRankSizeList);
        groupSize = gcdRes;
    }

    u32 curGroupLocalRankIdx = curlocalRank % groupSize; // userRank在group内的自然排序号
    u32 curGroupIdx = curlocalRank / groupSize;          // userRank所在group的自然排序号
    u32 nGroups = rankSize / groupSize;
    u32 nGroupsPow2 = Pow2Up(nGroups);
    HCCL_DEBUG("[HcclP2pSchedulerGenerate] groupSize:%u, nGroups:%u, nGroupsPow2:%u", groupSize, nGroups, nGroupsPow2);

    std::vector<u32> groupToServer(nGroups);
    std::vector<u32> groupToLocalRankBase(nGroups); // group的起始rank在server内的自然排序号 
    u32 groupIdx = 0;
    for (auto& pair : serverToRankList) {
        u32 serverIdx = pair.first;
        u32 localRankSize = serverToRankSize[serverIdx];
        u32 localGroupSize = localRankSize / groupSize;
        for (u32 localGroupIdx = 0; localGroupIdx < localGroupSize; localGroupIdx++) {
            groupToServer[groupIdx] = serverIdx;
            groupToLocalRankBase[groupIdx] = localGroupIdx * groupSize;
            groupIdx++;
        }
    }
    
    // 生成调度表
    planner->p2pSchedule.resize(rankSize);
    u32 round = 0;
    u32 groupRound = 0;
    u32 groupDelta = 0;
    do {
        if (groupDelta < nGroups) {
            u32 sendGroupIdx = (curGroupIdx + groupDelta) % nGroups;
            u32 recvGroupIdx = (curGroupIdx - groupDelta + nGroups) % nGroups;
            u32 sendServerIdx = groupToServer[sendGroupIdx];
            u32 recvServerIdx = groupToServer[recvGroupIdx];
            
            for (u32 delta = 0; delta < groupSize; delta++) {
                u32 sendLocalIdx = groupToLocalRankBase[sendGroupIdx] + (curGroupLocalRankIdx + delta) % groupSize;
                u32 recvLocalIdx = groupToLocalRankBase[recvGroupIdx] 
                                    + (curGroupLocalRankIdx - delta + groupSize) % groupSize;

                planner->p2pSchedule[round].sendRank = serverToRankList[sendServerIdx][sendLocalIdx];
                planner->p2pSchedule[round].recvRank = serverToRankList[recvServerIdx][recvLocalIdx];
                round++;
            }
        }
        groupRound++;
        groupDelta = (groupDelta + groupRound) & (nGroupsPow2 - 1);
    } while (groupRound != nGroupsPow2);

    if (rankSize != round) {
        HCCL_ERROR("[HcclP2pSchedulerGenerate] round:%u is not equal to rankSize:%u", round, rankSize);
        return HCCL_E_INTERNAL;
    }

    HCCL_INFO("[HcclP2pSchedulerGenerate] schedule generated, round:%u", round);
    return HCCL_SUCCESS;
}

HcclResult HcclP2pTaskSchedule(
    HcclComm comm, std::vector<HcclP2pTask> &sortedSendQue, std::vector<HcclP2pTask> &sortedRecvQue)
{
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    HCCL_INFO("[HcclP2pTaskSchedule] nTaskP2p:%u", planner->nTasksP2p);

    u32 epoch = 0;
    while (planner->nTasksP2p > 0) {
        for (u32 round = 0; round < planner->rankSize; round++) {
            u32 sendRank = planner->p2pSchedule[round].sendRank;
            u32 recvRank = planner->p2pSchedule[round].recvRank;

            if (!planner->peers[sendRank].sendQue.empty()) {
                auto &sendTask = planner->peers[sendRank].sendQue.front();
                sortedSendQue.emplace_back(sendTask);
                planner->peers[sendRank].sendQue.pop_front();
                planner->nTasksP2p--;
            }

            if (!planner->peers[recvRank].recvQue.empty()) {
                auto &recvTask = planner->peers[recvRank].recvQue.front();
                sortedRecvQue.emplace_back(recvTask);
                planner->peers[recvRank].recvQue.pop_front();
                planner->nTasksP2p--;
            }
        }
        epoch++;
    }
    HCCL_INFO("[HcclP2pTaskSchedule] done, use epochs:%u, sendQueSize:%u, recvQueSize:%u ", epoch,
        static_cast<u32>(sortedSendQue.size()), static_cast<u32>(sortedRecvQue.size()));

    return HCCL_SUCCESS;
}

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

HcclResult HcclGroupAddP2pTask(HcclComm comm, const HcclP2pTask& task, const HcclOpP2pDesc& p2pDesc)
{
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    if(planner->nTasksP2p == -1){
        initGroupPlanner(comm);
        HcclP2pSchedulerGenerate(comm, planner->rankSize);
        planner->peers.resize(planner->rankSize);
    }

    if (hcclP2pTaskNums == MAX_P2P_TASK_NUM) {
        HCCL_ERROR("[hcclGroupAddP2pTask] P2pTaskNums is out of %d", MAX_P2P_TASK_NUM);
        return HCCL_E_OOM;
    }

    HcclResult ret = HCCL_SUCCESS;
    hcclComm->SetGroupMode(true);
    bool isSendOp = (p2pDesc.cmdType == HcclCMDType::HCCL_CMD_SEND);
    if (isSendOp) {
        planner->peers[p2pDesc.remoteRank].sendQue.emplace_back(task);
    } else {
        planner->peers[p2pDesc.remoteRank].recvQue.emplace_back(task);
    }
    planner->nTasksP2p += 1;

    auto itComm = std::find(hcclGroupCommList.begin(), hcclGroupCommList.end(), comm);
    if (itComm == hcclGroupCommList.end()) {
        hcclGroupCommList.emplace_back(comm);
    }
    return ret;
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

constexpr u32 KERNEL_TIMEOUT_OFFSET = 300;

static HcclResult AicpuKernelLaunchDirect(HcclComm comm, HcclOpDesc opInfo, HcclKernelFuncInfo funcInfo,
    void *args, uint32_t argSize, ThreadHandle aicpuThreadHandle, aclrtStream userStream)
{
    HCCL_INFO("[AicpuKernelLaunchDirect] kernelSo[%s], kernelFuncName[%s], argSize[%u]",
        funcInfo.kernelSo, funcInfo.kernelFuncName, argSize);

    std::string kernelName = funcInfo.kernelFuncName;
    aclrtFuncHandle funcHandle;
    aclError ret = aclrtBinaryGetFunction(nullptr, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[AicpuKernelLaunchDirect][aclrtBinaryGetFunction]"
        "errNo[0x%016llx] get func handle failed, kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);

    aclrtArgsHandle argsHandle;
    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[AicpuKernelLaunchDirect][aclrtKernelArgsInit]"
        "errNo[0x%016llx] args init failed, kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);

    aclrtParamHandle paraHandle;
    ret = aclrtKernelArgsAppend(argsHandle, args, argSize, &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[AicpuKernelLaunchDirect][aclrtKernelArgsAppend]"
        "errNo[0x%016llx] args append failed, argSize %u, kernelName:%s", ret, argSize, kernelName.c_str()),
        HCCL_E_RUNTIME);

    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[AicpuKernelLaunchDirect][aclrtKernelArgsFinalize]"
        "errNo[0x%016llx] args finalize failed, kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);

    u32 kernelTimeoutTmp = UINT16_MAX;
    u16 kernelLaunchTimeout = (kernelTimeoutTmp > UINT16_MAX) ? UINT16_MAX : static_cast<u16>(kernelTimeoutTmp);
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = kernelLaunchTimeout;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 numBlocks = 1;

    HCCL_INFO("[AicpuKernelLaunchDirect] aicpuThreadHandle [%lu]", aicpuThreadHandle);
    void* unfoldStream = nullptr;
    if (aicpuThreadHandle != 0) {
        HcclResult ret1 = HcclThreadResGetInfo(comm, aicpuThreadHandle,
            THREAD_RES_TYPE_STREAM, sizeof(void*), reinterpret_cast<void**>(&unfoldStream));
        if (ret1 == HCCL_E_NOT_SUPPORT) {
            ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, userStream, &cfg, argsHandle, nullptr);
        } else if (ret1 != HCCL_SUCCESS) {
            return ret1;
        } else {
            ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, static_cast<aclrtStream>(unfoldStream),
                &cfg, argsHandle, nullptr);
        }
    } else {
        ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, userStream, &cfg, argsHandle, nullptr);
    }
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[AicpuKernelLaunchDirect][aclrtLaunchKernelWithConfig]"
        "errNo[0x%016llx] launch kernel failed", ret), HCCL_E_INTERNAL);

    return HCCL_SUCCESS;
}

HcclResult HcclAicpuKernelLaunch(HcclComm comm, HcclOpDesc opInfo, HcclKernelFuncInfo funcInfo,
    void *args, uint32_t argSize, ThreadHandle aicpuThreadHandle, aclrtStream userStream)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(userStream);

    HCCL_INFO("[HcclAicpuKernelLaunch] opDescType[%u], kernelSo[%s], kernelFuncName[%s], argSize[%u], "
        "aicpuThreadHandle[%lu], hcclGroupDepth[%d]",
        opInfo.opDescType, funcInfo.kernelSo, funcInfo.kernelFuncName, argSize, aicpuThreadHandle, hcclGroupDepth);

    if (hcclGroupDepth > 0) {
        HCCL_INFO("[HcclAicpuKernelLaunch] group mode, add p2p task");
        HcclP2pTask task;
        task.desc = opInfo.p2p;
        task.stream = userStream;
        task.funcInfo = funcInfo;
        task.args = args;
        task.argSize = argSize;
        CHK_RET(HcclGroupAddP2pTask(comm, task, opInfo.p2p));
        hcclP2pTaskNums++;
        return HCCL_SUCCESS;
    }

    HCCL_INFO("[HcclAicpuKernelLaunch] non-group mode, direct launch");
    return AicpuKernelLaunchDirect(comm, opInfo, funcInfo, args, argSize, aicpuThreadHandle, userStream);
}

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

static HcclResult doLaunches(HcclComm comm)
{
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    HcclUs startutime = TIME_NOW();
    if (planner->nTasksP2p != 0) { 
        // 将所有send/recv的任务打包作为一个集合通信算子来执行
        HCCL_INFO("HcclBatchSendRecvGroup, sendRecvInfo.size()[%u]", static_cast<u32>(planner->sendRecvInfo.size()));
        CHK_RET(HcclBatchSendRecvGroup(planner->sendRecvInfo.data(), planner->sendRecvInfo.size(), comm, planner->sendRecvMainStream));
    }
    HCCL_INFO("[doLaunches] take time [%lld]us.", DURATION_US(TIME_NOW() - startutime));
    if (planner->nTasksColl != 0) {
        /*展开下发集合通信算子*/
        while(!planner->collTaskQueue.empty()){
            hcclOpInfo taskColl = planner->collTaskQueue.front();
            planner->collTaskQueue.pop_front();
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
        }
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
    hcclP2pTaskNums = 0;

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
