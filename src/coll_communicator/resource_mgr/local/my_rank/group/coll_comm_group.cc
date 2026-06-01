 /**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_comm_group.h"

#include <mutex>
#include <thread>

#include "hccl_aicpu_interface.h"
#include "coll_alg_utils.h"
#include "hccl_res_expt.h"
#include "coll_alg_utils.h"
#include "acl/acl_rt.h"
#include "hccl/hccl_launch.h"
#include "hccl_group.h"

using namespace hccl;

thread_local s32 hcclP2pTaskNums = 0;
thread_local std::vector<HcclComm> hcclGroupCommListV2;
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

u32 generateP2pSchedule(std::shared_ptr<hcclKernelPlannerV2> planner,
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
    std::shared_ptr<hcclKernelPlannerV2> planner = hcclComm->GetCollComm()->plannerV2;
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

HcclResult HcclP2pTaskSchedule(
    HcclComm comm, std::vector<HcclP2pTask> &sortedSendQue, std::vector<HcclP2pTask> &sortedRecvQue)
{
    hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<hcclKernelPlannerV2> planner = hcclComm->GetCollComm()->plannerV2;
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
    std::shared_ptr<hcclKernelPlannerV2> planner = hcclComm->GetCollComm()->plannerV2;
    u32 rankSize = INVALID_VALUE_RANKSIZE;
    hcclComm->GetRankSize(rankSize);
    planner->rankSize = rankSize;
    HCCL_DEBUG("[initGroupPlanner] ranksize: %d", rankSize);
    
    planner->nTasksP2p = 0;
    return HCCL_SUCCESS;
}

}

namespace hccl{
HcclResult HcclGroupAddP2pTaskV2(HcclComm comm, const HcclP2pTask& task, const HcclOpP2pDesc& p2pDesc)
{
    HcclResult ret = HCCL_SUCCESS;
    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::shared_ptr<hcclKernelPlannerV2> planner = hcclComm->GetCollComm()->plannerV2;
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

    auto itComm = std::find(hcclGroupCommListV2.begin(), hcclGroupCommListV2.end(), comm);
    if (itComm == hcclGroupCommListV2.end()) {
        hcclGroupCommListV2.emplace_back(comm);
    }
    return ret;
}
}// namespace hccl

static HcclResult AicpuKernelLaunchDirect(HcclComm comm, HcclOpDesc opInfo, HcclKernelFuncInfo funcInfo,
    void *args, uint32_t argSize, ThreadHandle aicpuThreadHandle, aclrtStream userStream)
{
    CHK_PTR_NULL(comm);

    ThreadHandle cpuTsThread{0};
    ThreadHandle exportedCpuTsThread{0};
    CHK_RET(HcclThreadAcquireWithStream(comm, COMM_ENGINE_CPU_TS, userStream, 1, &cpuTsThread));
    CHK_RET(HcclThreadExportToCommEngine(comm, 1, &aicpuThreadHandle, COMM_ENGINE_CPU_TS, &exportedCpuTsThread));
    // Host stream通知Device主thread，使用主流上idx最大的notify
    CHK_RET(static_cast<HcclResult>(
        HcommThreadNotifyRecordOnThread(cpuTsThread, exportedCpuTsThread, opInfo.p2p.notifyNum - 1)));

    // AicpuKernel report
    uint64_t beginTime = HcommGetProfilingSysCycleTime();

    if (argSize > 0 && args == nullptr) {
        HCCL_ERROR("[AicpuKernelLaunchDirect] args is null but argSize[%u] > 0", argSize);
        return HCCL_E_PTR;
    }

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm*>(comm);
    std::string kernelName = "HcclLaunchAicpuKernel";
    aclrtFuncHandle funcHandle;
    aclrtArgsHandle argsHandle;
    aclError ret = aclrtBinaryGetFunction(hcclComm->GetBinHcclHandle(), kernelName.c_str(), &funcHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtBinaryGetFunction]errNo[0x%016llx] get func handle failed, "
        "kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);
    ret = aclrtKernelArgsInit(funcHandle, &argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsInit]errNo[0x%016llx] args init failed, "
        "kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);
    aclrtParamHandle paraHandle;
    size_t paramSize = argSize;
    ret = aclrtKernelArgsAppend(argsHandle, args, paramSize, &paraHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsAppend]errNo[0x%016llx] args append failed, append "
        "size %u, kernelName:%s", ret, paramSize, kernelName.c_str()), HCCL_E_RUNTIME);
    ret = aclrtKernelArgsFinalize(argsHandle);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[aclrtKernelArgsFinalize]errNo[0x%016llx] args finalize failed, "
        "kernelName:%s", ret, kernelName.c_str()), HCCL_E_RUNTIME);

    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr attr;
    attr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    attr.value.timeout = NOTIFY_DEFAULT_WAIT_TIME;
    cfg.numAttrs = 1;
    cfg.attrs = &attr;
    constexpr u32 numBlocks = 1;

    ret = aclrtLaunchKernelWithConfig(funcHandle, numBlocks, opInfo.p2p.stream, &cfg, argsHandle, nullptr);
    CHK_PRT_RET(ret != ACL_SUCCESS, HCCL_ERROR("[LoadCustomKernel][aclrtLaunchKernelWithConfig]"
        "errNo[0x%016llx] launch kernel failed, launchStream[%p]", ret, userStream), HCCL_E_OPEN_FILE_FAILURE);

    char* kernelNameCStr = const_cast<char*>(kernelName.c_str());
    HcclResult ret1 = HcclReportAicpuKernel(comm, beginTime, kernelNameCStr);
    if (ret1 != HCCL_SUCCESS) {
        HCCL_ERROR("[HcclAicpuKernelEntranceLaunch] HcclReportAicpuKernel failed, beginTime %lu, kernelNameCStr %s, ret %d ", beginTime, kernelNameCStr, ret);
        return ret1;
    }
    // Host stream等待Device的通知
    u32 hostNotifyWaitTime = opInfo.p2p.hostNotifyWaitTime;
    CHK_RET(static_cast<HcclResult>(HcommThreadNotifyWaitOnThread(cpuTsThread, opInfo.p2p.aicpuRecordCpuIdx, hostNotifyWaitTime)));

    return HCCL_SUCCESS;
}

HcclResult HcclAicpuKernelLaunch(HcclComm comm, HcclOpDesc opInfo, HcclKernelFuncInfo funcInfo,
    void *args, uint32_t argSize, ThreadHandle aicpuThreadHandle, aclrtStream userStream)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(userStream);

    if (argSize > 0 && args == nullptr) {
        HCCL_ERROR("[HcclAicpuKernelLaunch] args is null but argSize[%u] > 0", argSize);
        return HCCL_E_PTR;
    }

    HCCL_INFO("[HcclAicpuKernelLaunch] opDescType[%u], kernelSo[%s], kernelFuncName[%s], argSize[%u], "
        "aicpuThreadHandle[%lu], hcclGroupDepth[%d]",
        opInfo.opDescType, funcInfo.kernelSo, funcInfo.kernelFuncName,
        argSize, aicpuThreadHandle, hcclGroupDepth);

    if (hcclGroupDepth > 0) {
        HCCL_INFO("[HcclAicpuKernelLaunch] group mode, add p2p task");
        HcclP2pTask task;
        task.desc = opInfo.p2p;
        task.stream = opInfo.p2p.stream;
        task.funcInfo = funcInfo;
        task.args = args;
        task.argSize = argSize;
        CHK_RET(HcclGroupAddP2pTaskV2(comm, task, opInfo.p2p));
        hcclP2pTaskNums++;
        return HCCL_SUCCESS;
    }

    HCCL_INFO("[HcclAicpuKernelLaunch] non-group mode, direct launch");
    return AicpuKernelLaunchDirect(comm, opInfo, funcInfo, args, argSize, aicpuThreadHandle, userStream);
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

static HcclResult LaunchP2pExec(HcclComm comm, aclrtStream usrStream, HcclKernelFuncInfo funcInfo, void *funcArgs,
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

    for (HcclComm comm : hcclGroupCommListV2) {
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

HcclResult HcclGroupEndV2()
{
    groupLaunchA5();
    HCCL_INFO("[GroupEnd] to the end");
    return HCCL_SUCCESS;
}