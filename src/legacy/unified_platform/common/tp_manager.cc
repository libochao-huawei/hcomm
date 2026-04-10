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

#include "hccp_ctx.h"
#include "orion_adapter_rts.h"
#include "rdma_handle_manager.h"


namespace Hccl {

TpManager& TpManager::GetInstance(const int32_t deviceLogicId)
{
    static TpManager tpManager[MAX_MODULE_DEVICE_NUM];

    if (deviceLogicId < 0 ||
        static_cast<uint32_t>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM) {
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
    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::TP) {
        HCCL_WARNING("[TpManager][%s] failed, tpProtocol[%d] is not supported.",
            __func__, tpProtocol);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult TpManager::GetTpInfo(const RaUbGetTpInfoParam &param, TpInfo &tpInfo)
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
        StartGetTpInfoListRequest(param, reqCtx);
        return HcclResult::HCCL_E_AGAIN;
    }

    auto &reqCtx = locReqCtxIter->second;
    if (!CheckRequestResult(reqCtx.handle)) {
        return HcclResult::HCCL_E_AGAIN;
    }

    RequestCtx completedReqCtx = locReqCtxIter->second; // 深拷贝构造对象，与map解耦
    locReqCtxMap.erase(locReqCtxIter); // 删除已经完成的请求，避免下次申请错误复用
    reqCtxLock.unlock();

    return HandleCompletedRequest(std::move(completedReqCtx), param, tpInfo);
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
    // 暂时不能主动释放tp handle，跟随unimport jetty释放
    return HcclResult::HCCL_SUCCESS;
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
    TpManager::RequestCtx &reqCtx) const
{
    RdmaHandle rdmaHandle =
        RdmaHandleManager::GetInstance().GetByIp(devPhyId, param.locAddr);
    if (!rdmaHandle) {
        THROW<InternalException>("[TpManager][%s] can not find rdmaHandle, "
            "devPhyId[%u] locAddr[%s].", __func__, devPhyId,
            param.locAddr.Describe().c_str());
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
    return tpProtocol == TpProtocol::CTP ? ctpInfoMap : tpInfoMap;
}

TpManager::ReqCtxMap& TpManager::GetReqCtxMap(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMap : tpReqMap;
}

std::mutex& TpManager::GetInfoCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpInfoMutex : tpInfoMutex;
}

std::mutex& TpManager::GetReqCtxMutex(const TpProtocol tpProtocol)
{
    return tpProtocol == TpProtocol::CTP ? ctpReqMutex : tpReqMutex;
}

} // namespace Hccl
