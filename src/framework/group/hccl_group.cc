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
#include "hccl_group_utils.h"
#include "hccl_aicpu_interface.h"
#include "coll_alg_utils.h"
#include "hccl_res_expt.h"

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
u32 pow2Up(u32 n) {
    u32 power = 1;
    while (power < n) { power *= 2; }
    return power;
}

void serverRankMapCreate(const std::vector<RankInfo>& rankList,
    std::map<u32, std::vector<u32>>& serverToRankList,
    std::map<u32, u32>& serverToRankSize)
{
    for (u32 rankIdx = 0; rankIdx < rankList.size(); rankIdx++) {
        u32 serverIdx = rankList[rankIdx].serverIdx;
        serverToRankList[serverIdx].emplace_back(rankIdx);
        serverToRankSize[serverIdx]++;
    }
}

u32 calculateGroupSize(u32 rankSize, u32 serverNum,
    const std::map<u32, u32>& serverToRankSize)
{
    if (serverNum == 1) {
        return rankSize;
    }
    
    std::vector<u32> serverRankSizeList;
    for (const auto& pair : serverToRankSize) {
        serverRankSizeList.emplace_back(pair.second);
    }
    return CalGCD(serverRankSizeList);
}

void groupServerMapCreate(const std::map<u32, std::vector<u32>>& serverToRankList,
    const std::map<u32, u32>& serverToRankSize,
    u32 groupSize, u32 nGroups,
    std::vector<u32>& groupToServer,
    std::vector<u32>& groupToLocalRankBase)
{
    u32 groupIdx = 0;
    for (const auto& pair : serverToRankList) {
        u32 serverIdx = pair.first;
        u32 localRankSize = serverToRankSize.at(serverIdx);
        u32 localGroupSize = localRankSize / groupSize;
        for (u32 localGroupIdx = 0; localGroupIdx < localGroupSize; localGroupIdx++) {
            groupToServer[groupIdx] = serverIdx;
            groupToLocalRankBase[groupIdx] = localGroupIdx * groupSize;
            groupIdx++;
        }
    }
}

u32 generateP2pSchedule(std::shared_ptr<struct hcclKernelPlanner> planner,
    const std::map<u32, std::vector<u32>>& serverToRankList,
    const std::vector<u32>& groupToServer,
    const std::vector<u32>& groupToLocalRankBase,
    u32 rankSize, u32 groupSize, u32 nGroups, u32 nGroupsPow2,
    u32 curGroupIdx, u32 curGroupLocalRankIdx)
{
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
                u32 sendLocalIdx = groupToLocalRankBase[sendGroupIdx] 
                    + (curGroupLocalRankIdx + delta) % groupSize;
                u32 recvLocalIdx = groupToLocalRankBase[recvGroupIdx] 
                    + (curGroupLocalRankIdx - delta + groupSize) % groupSize;

                planner->p2pSchedule[round].sendRank = serverToRankList.at(sendServerIdx)[sendLocalIdx];
                planner->p2pSchedule[round].recvRank = serverToRankList.at(recvServerIdx)[recvLocalIdx];
                round++;
            }
        }
        groupRound++;
        groupDelta = (groupDelta + groupRound) & (nGroupsPow2 - 1);
    } while (groupRound != nGroupsPow2);
    
    return round;
}

