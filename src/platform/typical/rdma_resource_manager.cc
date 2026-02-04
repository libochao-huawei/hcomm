/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rdma_resource_manager.h"
#include "dltdt_function.h"
#include "adapter_hccp.h"
#include "adapter_tdt.h"
#include "externalinput_pub.h"
#include "adapter_hccp_common.h"
#include "adapter_rts_common.h"
#include "network_manager_pub.h"

namespace hccl{
constexpr u32 RETRY_CQE_ARRAY_SIZE = 128; // 重执行时获取的CQE数组的最大数量，最大128
RdmaResourceManager::RdmaResourceManager() : nicDeploy_(NICDeployment::NIC_DEPLOYMENT_DEVICE)
{
}

RdmaResourceManager::~RdmaResourceManager()
{
    DeInit();
}

RdmaResourceManager& RdmaResourceManager::GetInstance() 
{
    static RdmaResourceManager rdmaInstance[MAX_MODULE_DEVICE_NUM + 1];
    s32 deviceLogicId = INVALID_INT;
    HcclResult ret = hrtGetDevice(&deviceLogicId);
    if (ret == HCCL_SUCCESS && (static_cast<u32>(deviceLogicId) < MAX_MODULE_DEVICE_NUM)) {
        HCCL_INFO("[RdmaResourceManager::GetInstance]deviceLogicID[%d]", deviceLogicId);
        return rdmaInstance[deviceLogicId];
    }
    HCCL_WARNING("[RdmaResourceManager::GetInstance]deviceLogicID[%d] is invalid, ret[%d]", deviceLogicId, ret);
    return rdmaInstance[MAX_MODULE_DEVICE_NUM];
}

HcclResult RdmaResourceManager::Init()
{
    HcclResult ret = InitExternalInput();
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("[RdmaResourceManager][Init]errNo[0x%016llx] init external input error.",
        HCCL_ERROR_CODE(ret)), HCCL_E_PARA);
    HCCL_INFO("[RdmaResourceManager][Init] Init ExternalInput success!");

    CHK_RET(hrtGetDevice(&deviceLogicId_));
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(deviceLogicId_), devicePhyId_, true));
    CHK_RET(NetworkManager::GetInstance(deviceLogicId_).Init(nicDeploy_, false, devicePhyId_, true));
    HCCL_DEBUG("[RdmaResourceManager][Init] NetworkManager Init, deviceLogicId[%d], devicePhyId[%u], nicDeployment_[%d]",
        deviceLogicId_, devicePhyId_, nicDeploy_);
        
    std::vector<HcclIpAddress> deviceIPs;
    CHK_RET(hrtRaGetDeviceIP(devicePhyId_, deviceIPs));
    CHK_PRT_RET(deviceIPs.size() < 1,
        HCCL_ERROR("[RdmaResourceManager][Init] Get ip address failed, deviceLogicId[%d], devicePhyId[%u]",
        deviceLogicId_, devicePhyId_), HCCL_E_INTERNAL);

    for (u32 ipIdex = 0; ipIdex < deviceIPs.size(); ipIdex++) {
        if (deviceIPs[ipIdex].IsInvalid()) {
            continue;
        }
        // port传入的值为无效值0xFFFFFFFF, 不启动监听
        ret = NetworkManager::GetInstance(deviceLogicId_).StartNic(deviceIPs[ipIdex], port_, true);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[RdmaResourceManager][Init] Start nic ipaddr[%s] failed", deviceIPs[ipIdex].GetReadableAddress()),
            ret);
        ipAddr_ = deviceIPs[ipIdex];
        break;
    }
    if (ipAddr_.IsInvalid()) {
        HCCL_ERROR("[RdmaResourceManager][Init] No valid ipAddr, devicePhyId is [%u].", devicePhyId_);
        return HCCL_E_NOT_FOUND;
    }
    CHK_RET(NetworkManager::GetInstance(deviceLogicId_).GetRdmaHandleByIpAddr(ipAddr_, rdmaHandle_));
    CHK_PTR_NULL(rdmaHandle_);
    HCCL_INFO("[RdmaResourceManager][Init] Start nic, devicePhyId is [%u], ip address[%s], port[%u].",
        devicePhyId_, this->ipAddr_.GetReadableAddress(), this->port_);

    CHK_RET(HrtRaGetNotifyBaseAddr(rdmaHandle_, &notifyBaseVa_, &notifyTotalSize_));
    HCCL_INFO("[RdmaResourceManager][Init] NotifyBaseAddr addr[%llu] size[%llu].", notifyBaseVa_,
        notifyTotalSize_);
 
    notifyMrInfo_.addr = reinterpret_cast<void*>(notifyBaseVa_);
    notifyMrInfo_.size = notifyTotalSize_;
    notifyMrInfo_.access = RA_ACCESS_REMOTE_WRITE | RA_ACCESS_LOCAL_WRITE | RA_ACCESS_REMOTE_READ;
    CHK_RET(hrtRaRegGlobalMr(rdmaHandle_, notifyMrInfo_, mrHandle_));
    HCCL_RUN_INFO("[RdmaResourceManager][Init] Register NotifyBaseAddr addr[%p] size[%llu] key[%u]", notifyMrInfo_.addr,
        notifyMrInfo_.size, notifyMrInfo_.lkey);
    return HCCL_SUCCESS;
}

