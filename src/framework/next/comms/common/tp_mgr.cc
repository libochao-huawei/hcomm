/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tp_mgr.h"

#include "hccp_ctx.h"
#include "hccp_async_ctx.h"

#include "hccl_common.h"
#include "exception_handler.h"
#include "rdma_handle_manager.h"
#include "orion_adapter_hccp.h"
#include "env_config/env_config.h"

namespace hcomm {

TpMgr& TpMgr::GetInstance(const uint32_t devicePhyId)
{
    static TpMgr tpMgr[MAX_MODULE_DEVICE_NUM + 1];

    uint32_t devPhyId = devicePhyId;
    if (devPhyId >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[TpMgr][%s] use the backup device, devPhyId[%u] should be "
            "less than %u.", __func__, devPhyId, MAX_MODULE_DEVICE_NUM);
        devPhyId = MAX_MODULE_DEVICE_NUM; // 使用备份设备
    }

    tpMgr[devPhyId].devPhyId_ = devPhyId;

    return tpMgr[devPhyId];
}

static HcclResult CheckRequestResult(RequestHandle &reqHandle)
{
    if (reqHandle == 0) {
        return HcclResult::HCCL_SUCCESS;
    }

    RequestResult result = HccpGetAsyncReqResult(reqHandle);
    if (result == RequestResult::NOT_COMPLETED) {
        // 不提供日志避免刷屏
        return HcclResult::HCCL_E_AGAIN;
    }

    if (result != RequestResult::COMPLETED) {
        HCCL_ERROR("[TpMgr][%s] failed, result[%s] is unexpected.",
            __func__, result.Describe().c_str());
        return HcclResult::HCCL_E_NETWORK;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CheckTpProtocol(const TpProtocol tpProtocol) {
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::RTP) {
        HCCL_ERROR("[TpMgr][%s] failed, tpProtocol[%d] is not supported.",
            __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::GetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const auto &tpProtocol = param.tpProtocol;
    CHK_RET(CheckTpProtocol(tpProtocol));
    if (FindAndGetTpInfo(param, tpInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(GetReqCtxMutex(tpProtocol));

    auto &reqCtxMap = GetReqCtxMap(tpProtocol);
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    auto &locReqCtxMap = reqCtxMap[locAddr];
    auto locReqCtxIter = locReqCtxMap.find(rmtAddr);
    if (locReqCtxIter == locReqCtxMap.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpInfo, param[%s].", __func__,
            param.Describe().c_str());

        RequestCtx &reqCtx = locReqCtxMap[rmtAddr];
        CHK_RET(StartGetTpInfoListRequest(param, reqCtx));
        return HcclResult::HCCL_E_AGAIN; // 首次触发异步接口调用，动作一定未完成
    }

    auto &reqCtx = locReqCtxIter->second;
    auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    RequestCtx completedReqCtx = locReqCtxIter->second; // 深拷贝构造对象，与map解耦
    locReqCtxMap.erase(locReqCtxIter); // 删除已经完成的请求，避免下次申请错误复用
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
}

HcclResult TpMgr::ReleaseTpInfo(const GetTpInfoParam &param, const TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[locAddr];
    auto locInfoIter = locInfoMap.find(rmtAddr);
    if (locInfoIter == locInfoMap.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info is not found, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (tpInfo.tpHandle != locInfoIter->second.tpInfo.tpHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, tp info[%llu] is not expected[%llu].",
            __func__, tpInfo.tpHandle, locInfoIter->second.tpInfo.tpHandle);
        return HcclResult::HCCL_E_PARA;
    }

    if (locInfoIter->second.useCnt > 1) {
        locInfoIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    locInfoMap.erase(locInfoIter);
    // 当前ub在unimport jetty时通过引用计数管理释放tp handle
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpInfo(const GetTpInfoParam &param, TpInfo &tpInfo)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    auto &locInfoMap = infoMap[locAddr];
    auto locInfoIter = locInfoMap.find(rmtAddr);
    if (locInfoIter != locInfoMap.end()) {
        locInfoIter->second.useCnt += 1;
        tpInfo = locInfoIter->second.tpInfo;
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_E_NOT_FOUND;
}

static HcclResult GetTpInfoAsync(const CtxHandle ctxHandle, const GetTpInfoParam &param,
    std::vector<char> &out, uint32_t &num, RequestHandle &reqHandle)
{
    Hccl::IpAddress locAddr{}, rmtAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(param.rmtAddr, rmtAddr));
    const auto &tpProtocol = param.tpProtocol;

    struct GetTpCfg cfg{};
    cfg.flag.bs.rtp = tpProtocol == TpProtocol::RTP ? 1 : 0;
    cfg.flag.bs.ctp = tpProtocol == TpProtocol::CTP ? 1 : 0;
    cfg.transMode = TransportModeT::CONN_RM; // 当前只使用RM Jetty
    CHK_RET(IpAddressToHccpEid(locAddr, cfg.localEid)); // 当前复用orion ip address
    HCCL_INFO("RaUbGetTpInfoAsync cfg.local_eid[subnetPrefix[%016llx], interfaceId[%016llx]]",
              cfg.localEid.in6.subnetPrefix, cfg.localEid.in6.interfaceId);
    CHK_RET(IpAddressToHccpEid(rmtAddr, cfg.peerEid));
    HCCL_INFO("RaUbGetTpInfoAsync cfg.peer_eid[subnetPrefix[%016llx], interfaceId[%016llx]]",
              cfg.peerEid.in6.subnetPrefix, cfg.peerEid.in6.interfaceId);

    out.resize(sizeof(HccpTpInfo));
    struct HccpTpInfo *info = reinterpret_cast<struct HccpTpInfo *>(out.data());

    void *raReqHandle = nullptr;
    constexpr uint32_t TP_HANDLE_REQUEST_NUM = 1;
    num = TP_HANDLE_REQUEST_NUM; // 指定需要从管控面申请tp handle的数量, hccp 会返回实际个数
    s32 ret = RaGetTpInfoListAsync(ctxHandle, &cfg, info, &num, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[%s] failed, call interface error[%d] raReqHandle[%p], "
            "ctxHandle[%p] locAddr[%s] rmtAddr[%s].", __func__, ret, raReqHandle, ctxHandle,
            locAddr.Describe().c_str(), rmtAddr.Describe().c_str());
        return HcclResult::HCCL_E_NETWORK;
    }

    reqHandle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[%s] get request handle[%llu].", __func__, reqHandle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::StartGetTpInfoListRequest(const GetTpInfoParam &param,
    TpMgr::RequestCtx &reqCtx) const
{
    EXCEPTION_HANDLE_BEGIN
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(param.locAddr, ipAddr));
    const CtxHandle ctxHandle = static_cast<CtxHandle>(
        Hccl::RdmaHandleManager::GetInstance().GetByIp(devPhyId_, ipAddr));
    CHK_PTR_NULL(ctxHandle);

    CHK_RET(GetTpInfoAsync(ctxHandle, param, reqCtx.dataBuffer,
        reqCtx.tpInfoNum, reqCtx.handle));
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

inline TpInfo ParseTpInfo(const struct HccpTpInfo *infoPtr)
{
    TpInfo tpInfo;
    tpInfo.tpHandle = infoPtr->tpHandle;

    return tpInfo;
}

HcclResult TpMgr::HandleCompletedRequest(const TpMgr::RequestCtx reqCtx,
    const GetTpInfoParam &param, TpInfo &tpInfo)
{
    const uint32_t tpInfoNum = reqCtx.tpInfoNum;
    if (tpInfoNum == 0) {
        HCCL_WARNING("[TpMgr][%s] failed to find tp info, tpInfoNum is 0, "
            "param[%s].", __func__, param.Describe().c_str());
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    const struct HccpTpInfo *baseInfoPtr = // 类的私有变量vector指向的堆内存，不会为空
        reinterpret_cast<const struct HccpTpInfo *>(reqCtx.dataBuffer.data());

    TpInfo tmpTpInfo = ParseTpInfo(baseInfoPtr); // 封装接口只会申请1个tpHandle

    Hccl::IpAddress locAddr{}, rmtAddr{};
    (void)CommAddrToIpAddress(param.locAddr, locAddr);
    (void)CommAddrToIpAddress(param.rmtAddr, rmtAddr);

    std::lock_guard<std::mutex> lock(GetInfoCtxMutex(param.tpProtocol));
    auto &infoMap = GetInfoCtxMap(param.tpProtocol);
    infoMap[locAddr][rmtAddr] = {std::move(tmpTpInfo), 1};
    
    tpInfo = infoMap[locAddr][rmtAddr].tpInfo;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::FindAndGetTpAttr(const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex_);
    auto attrIter = tpAttrCtxMap_.find(tpHandle);
    if (attrIter != tpAttrCtxMap_.end()) {
        attrIter->second.useCnt += 1;
        tpAttrInfo = attrIter->second.tpAttrInfo;
        return HcclResult::HCCL_SUCCESS;
    }

    return HcclResult::HCCL_E_NOT_FOUND;
}

HcclResult TpMgr::GetTpAttr(const GetTpAttrParam &param, TpAttrInfo &tpAttrInfo, CtxHandle ctxHandle)
{
    const TpHandle tpHandle = param.tpHandle;
    if (FindAndGetTpAttr(tpHandle, tpAttrInfo) == HcclResult::HCCL_SUCCESS) {
        return HcclResult::HCCL_SUCCESS;
    }

    std::unique_lock<std::mutex> reqCtxLock(tpAttrReqMutex_);
    auto reqCtxIter = tpAttrReqCtxMap_.find(tpHandle);
    if (reqCtxIter == tpAttrReqCtxMap_.end()) {
        HCCL_INFO("[TpMgr][%s] get new tpAttr, param[%s].", __func__,
            param.Describe().c_str());

        TpAttrRequestCtx &reqCtx = tpAttrReqCtxMap_[tpHandle];
        CHK_RET(StartGetTpAttrRequest(param, reqCtx, ctxHandle));
        return HcclResult::HCCL_E_AGAIN;
    }

    auto &reqCtx = reqCtxIter->second;
    auto ret = CheckRequestResult(reqCtx.handle);
    if (ret == HcclResult::HCCL_E_AGAIN) {
        return ret;
    }
    CHK_RET(ret);

    TpAttrRequestCtx completedReqCtx = reqCtxIter->second;
    tpAttrReqCtxMap_.erase(reqCtxIter);
    reqCtxLock.unlock();

    return HandleCompletedTpAttrRequest(std::move(completedReqCtx), tpHandle, tpAttrInfo);
}

HcclResult TpMgr::StartGetTpAttrRequest(const GetTpAttrParam &param,
    TpMgr::TpAttrRequestCtx &reqCtx, CtxHandle ctxHandle) const
{
    void *raReqHandle = nullptr;
    s32 ret = RaGetTpAttrAsync(ctxHandle, param.tpHandle, 
        const_cast<uint32_t*>(&param.attrBitmap), &reqCtx.tpAttr, &raReqHandle);
    if (ret != 0 || !raReqHandle) {
        HCCL_ERROR("[TpMgr][%s] failed, call RaGetTpAttrAsync error[%d] raReqHandle[%p], "
            "tpHandle[0x%llx] attrBitmap[0x%x].", __func__, ret, raReqHandle,
            param.tpHandle, param.attrBitmap);
        return HcclResult::HCCL_E_NETWORK;
    }

    reqCtx.handle = reinterpret_cast<RequestHandle>(raReqHandle);
    HCCL_INFO("[TpMgr][%s] success, tpHandle[0x%llx] reqHandle[%llu].",
        __func__, param.tpHandle, reqCtx.handle);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::HandleCompletedTpAttrRequest(const TpMgr::TpAttrRequestCtx reqCtx,
    const TpHandle tpHandle, TpAttrInfo &tpAttrInfo)
{
    TpAttrInfo tmpTpAttrInfo(reqCtx.tpAttr);

    std::lock_guard<std::mutex> lock(tpAttrCtxMutex_);
    tpAttrCtxMap_[tpHandle] = {std::move(tmpTpAttrInfo), 1};
    
    tpAttrInfo = tpAttrCtxMap_[tpHandle].tpAttrInfo;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::ReleaseTpAttr(const TpHandle tpHandle, const TpAttrInfo &tpAttrInfo)
{
    std::lock_guard<std::mutex> lock(tpAttrCtxMutex_);
    auto attrIter = tpAttrCtxMap_.find(tpHandle);
    if (attrIter == tpAttrCtxMap_.end()) {
        HCCL_ERROR("[TpMgr][%s] failed, tp attr is not found, "
            "tpHandle[0x%llx].", __func__, tpHandle);
        return HcclResult::HCCL_E_NOT_FOUND;
    }

    if (attrIter->second.useCnt > 1) {
        attrIter->second.useCnt -= 1;
        return HcclResult::HCCL_SUCCESS;
    }

    tpAttrCtxMap_.erase(attrIter);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpMgr::GetTpTotalTimeout(const TpAttrInfo &tpAttrInfo, uint32_t &tpTimeOutMs)
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

static uint32_t TaHwValueToMs(uint8_t hwValue)
{
    uint8_t gear = hwValue / 8;
    switch (gear) {
        case 0: return TA_TIMEOUT_MS_GEAR0;
        case 1: return TA_TIMEOUT_MS_GEAR1;
        case 2: return TA_TIMEOUT_MS_GEAR2;
        case 3: return TA_TIMEOUT_MS_GEAR3;
        default: return TA_TIMEOUT_MS_GEAR2;
    }
}

static uint8_t FindMinTaHwValue(uint32_t tpTotalTimeoutMs)
{
    if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR0) {
        return TA_HW_GEAR0_BASE;
    } else if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR1) {
        return TA_HW_GEAR1_BASE;
    } else if (tpTotalTimeoutMs < TA_TIMEOUT_MS_GEAR2) {
        return TA_HW_GEAR2_BASE;
    } else {
        return TA_HW_GEAR3_BASE;
    }
}

uint8_t TpMgr::CalcTaTimeout(const TpAttrInfo &tpAttrInfo)
{
    constexpr uint8_t UB_TIMEOUT_DEFAULT = 8; // 默认 UBC_CTP 和 UBC_TP 超时配置为8
    uint8_t envValue = static_cast<uint8_t>(Hccl::EnvConfig::GetInstance().GetRdmaConfig().GetUbTimeOut());
    uint32_t envTimeoutMs = TaHwValueToMs(envValue);
    
    uint32_t tpTimeOutMs = 0;
    (void)GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs);
    
    uint8_t errTimeout = UB_TIMEOUT_DEFAULT;
    if (envTimeoutMs < tpTimeOutMs) {
        errTimeout = FindMinTaHwValue(tpTimeOutMs);
        HCCL_WARNING("[TpMgr][%s] Env timeout [%ums] < TP timeout [%ums]. Auto upgrade TA to hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, errTimeout, TaHwValueToMs(errTimeout));
    } else {
        errTimeout = envValue;
        HCCL_INFO("[TpMgr][%s] Env timeout [%ums] >= TP timeout [%ums]. Use env gear base hw_val[%u] (%ums).",
            __func__, envTimeoutMs, tpTimeOutMs, envValue, envTimeoutMs);
    }
    
    return errTimeout;
}

TpMgr::InfoCtxMap& TpMgr::GetInfoCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMap_ : rtpInfoMap_;
}

TpMgr::ReqCtxMap& TpMgr::GetReqCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMap_ : rtpReqMap_;
}

std::mutex& TpMgr::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMutex_ : rtpInfoMutex_;
}

std::mutex& TpMgr::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMutex_ : rtpReqMutex_;
}

} // namespace Hccl