HcclResult HcclP2pSchedulerGenerate(HcclComm comm, u32 rankSize)
{
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm*>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    u32 userRank = INVALID_VALUE_RANKID;
    hcclComm->GetUserRank(userRank);
    u32 serverNum = hcclComm->GetServerNum();
    std::vector<RankInfo> rankList = hcclComm->GetRankLists();
    u32 curlocalRank = rankList[userRank].localRank;

    std::map<u32, std::vector<u32>> serverToRankList;
    std::map<u32, u32> serverToRankSize;
    serverRankMapCreate(rankList, serverToRankList, serverToRankSize);

    u32 groupSize = calculateGroupSize(rankSize, serverNum, serverToRankSize);
    u32 curGroupLocalRankIdx = curlocalRank % groupSize;
    u32 curGroupIdx = curlocalRank / groupSize;
    u32 nGroups = rankSize / groupSize;
    u32 nGroupsPow2 = pow2Up(nGroups);
    HCCL_DEBUG("[HcclP2pSchedulerGenerate] groupSize:%u, nGroups:%u, nGroupsPow2:%u", 
        groupSize, nGroups, nGroupsPow2);

    std::vector<u32> groupToServer(nGroups);
    std::vector<u32> groupToLocalRankBase(nGroups);
    groupServerMapCreate(serverToRankList, serverToRankSize, groupSize, nGroups,
        groupToServer, groupToLocalRankBase);
    
    u32 round = generateP2pSchedule(planner, serverToRankList, groupToServer, groupToLocalRankBase,
        rankSize, groupSize, nGroups, nGroupsPow2, curGroupIdx, curGroupLocalRankIdx);

    if (rankSize != round) {
        HCCL_ERROR("[HcclP2pSchedulerGenerate] round:%u is not equal to rankSize:%u", round, rankSize);
        return HCCL_E_INTERNAL;
    }

    HCCL_INFO("[HcclP2pSchedulerGenerate] schedule generated, round:%u", round);
    return HCCL_SUCCESS;
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
    HcclResult ret = HCCL_SUCCESS;
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<struct hcclKernelPlanner> planner = hcclComm->planner;
    if(planner->nTasksP2p == -1){
        initGroupPlanner(comm);
        ret = HcclP2pSchedulerGenerate(comm, planner->rankSize);
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
        planner->peers.resize(planner->rankSize);
    }

    if (hcclP2pTaskNums == MAX_P2P_TASK_NUM) {
        HCCL_ERROR("[hcclGroupAddP2pTask] P2pTaskNums is out of %d", MAX_P2P_TASK_NUM);
        return HCCL_E_OOM;
    }

    hcclComm->SetGroupMode(true);
    if (p2pDesc.cmdType == HcclCMDType::HCCL_CMD_SEND) {
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
    std::string kernelName = "HcclLaunchAicpuKernel";
    aclrtFuncHandle funcHandle;
    aclrtArgsHandle argsHandle;
    // 注意，目前开源HCCL加载AICPU kernel使用的是从json文件加载
    // 详见load_kernel.cc中的LoadAICPUKernel函数，且只实现了scatter的，先共用scatter的
    aclError ret = aclrtBinaryGetFunction(g_binKernelHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, "
        "kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);
    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, "
        "kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);
    aclrtParamHandle paraHandle;
    size_t paramSize = sizeof(OpParam) + param.varMemSize;
    ret = aclrtKernelArgsAppend(argsHandle, &param, paramSize, &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, append "
        "size %u, kernelName:%s", ret, paramSize, kernelName.c_str()), HCCL_E_RUNTIME);
    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, "
        "kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);

    u32 kernelTimeoutTmp = param.execTimeout + KERNEL_TIMEOUT_OFFSET;
    u16 kernelLaunchTimeout = (kernelTimeoutTmp > UINT16_MAX) ? UINT16_MAX : static_cast<u16>(kernelTimeoutTmp);
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = kernelLaunchTimeout;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 numBlocks = 1;
    HCCL_INFO("[AicpuKernelLaunch] unfoldThread [%lu]", unfoldThread);  // 通过Thread获取展开流stream
    void* unfoldStream = nullptr;
    auto& HcclThreadResGetInfoFunc = ops_hccl::DlHcommFunction::GetInstance();
    ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, unfoldStream, &cfg, argsHandle, nullptr);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[LoadCustomKernel][aclrtLaunchKernelWithConfig]"
        "errNo[0x%016llx] launch kernel failed", ret), HCCL_E_OPEN_FILE_FAILURE);
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