HcclResult RdmaResourceManager::GetRdmaHandle(RdmaHandle& rdmaHandle)
{
    CHK_PTR_NULL(rdmaHandle_);
    rdmaHandle = rdmaHandle_;
    return HCCL_SUCCESS;
}

HcclResult RdmaResourceManager::DeInit()
{
    if (rdmaHandle_ == nullptr) {
        HCCL_INFO("[RdmaResourceManager][DeInit] RDMA resource has been already deinited!");
        return HCCL_SUCCESS;
    }
    CHK_PTR_NULL(mrHandle_);
    CHK_RET(hrtRaDeRegGlobalMr(rdmaHandle_, mrHandle_));
    CHK_RET(NetworkManager::GetInstance(deviceLogicId_).StopNic(ipAddr_, port_));
    rdmaHandle_ = nullptr;
    HCCL_INFO("[RdmaResourceManager][DeInit] Stop nic, devicePhyId is [%u], ip address[%s], port[%u].",
        devicePhyId_, ipAddr_.GetReadableAddress(), port_);
    CHK_RET(NetworkManager::GetInstance(deviceLogicId_).DeInit(nicDeploy_));
    return HCCL_SUCCESS;
}

HcclResult RdmaResourceManager::GetCqeErrInfo(struct CqeErrInfo *infoList, u32 *num)
{
    CHK_PTR_NULL(rdmaHandle_);
    return hrtRaGetCqeErrInfoList(rdmaHandle_, infoList, num);
}

HcclResult RdmaResourceManager::GetCqeErrInfoByQpn(u32 qpn, struct HcclErrCqeInfo *errCqeList, u32 *num)
{
    CHK_PTR_NULL(rdmaHandle_);
    u32 qpnNum = RETRY_CQE_ARRAY_SIZE;
    u32 listLen = *num;
    std::unique_lock<std::mutex> lock(cqeErrMapMutex_);
    struct CqeErrInfo cqeErrInfolist[qpnNum] = {};
    CHK_RET(hrtRaGetCqeErrInfoList(rdmaHandle_, cqeErrInfolist, &qpnNum));
    for (u32 i = 0; i < qpnNum; i++) {
        u32 errQpn = cqeErrInfolist[i].qpn;
        cqeErrPerQP_[errQpn].push(cqeErrInfolist[i]);
    }
    if (cqeErrPerQP_.empty() || cqeErrPerQP_.find(qpn) == cqeErrPerQP_.end()) {
        *num = 0;
    } else {
        *num = cqeErrPerQP_[qpn].size();
        if (*num > listLen) {
            *num = listLen;
            HCCL_WARNING("[GetCqeErrInfoByQpn] GetCqeErrInfo num is larger than infoList user given.");
        } 
        u32 i = 0;
        while (!cqeErrPerQP_[qpn].empty() && i < listLen) {
            errCqeList[i].qpn = cqeErrPerQP_[qpn].front().qpn;
            errCqeList[i].status = cqeErrPerQP_[qpn].front().status;
            errCqeList[i].time = cqeErrPerQP_[qpn].front().time;
            time_t tmpt = static_cast<time_t>(errCqeList[i].time.tv_sec);
            struct tm errTime;
            localtime_r(&tmpt, &errTime);
            HCCL_INFO("[GetCqeErrInfoByQpn] Err Cqe status[%d], qpn[%d], time[%04u-%02d-%02d %02d:%0d:%02d.%06u]", 
                errCqeList[i].status, errCqeList[i].qpn, errTime.tm_year + TIME_FROM_1900,
                errTime.tm_mon + 1,
                errTime.tm_mday,
                errTime.tm_hour,
                errTime.tm_min,
                errTime.tm_sec,
                static_cast<u32>(errCqeList[i].time.tv_usec));
            cqeErrPerQP_[qpn].pop();    
            i++;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RdmaResourceManager::GetNotifyMrInfo(struct MrInfoT& mrInfo)
{
    CHK_PTR_NULL(rdmaHandle_);
    mrInfo.lkey = notifyMrInfo_.lkey;
    HCCL_RUN_INFO("[RdmaResourceManager][GetNotifyMrInfo] SyncMem key[%u].", mrInfo.lkey);
    return HCCL_SUCCESS;
}
}