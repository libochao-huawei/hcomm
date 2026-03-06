/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rdma_handle_manager.h"
#include <mutex>

#include "socket_handle_manager.h"
#include "orion_adapter_rts.h"
#include "orion_adapter_hccp.h"
#include "hccp_hdc_manager.h"
#include "null_ptr_exception.h"
#include "network_api_exception.h"
#include "log.h"
#include "tokenInfo_manager.h"

namespace Hccl {

RdmaHandleManager::RdmaHandleManager()
{
    rdmaHandleMap.resize(MAX_DEVICE_NUM);
    for (u32 i = 0; i < rdmaHandleMap.size(); ++i) {
        rdmaHandleMap[i].resize(LINK_PROTO_TYPE_NUM);
    }
}

RdmaHandleManager::~RdmaHandleManager()
{
    DECTOR_TRY_CATCH("RdmaHandleManager", DestroyAll());
}

RdmaHandleManager &RdmaHandleManager::GetInstance()
{
    static RdmaHandleManager rdmaHandleManager;
    return rdmaHandleManager;
}

RdmaHandle RdmaHandleManager::Create(u32 devPhyId, const PortData &localPort)
{
    RaInterface intf{};
    intf.address = localPort.GetAddr();
    intf.phyId   = devPhyId;

    HCCL_INFO("RdmaHandleManager::Create, devPhyId[%u], localPort[%s]", devPhyId, localPort.Describe().c_str());

    HrtNetworkMode netMode = HrtNetworkMode::HDC;
    if (localPort.GetType() == PortDeploymentType::HOST_NET) {
        netMode = HrtNetworkMode::PEER;
    }

    RdmaHandle rdmaHandle = HrtRaRdmaInit(netMode, intf);
    rdmaHandleMap[devPhyId][localPort.GetProto()][localPort.GetAddr()] = rdmaHandle;
    return rdmaHandle;
}

RdmaHandle RdmaHandleManager::Create(u32 devPhyId, const LinkProtoType &localProtocolType,
    const IpAddress &localIp, PortDeploymentType type)
{
    RaInterface intf{};
    intf.address = localIp;
    intf.phyId   = devPhyId;

    HCCL_INFO("RdmaHandleManager::Create, devPhyId[%u], LinkProtoType[%s], localIp[%s]", 
        devPhyId, localProtocolType.Describe().c_str(), localIp.GetIpStr().c_str());

    HrtNetworkMode netMode = HrtNetworkMode::HDC;
    if (type == PortDeploymentType::HOST_NET) {
        netMode = HrtNetworkMode::PEER;
    }
    RdmaHandle rdmaHandle = HrtRaRdmaInit(netMode, intf);
    rdmaHandleMap[devPhyId][localProtocolType][localIp] = rdmaHandle;
    netWorkModeMap[rdmaHandle] = netMode;
    tokenInfoMap[rdmaHandle] = std::make_unique<TokenInfoManager>(devPhyId, rdmaHandle);
    return rdmaHandle;
}

RdmaHandle RdmaHandleManager::Get(u32 devPhyId, const PortData &localPort)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    LinkProtoType localProto = localPort.GetProto();
    if (devPhyId > rdmaHandleMap.size() - 1 || localProto == LinkProtoType::HCCS_PCIE) {
        return nullptr;
    }

    IpAddress  localIp = localPort.GetAddr();
    RdmaHandle res     = rdmaHandleMap[devPhyId][localProto][localIp];
    if (res == nullptr) {
        if (localProto == LinkProtoType::RDMA) {
            res = Create(devPhyId, localPort);
        } else if (localProto == LinkProtoType::UB) {
            HrtRaUbCtxInitParam in(HrtNetworkMode::HDC, devPhyId, localIp);
            res                                          = HrtRaUbCtxInit(in);
            rdmaHandleMap[devPhyId][localProto][localIp] = res;
            tokenInfoMap[res] = std::make_unique<TokenInfoManager>(devPhyId, res);
            HCCL_INFO("Create one rdmahandle [%p], devPhyId [%u], portAddr [%s]",
                res, devPhyId, localPort.GetAddr().Describe().c_str());
        }
    }

    HCCL_INFO("[RdmaHandleManager::Get] one rdmahandle [%p], devPhyId [%u], portAddr [%s]",
                res, devPhyId, localPort.GetAddr().Describe().c_str());
    return res;
}