static HcclResult LaunchNotifyRecordToThread(HcclComm comm, aclrtStream unfoldStream, ThreadHandle srcThread,
    ThreadHandle dstThread, uint32_t dstNotifyIdx, std::string kernelName)
{
    aclrtFuncHandle funcHandle;
    aclrtArgsHandle argsHandle;
    // 1. 获取 function handle
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto binKernelHandle = hcclComm->GetBinHandle();
    aclError ret = aclrtBinaryGetFunction(binKernelHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 2. 初始化 args handle
    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 3. 准备参数并 append
    ThreadNotifyRecordParam param;
    param.thread = srcThread;
    param.dstThread = dstThread;
    param.dstNotifyIdx = dstNotifyIdx;
    aclrtParamHandle paraHandle;
    ret = aclrtKernelArgsAppend(argsHandle, &param, sizeof(ThreadNotifyRecordParam), &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 4. finalize args
    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 5. 下发 kernel
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = NOTIFY_DEFAULT_WAIT_TIME;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 numBlocks = 1;

    ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, unfoldStream, &cfg, argsHandle, nullptr);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtLaunchKernelWithConfig]errNo[0x%016llx] launch kernel failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    return HCCL_SUCCESS;
}

static HcclResult LaunchNotifyWaitToThread(
    HcclComm comm, aclrtStream unfoldStream, ThreadHandle srcThread, uint32_t dstNotifyIdx, std::string kernelName)
{
    aclrtFuncHandle funcHandle;
    aclrtArgsHandle argsHandle;
    // 1. 获取 function handle
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto binKernelHandle = hcclComm->GetBinHandle();
    aclError ret = aclrtBinaryGetFunction(binKernelHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 2. 初始化 args handle
    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 3. 准备参数并 append
    ThreadNotifyWaitParam param;
    param.thread = srcThread;
    param.notifyIdx = dstNotifyIdx;
    param.timeOut = 1827;
    aclrtParamHandle paraHandle;
    ret = aclrtKernelArgsAppend(argsHandle, &param, sizeof(ThreadNotifyRecordParam), &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 4. finalize args
    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 5. 下发 kernel
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = NOTIFY_DEFAULT_WAIT_TIME;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 numBlocks = 1;

    ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, unfoldStream, &cfg, argsHandle, nullptr);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtLaunchKernelWithConfig]errNo[0x%016llx] launch kernel failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    return HCCL_SUCCESS;
}

