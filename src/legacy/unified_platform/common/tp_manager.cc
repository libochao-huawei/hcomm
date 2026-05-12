/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tp_manager.h"

#include "exception_util.h"
#include "hccl_common_v2.h"
#include "invalid_params_exception.h"
#include "env_config/env_config.h"

#include "hccp_ctx.h"
#include "orion_adapter_rts.h"
#include "rdma_handle_manager.h"


namespace Hccl {

TpManager& TpManager::GetInstance(const int32_t deviceLogicId)
{
    static TpManager tpManager[MAX_MODULE_DEVICE_NUM + 1];

    if (deviceLogicId < 0 ||
        static_cast<uint32_t>(deviceLogicId) > MAX_MODULE_DEVICE_NUM) {
        THROW<InvalidParamsException>("[TpManager][%s] failed to get instance, "
            "devLogicId[%d] should be less than %u.", __func__,
            deviceLogicId, MAX_MODULE_DEVICE_NUM);
    }

    tpManager[deviceLogicId].devLogicId = deviceLogicId;

    return tpManager[deviceLogicId];
}

void TpManager::Init()
{
    if (initFlag) {
        return;
    }

    devPhyId = HrtGetDevicePhyIdByIndex(devLogicId);
    initFlag = true;
}

bool TpManager::CheckRequestResult(RequestHandle &reqHandle) const
{
    if (reqHandle == 0) {
        return true;
    }

    ReqHandleResult result = HrtRaGetAsyncReqResult(reqHandle);
    if (result == ReqHandleResult::NOT_COMPLETED) {
        return false;
    }

    if (result != ReqHandleResult::COMPLETED) {
        THROW<InternalException>("[TpManager][%s] failed, result[%s] is unexpected.",
            __func__, result.Describe().c_str());
    }

    return true;
}

HcclResult CheckTpProtocol(const TpProtocol tpProtocol) {
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::TP && tpProtocol != TpProtocol::UBOE) {
        HCCL_WARNING("[TpManager][%s] failed, tpProtocol[%d] is not supported.",
            __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo,
    bool isSync)
{
    const auto &tpProtocol = param.tpProtocol;
    CHK_RET(CheckTpProtocol(tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo)) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(tpProtocol));

    auto &reqCtxMap = GetReqCtxMap(tpProtocol);
    const auto &locAddr = param.locAddr;
    const auto &rmtAddr = param.rmtAddr;
    auto &locReqCtxMap = reqCtxMap[locAddr];
    auto locReqCtxIter = locReqCtxMap.find(rmtAddr);
    if (locReqCtxIter == locReqCtxMap.end()) {
        HCCL_INFO("[TpManager][%s] get new tpInfo, param[%s].", __func__,
            param.Describe().c_str());

        RequestCtx &reqCtx = locReqCtxMap[rmtAddr];
        reqCtx.isSync = isSync;
        StartGetTpInfoListRequest(param, reqCtx, isSync);
        return HcclResult::HCCL_E_AGAIN;
    }

    if (!locReqCtxIter->second.isSync) {
        auto &reqCtx = locReqCtxIter->second;
        if (!CheckRequestResult(reqCtx.handle)) {
            return HcclResult::HCCL_E_AGAIN;
        }
    }

    RequestCtx completedReqCtx = locReqCtxIter->second; // 深拷贝构造对象，与map解耦
    locReqCtxMap.erase(locReqCtxIter); // 删除已经完成的请求，避免下次申请错误复用
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpManager::FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
    auto attrIter = tpAttrCtxMap.find(tpHandle);
    if (attrIter != tpAttrCtxMap.end()) {
        attrIter->second.useCnt += 1;
        tpAttrInfo = attrIter->second.tpAttrInfo;
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_E_NOT_FOUND;
}

HcclResult TpManager::GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, RdmaHandle rdmaHandle)
{
    const TpHandle tpHandle = param.tpHandle;
    if (FindAndGetTpAttr(tpHandle, tpAttrInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(tpAttrReqMutex);
    auto reqCtxIter = tpAttrReqCtxMap.find(tpHandle);
    if (reqCtxIter == tpAttrReqCtxMap.end()) {
        HCCL_INFO("[TpManager][%s] get new tpAttr, param[%s].", __func__,
            param.Describe().c_str());

        TpAttrRequestCtx &reqCtx = tpAttrReqCtxMap[tpHandle];
        CHK_RET(StartGetTpAttrRequest(param, reqCtx, rdmaHandle));
        return HcclResult::HCCL_E_AGAIN;
    }

    auto &reqCtx = reqCtxIter->second;
    if (!isHost) {
        if (!CheckRequestResult(reqCtx.handle)) {
            return HcclResult::HCCL_E_AGAIN;
        }
    }

    TpAttrRequestCtx completedReqCtx = reqCtxIter->second;
    tpAttrReqCtxMap.erase(reqCtxIter);
    reqCtxLock.unlock();

    return HandleCompletedTpAttrRequest(std::move(completedReqCtx), tpHandle, tpAttrInfo);
}

HcclResult TpManager::StartGetTpAttrRequest(const GetTpAttrParam &param,
    TpManager::TpAttrRequestCtx &reqCtx, RdmaHandle rdmaHandle) const
{
    if (isHost) {
        struct TpAttr tpAttr = {0};
        auto ret = RaGetTpAttr(rdmaHandle, param.tpHandle, param.attrBitmap, tpAttr);
        if (ret != 0) {
            HCCL_ERROR("[TpManager][%s] failed, call RaGetTpAttr error[%d], "
                "tpHandle[0x%llx] attrBitmap[0x%x].", __func__, ret,
                param.tpHandle, param.attrBitmap);
            return HcclResult::HCCL_E_NETWORK;
        }
        reqCtx.tpAttr = tpAttr;
        reqCtx.handle = 0;
        return HcclResult::HCCL_SUCCESS;
    }

    void *raReqHandle = nullptr;
    s32 ret = RaGetTpAttrAsync(rdmaHandle, param.tpHandle,
        const_cast<uint32_t*>(&param.attrBitmap), &reqCtx.tpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[TpManager][%s] failed, call RaGetTpAttrAsync error[%d] raReqHandle[%p], "
            "tpHandle[0x%llx] attrBitmap[0x%x].", __func__, ret, raReqHandle,
            param.tpHandle, param.attrBitmap);
        return HcclResult::HCCL_E_NETWORK;
    }

    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[TpManager][%s] success, tpHandle[0x%llx] reqHandle[%llu].",
        __func__, param.tpHandle, reqCtx.handle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::HandleCompletedTpAttrRequest(const TpManager::TpAttrRequestCtx reqCtx,
    const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    TpAttrInfo tmpTpAttrInfo(reqCtx.tpAttr);

    std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
    tpAttrCtxMap[tpHandle] = {std::move(tmpTpAttrInfo), 1};
    
    tpAttrInfo = tpAttrCtxMap[tpHandle].tpAttrInfo;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::ReleaseTpInfo(const RaUbGetTpInfoParam &param, const TpInfo &tpInfo)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[param.locAddr];
    auto locInfoIter = locInfoMap.find(param.rmtAddr);
    if (locInfoIter == locInfoMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != locInfoIter->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpManager][%s] failed, tp info[%llu] is not expected[%llu].",
            __func__, tpInfo.tpHandle, locInfoIter->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (locInfoIter->second.useCnt > 1) {
        locInfoIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    locInfoMap.erase(locInfoIter);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::ReleaseTpAttr(const TpHandle tpHandle, const TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex);
    auto attrIter = tpAttrCtxMap.find(tpHandle);
    if (attrIter == tpAttrCtxMap.end()) {
        HCCL_ERROR("[TpManager][%s] failed, tp attr is not found, "
            "tpHandle[0x%llx].", __func__, tpHandle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (attrIter->second.useCnt > 1) {
        attrIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    tpAttrCtxMap.erase(attrIter);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::GetTpTotalTimeout(const TpAttrInfo &tpAttrInfo, uint32_t &tpTimeOutMs)
{
    uint8_t rawAtGear = tpAttrInfo.tpAttr.at;
    uint8_t rawRetryTimes = tpAttrInfo.tpAttr.retryTimesInit;

    uint8_t finalAtGear = rawAtGear;
    if (rawAtGear < AT_GEAR_MIN || rawAtGear > AT_GEAR_MAX) {
        finalAtGear = AT_GEAR_DEFAULT;
        HCCL_WARNING("%s Invalid at gear[%u], expect [%u, %u], use default gear[%u].",
            __func__, rawAtGear, AT_GEAR_MIN, AT_GEAR_MAX, finalAtGear);
    }

    uint32_t singleAtTimeoutMs = AT_TIMEOUT_MAP[finalAtGear];
    tpTimeOutMs = singleAtTimeoutMs * static_cast<uint32_t>(rawRetryTimes + 1);

    HCCL_INFO("%s TP timeout calc success: raw_at_gear[%u], final_at_gear[%u], "
        "single_timeout[%ums], retry_times[%u], total_timeout[%ums].",
        __func__, rawAtGear, finalAtGear, singleAtTimeoutMs, rawRetryTimes, tpTimeOutMs);

    return HcclResult::HCCL_SUCCESS;
}

uint8_t TpManager::CalcTaTimeout(const TpAttrInfo &tpAttrInfo)
{
    constexpr uint8_t UB_TIMEOUT_DEFAULT = 8;
    uint8_t envValue = static_cast<uint8_t>(EnvConfig::GetInstance().GetRdmaConfig().GetUbTimeOut());
    uint32_t envTimeoutMs = TaHwValueToMs(envValue);
    
    uint32_t tpTimeOutMs = 0;
    (void)GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs);
    
    uint8_t errTimeout = UB_TIMEOUT_DEFAULT;
    if (envTimeoutMs < tpTimeOutMs) {
        errTimeout = FindMinTaHwValue(tpTimeOutMs);
        HCCL_WARNING("[TpManager][%s] Env timeout [%ums] < TP timeout [%ums]. Auto upgrade TA to hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, errTimeout, TaHwValueToMs(errTimeout));
    } else {
        errTimeout = envValue;
        HCCL_INFO("[TpManager][%s] Env timeout [%ums] >= TP timeout [%ums]. Use env gear base hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, envValue, envTimeoutMs);
    }
    
    return errTimeout;
}

bool TpManager::FindAndGetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[param.locAddr];
    auto locInfoIter = locInfoMap.find(param.rmtAddr);
    if (locInfoIter != locInfoMap.end()) {
        locInfoIter->second.useCnt += 1;
        tpInfo = locInfoIter->second.tpInfo;
        return true;
    }

    return false;
}

void TpManager::StartGetTpInfoListRequest(const RaUbGetTpInfoParam &param,
    TpManager::RequestCtx &reqCtx, bool isSync) const
{
    Hccl::IpAddress localIp = param.locAddr;

    // isSync为true走同步路径，false走异步路径。当前仅HostUbConnection使用同步模式。
    RdmaHandle rdmaHandle = isSync 
        ? RdmaHandleManager::GetInstance().GetByAddr(devPhyId, LinkProtoType::UB, 
                                localIp, Hccl::PortDeploymentType::HOST_NET)
        : RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle, "
            "devPhyId[%u] locAddr[%s].", __func__, devPhyId,
            param.locAddr.Describe().c_str());
    }
    if (isSync) {
        RaUbGetTpInfo(rdmaHandle, param, reqCtx.dataBuffer, reqCtx.tpInfoNum);
        return;
    }
    reqCtx.handle = RaUbGetTpInfoAsync(rdmaHandle, param, reqCtx.dataBuffer,
        reqCtx.tpInfoNum);
}

inline TpInfo ParseTpInfo(const struct HccpTpInfo *infoPtr)
{
    TpInfo tpInfo;
    tpInfo.tpHandle = infoPtr->tpHandle;

    return tpInfo;
}

HcclResult TpManager::HandleCompletedRequest(const TpManager::RequestCtx reqCtx,
    const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0) {
        HCCL_WARNING("[TpManager][%s] failed to find tp info, tpInfoNum is 0, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *baseInfoPtr = // 类的私有变量vector指向的堆内存，不会为空
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());

    TpInfo tmpTpInfo = ParseTpInfo(baseInfoPtr); // 封装接口只会申请1个tpHandle

    auto &locAddr = param.locAddr;
    auto &rmtAddr = param.rmtAddr;

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    infoMap[locAddr][rmtAddr] = {std::move(tmpTpInfo), 1};
    
    tpInfo = infoMap[locAddr][rmtAddr].tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

TpManager::InfoCtxMap& TpManager::GetInfoCtxMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpInfoMap;
        case TpProtocol::TP:
            return tpInfoMap;
        case TpProtocol::UBOE:
            return uboeInfoMap;
        default:
            return tpInfoMap;
    }
}

TpManager::ReqCtxMap& TpManager::GetReqCtxMap(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpReqMap;
        case TpProtocol::TP:
            return tpReqMap;
        case TpProtocol::UBOE:
            return uboeReqMap;
        default:
            return tpReqMap;
    }
}

std::mutex& TpManager::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpInfoMutex;
        case TpProtocol::TP:
            return tpInfoMutex;
        case TpProtocol::UBOE:
            return uboeInfoMutex;
        default:
            return tpInfoMutex;
    }
}

std::mutex& TpManager::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    switch (tpProtocol) {
        case TpProtocol::CTP:
            return ctpReqMutex;
        case TpProtocol::TP:
            return tpReqMutex;
        case TpProtocol::UBOE:
            return uboeReqMutex;
        default:
            return tpReqMutex;
    }
}

} // namespace Hccl