RdmaHandle RdmaHandleManager::GetByAddr(u32 devPhyId, const LinkProtoType &localProtocolType,
    IpAddress &localIp, PortDeploymentType type)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    if (devPhyId > rdmaHandleMap.size() - 1 || localProtocolType == LinkProtoType::HCCS_PCIE) {
        return nullptr;
    }

    RdmaHandle res     = rdmaHandleMap[devPhyId][localProtocolType][localIp];
    if (res == nullptr) {
        if (localProtocolType == LinkProtoType::RDMA) {
            res = Create(devPhyId, localProtocolType, localIp, type);
        } else if (localProtocolType == LinkProtoType::UB) {
            HrtRaUbCtxInitParam in(HrtNetworkMode::HDC, devPhyId, localIp);
            res                                          = HrtRaUbCtxInit(in);
            rdmaHandleMap[devPhyId][localProtocolType][localIp] = res;
            tokenInfoMap[res] = std::make_unique<TokenInfoManager>(devPhyId, res);
            HCCL_INFO("Create one rdmahandle [%p], devPhyId [%u], portAddr [%s]",
                res, devPhyId, localIp.Describe().c_str());
        }
    }
    HCCL_INFO("[RdmaHandleManager::GetByAddr] one rdmahandle [%p], devPhyId [%u], portAddr [%s]",
        res, devPhyId, localIp.Describe().c_str());
    return res;
}

RdmaHandle RdmaHandleManager::GetByIp(u32 devPhyId, const IpAddress &localIp)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    if (devPhyId > rdmaHandleMap.size() - 1) {
        HCCL_ERROR("[RdmaHandleManager][GetByIp]devPhyId[%u] is invalid, "
            "should be less than [%zu]", devPhyId, rdmaHandleMap.size());
        return nullptr;
    }

    // only support UB type
    RdmaHandle res = rdmaHandleMap[devPhyId][LinkProtoType::UB][localIp];
    if (res == nullptr) {
        HrtRaUbCtxInitParam in(HrtNetworkMode::HDC, devPhyId, localIp);
        res = HrtRaUbCtxInit(in);
        rdmaHandleMap[devPhyId][LinkProtoType::UB][localIp] = res;
        tokenInfoMap[res] = std::make_unique<TokenInfoManager>(devPhyId, res);
        HCCL_INFO("Create one rdmahandle [%p], devPhyId [%u], ipAddr [%s]",
            res, devPhyId, localIp.Describe().c_str());
    }

    return res;
}

JfcHandle RdmaHandleManager::GetJfcHandle(RdmaHandle rdmaHandle, HrtUbJfcMode jfcMode)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    if (rdmaHandle == nullptr) {
        THROW<InvalidParamsException>("[RdmaHandleManager][GetJfcHandle]rdmaHandle is nullptr, please check input.");
    }

    if (jfcMode != HrtUbJfcMode::STARS_POLL && jfcMode != HrtUbJfcMode::CCU_POLL) {
        THROW<InvalidParamsException>("[RdmaHandleManager][GetJfcHandle]jfcMode[%s] is not STARS_POLL or CCU_POLL, "
            "please check input.", jfcMode.Describe().c_str());
    }

    if (jfcHandleMap.find(rdmaHandle) != jfcHandleMap.end() && 
        jfcHandleMap[rdmaHandle].find(jfcMode) != jfcHandleMap[rdmaHandle].end()) {
        return jfcHandleMap[rdmaHandle][jfcMode];
    }

    jfcHandleMap[rdmaHandle][jfcMode] = HrtRaUbCreateJfc(rdmaHandle, jfcMode);
    return jfcHandleMap[rdmaHandle][jfcMode];
}

JfcHandle  RdmaHandleManager::GetJfcHandleAndCqInfo(RdmaHandle rdmaHandle, CqCreateInfo& cqInfo, HrtUbJfcMode jfcMode)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    if (rdmaHandle == nullptr) {
        THROW<InvalidParamsException>("[RdmaHandleManager][GetJfcHandleAndCqInfo]rdmaHandle is nullptr, please check input.");
    }

    if (jfcMode != HrtUbJfcMode::USER_CTL) {
    THROW<InvalidParamsException>("[RdmaHandleManager][GetJfcHandleAndCqInfo]jfcMode[%s] is not USER_CTL, "
            "please check input.", jfcMode.Describe().c_str());
    }

    if (jfcHandleMap.find(rdmaHandle) != jfcHandleMap.end() && 
        jfcHandleMap[rdmaHandle].find(jfcMode) != jfcHandleMap[rdmaHandle].end()) {
        return jfcHandleMap[rdmaHandle][jfcMode];
    }
    
    jfcHandleMap[rdmaHandle][jfcMode] = HrtRaUbCreateJfcUserCtl(rdmaHandle, cqInfo);
    return jfcHandleMap[rdmaHandle][jfcMode];
}