static HcclResult LaunchP2pExec(HcclComm comm, aclrtStream usrStream, hccl::HcclKernelFuncInfo funcInfo, void *funcArgs,
    ThreadHandle sendRecvStream, std::string kernelName)
{
    aclrtFuncHandle funcHandle;
    aclrtArgsHandle argsHandle;
    // 1. 获取 function handle
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto binKernelHandle = hcclComm->GetBinHandle();
    aclError ret = aclrtBinaryGetFunction(binKernelHandle, kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 2. 初始化 args handle
    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 3. 准备参数并 append
    P2pGroupAicpuKernelParam param;
    param.funcInfo = funcInfo;
    param.funcArgs = funcArgs;
    param.sendRecvStream = sendRecvStream;

    aclrtParamHandle paraHandle;
    ret = aclrtKernelArgsAppend(argsHandle, &param, sizeof(ThreadNotifyRecordParam), &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 4. finalize args
    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    // 5. 下发 kernel
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = NOTIFY_DEFAULT_WAIT_TIME;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 numBlocks = 1;

    ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, usrStream, &cfg, argsHandle, nullptr);
    CHK_PRT_RET(ret != ACL_SUCCESS,
        HCCL_ERROR("[aclrtLaunchKernelWithConfig]errNo[0x%016llx] launch kernel failed, "
                   "kernelName:%s",
            ret, kernelName.c_str()),
        HCCL_E_RUNTIME);

    return HCCL_SUCCESS;
}

static HcclResult groupLaunchA5()
{
    constexpr u32 hostNotifyWaitTime = 1827;

    for (HcclComm comm : hcclGroupCommList) {
        /*新建send/recv流*/
        ThreadHandle aicpuSendThread = 0, aicpuRecvThread = 0;
        CHK_RET(HcclThreadAcquire(comm, COMM_ENGINE_AICPU_TS, 1, 1, &aicpuSendThread));
        CHK_RET(HcclThreadAcquire(comm, COMM_ENGINE_AICPU_TS, 1, 1, &aicpuRecvThread));
        std::vector<HcclP2pTask> sortedSendQue;
        std::vector<HcclP2pTask> sortedRecvQue;
        CHK_RET(HcclP2pTaskSchedule(comm, sortedSendQue, sortedRecvQue));
        aclrtStream usrStream = nullptr;
        ThreadHandle cpuTsThread = 0;
        ThreadHandle exportedAicpuTsThread = 0;
        ThreadHandle exportedCpuTsSendThread = 0;
        ThreadHandle exportedCpuTsRecvThread = 0;
        if (!sortedSendQue.empty()) {
            usrStream = sortedSendQue[0].stream;
        } else if (!sortedRecvQue.empty()) {
            usrStream = sortedRecvQue[0].stream;
        } else {
            continue;
        }
        CHK_RET(HcclThreadAcquireWithStream(comm, COMM_ENGINE_CPU_TS, usrStream, 2, &cpuTsThread));
        CHK_RET(HcclThreadExportToCommEngine(comm, 1, &cpuTsThread, COMM_ENGINE_AICPU_TS, &exportedAicpuTsThread));

        CHK_RET(HcclThreadExportToCommEngine(comm, 1, &aicpuSendThread, COMM_ENGINE_CPU_TS, &exportedCpuTsSendThread));
        CHK_RET(HcclThreadExportToCommEngine(comm, 1, &aicpuRecvThread, COMM_ENGINE_CPU_TS, &exportedCpuTsRecvThread));
        CHK_RET(static_cast<HcclResult>(HcommThreadNotifyRecordOnThread(cpuTsThread, exportedCpuTsSendThread, 0)));
        CHK_RET(static_cast<HcclResult>(HcommThreadNotifyRecordOnThread(cpuTsThread, exportedCpuTsRecvThread, 0)));

        // 下发wait kernel
        CHK_RET(LaunchNotifyWaitToThread(comm, usrStream, aicpuSendThread, 0, "RunAicpuNotifyWait"));
        CHK_RET(LaunchNotifyWaitToThread(comm, usrStream, aicpuRecvThread, 0, "RunAicpuNotifyWait"));
        /*待定，最好还是send/recv交替着发*/
        for (const auto &task : sortedSendQue) {
            CHK_RET(
                LaunchP2pExec(comm, task.stream, task.funcInfo, task.args, aicpuSendThread, "HcclP2pLaunchGroupAicpuKernel"));
        }

        for (const auto &task : sortedRecvQue) {
            CHK_RET(
                LaunchP2pExec(comm, task.stream, task.funcInfo, task.args, aicpuRecvThread, "HcclP2pLaunchGroupAicpuKernel"));
        }
        // 下发record kernel
        CHK_RET(
            LaunchNotifyRecordToThread(comm, usrStream, aicpuSendThread, exportedAicpuTsThread, 0, "RunAicpuNotifyRecord"));
        CHK_RET(
            LaunchNotifyRecordToThread(comm, usrStream, aicpuRecvThread, exportedAicpuTsThread, 1, "RunAicpuNotifyRecord"));

        CHK_RET(static_cast<HcclResult>(HcommThreadNotifyWaitOnThread(cpuTsThread, 0, hostNotifyWaitTime)));
        CHK_RET(static_cast<HcclResult>(HcommThreadNotifyWaitOnThread(cpuTsThread, 1, hostNotifyWaitTime)));
    }
    return HCCL_SUCCESS;
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

    if (!hcclGroupCommList.empty()) {
        hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm*>(hcclGroupCommList[0]);
        DevType devType;
        hcclComm->GetDevType(devType);
    
        if (devType == DevType::DEV_TYPE_950) {
            CHK_RET(groupLaunchA5());
            return HCCL_SUCCESS;
        }
    }
    

    groupLaunch();
    HCCL_INFO("[GroupEnd] done groupLaunch");
    groupLocalResetJobState();
    HCCL_INFO("[GroupEnd] to the end");
    return HCCL_SUCCESS;
}