std::pair<uint32_t, uint32_t> RdmaHandleManager::GetDieAndFuncId(RdmaHandle rdmaHandle)
{
    std::lock_guard<std::mutex> lock(managerMutex);
    
    if (rdmaHandle == nullptr) {
        THROW<InvalidParamsException>("[RdmaHandleManager][GetDieAndFuncId]rdmaHandle is nullptr, please check input.");
    }

    if (DieAndFuncIdMap.find(rdmaHandle) != DieAndFuncIdMap.end()) {
        return DieAndFuncIdMap[rdmaHandle];
    }

    DieAndFuncIdMap[rdmaHandle] = HraGetDieAndFuncId(rdmaHandle);
    return DieAndFuncIdMap[rdmaHandle];
}

bool RdmaHandleManager::GetRtpEnable(RdmaHandle rdmaHandle)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    if (rdmaHandle == nullptr) {
        THROW<InvalidParamsException>("[RdmaHandleManager][GetRtpEnable]rdmaHandle is nullptr, please check input.");
    }

    if (RtpEnableMap.find(rdmaHandle) != RtpEnableMap.end()) {
        return RtpEnableMap[rdmaHandle];
    }

    RtpEnableMap[rdmaHandle] = HraGetRtpEnable(rdmaHandle);
    HCCL_RUN_INFO("[%s] GetRtpEnable return[%d]", __func__, RtpEnableMap[rdmaHandle]);
    return RtpEnableMap[rdmaHandle];
}

std::pair<TokenIdHandle, uint32_t> RdmaHandleManager::GetTokenIdInfo(RdmaHandle rdmaHandle, const BufferKey<uintptr_t, u64> &bufKey)
{
    std::lock_guard<std::mutex> lock(managerMutex);

    if (rdmaHandle == nullptr) {
        THROW<InvalidParamsException>("[RdmaHandleManager::%s]rdmaHandle is nullptr, please check input.", __func__);
    }

    if (tokenInfoMap.find(rdmaHandle) == tokenInfoMap.end()) {
        THROW<InvalidParamsException>("[RdmaHandleManager::%s]tokenInfoManager is nullptr, please check.", __func__);
    }
    
    std::pair<TokenIdHandle, uint32_t> tokenInfo = tokenInfoMap[rdmaHandle]->GetTokenInfo(bufKey);
 
    HCCL_INFO("[RdmaHandleManager::%s] Addr[%llu] Size[%llu] rdmahandle[%p]", __func__, bufKey.Addr(), bufKey.Size(), rdmaHandle);
    return tokenInfo;
}

constexpr u32 UB_HANDLE_INDEX   = 3;
constexpr u32 RDMA_HANDLE_INDEX = 2;

void RdmaHandleManager::DestroyAll()
{
    for (auto &handleIter : jfcHandleMap) {
        for (auto &modeIter : handleIter.second) {
            DECTOR_TRY_CATCH("jfc handle destroy", HrtRaUbDestroyJfc(handleIter.first, modeIter.second));
        }
    }

    for (u32 i = 0; i < rdmaHandleMap.size(); ++i) {
        for (u32 j = 0; j < rdmaHandleMap[i].size(); ++j) {
            for (auto &handleIter : rdmaHandleMap[i][j]) {
                if (j == RDMA_HANDLE_INDEX && handleIter.second != nullptr) {
                    DECTOR_TRY_CATCH("rdma handle deinit", HrtRaRdmaDeInit(handleIter.second, netWorkModeMap[handleIter.second]));
                }
                if (j == UB_HANDLE_INDEX && handleIter.second != nullptr) {
                    if (tokenInfoMap[handleIter.second] != nullptr) {
                        DECTOR_TRY_CATCH("token id handle destroy", tokenInfoMap[handleIter.second]->Destroy());
                    }
                    DECTOR_TRY_CATCH("ub handle destroy", HrtRaUbCtxDestroy(handleIter.second));
                }
            }
            rdmaHandleMap[i][j].clear();
        }
    }

    rdmaHandleMap.clear();
    DieAndFuncIdMap.clear();
    RtpEnableMap.clear();
    jfcHandleMap.clear();
    netWorkModeMap.clear();
}

} // namespace Hccl
