/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_dev_mgr_imp.h"

#include "hccl_res.h"
#include "hccl_common.h"
#include "eid_info_mgr.h"

#include "ccu_comp.h"
#include "ccu_res_specs.h"
#include "ccu_res_batch_allocator.h"

#include "adapter_rts.h"

namespace hcomm {

static std::unordered_map<int32_t, std::shared_ptr<CcuDrvHandle>> ccuDrvHandleMap;
static std::mutex ccuDrvHandleMutex;

HcclResult CcuInitFeature(const int32_t devLogicId, std::shared_ptr<CcuDrvHandle> &ccuDrvHandle)
{
    if (devLogicId >= static_cast<int32_t>(MAX_MODULE_DEVICE_NUM)) {
        HCCL_ERROR("[%s] failed, devLogicId[%d] is too large, should be less than %u.",
            __func__, devLogicId, MAX_MODULE_DEVICE_NUM);
        return HcclResult::HCCL_E_PARA;
    }

    std::lock_guard<std::mutex> lock(ccuDrvHandleMutex);
    auto iter = ccuDrvHandleMap.find(devLogicId);
    if (iter != ccuDrvHandleMap.end()) {
        ccuDrvHandle = iter->second;
        HCCL_RUN_INFO("[%s] devLogicId[%d] init ccu feature, handle[0x%llx].",
            __func__, devLogicId, ccuDrvHandle.get());
        return HcclResult::HCCL_SUCCESS;
    }

    std::shared_ptr<CcuDrvHandle> drvHandle = nullptr;
    drvHandle.reset(new (std::nothrow) CcuDrvHandle(devLogicId));
    CHK_PTR_NULL(drvHandle);
    CHK_RET(drvHandle->Init());
    ccuDrvHandleMap[devLogicId] = drvHandle;
    ccuDrvHandle = ccuDrvHandleMap[devLogicId];
    HCCL_RUN_INFO("[%s] devLogicId[%d] init ccu feature, handle[0x%llx].",
        __func__, devLogicId, ccuDrvHandle.get());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeinitFeature(const int32_t devLogicId)
{
    std::lock_guard<std::mutex> lock(ccuDrvHandleMutex);
    auto iter = ccuDrvHandleMap.find(devLogicId);
    if (iter == ccuDrvHandleMap.end()) {
        HCCL_INFO("[%s] passed, ccu feature was not be inited, devLogicId[%d].",
            __func__, devLogicId);
        return HcclResult::HCCL_SUCCESS;
    }

    auto &ccuDrvHandle = ccuDrvHandleMap[devLogicId];
    if (ccuDrvHandle.use_count() == 1) {
        HCCL_RUN_INFO("[%s] entry, start to deinit ccu feature, "
            "handle[0x%llx] devLogicId[%d].",
            __func__, ccuDrvHandle.get(), devLogicId);
        ccuDrvHandle = nullptr;
        ccuDrvHandleMap.erase(devLogicId);
    }

    return HcclResult::HCCL_SUCCESS;
}

// CCU设备管理对集合通信提供的接口
HcclResult CcuAllocEngineResHandle(const int32_t deviceLogicId,
    const CcuEngine ccuEngine, CcuResHandle &resHandle)
{
    auto dieEnableFlags = CcuComponent::GetInstance(deviceLogicId).GetDieEnableFlags();
    if (!dieEnableFlags[0] && !dieEnableFlags[1]) {
        HCCL_ERROR("[%s] failed, all ccu dies are disable, devLogicId[%d].",
            __func__, deviceLogicId);
        return HcclResult::HCCL_E_INTERNAL;
    }

    CcuResReq resReq{};
    for (uint8_t dieId = 0; dieId < CCU_MAX_IODIE_NUM; dieId++) {
        if (!dieEnableFlags[dieId]) {
            continue;
        }

        if (ccuEngine == CcuEngine::CCU_MS) {
            resReq.loopEngineReq[dieId] = 0;
            resReq.blockLoopEngineReq[dieId] = 8 * 8 * 2;
            resReq.msReq[dieId] = 0;
            resReq.blockMsReq[dieId] = 64 * 8 * 2;
            resReq.ckeReq[dieId] = 32;
            resReq.blockCkeReq[dieId] = 8 * 8 * 2;
            resReq.continuousXnReq[dieId] = 0;
            resReq.xnReq[dieId] = 400;
            resReq.gsaReq[dieId] = 400;
            resReq.missionReq.reqType = MissionReqType::FUSION_MULTIPLE_DIE;
            resReq.missionReq.req[dieId] = 2;
        } else {
            resReq.loopEngineReq[dieId] = 0;
            resReq.blockLoopEngineReq[dieId] = 16;
            resReq.msReq[dieId] = 0;
            resReq.blockMsReq[dieId] = 128;
            resReq.ckeReq[dieId] = 32;
            resReq.blockCkeReq[dieId] = 16;
            resReq.continuousXnReq[dieId] = 0;
            resReq.xnReq[dieId] = 400;
            resReq.gsaReq[dieId] = 400;
            resReq.missionReq.reqType = MissionReqType::FUSION_MULTIPLE_DIE;
            resReq.missionReq.req[dieId] = 2;
        }
    }

    CHK_RET(CcuDevMgrImp::AllocResHandle(deviceLogicId, resReq, resHandle));
    HCCL_INFO("[%s] succeed, get res handle[%llx], devLogicId[%d]", __func__, resHandle, deviceLogicId);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuCheckResource(const int32_t deviceLogicId, const CcuResHandle resHandle, CcuResRepository &resRepo)
{
    CHK_RET(CcuDevMgrImp::GetResource(deviceLogicId, resHandle, resRepo));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuReleaseResHandle(const int32_t deviceLogicId, const CcuResHandle resHandle)
{
    CHK_RET(CcuDevMgrImp::ReleaseResHandle(deviceLogicId, resHandle));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuAllocChannels(const int32_t deviceLogicId, const CcuChannelPara &ccuChannelPara,
    std::vector<CcuChannelInfo> &ccuChannelInfos)
{
    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(ccuChannelPara.commAddr, ipAddr)); // 为了打印信息暂时添加
    HCCL_INFO("[%s] new allocation request: deviceLogicId[%d], ipAddr[%s], "
        "channelnum[%u], jettyNum[%u], sqSize[%u].", __func__, deviceLogicId,
        ipAddr.Describe().c_str(), ccuChannelPara.channelNum,
        ccuChannelPara.jettyNum, ccuChannelPara.sqSize);

    uint32_t devPhyId{0};
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<uint32_t>(deviceLogicId), devPhyId));

    DevEidInfo eidInfo{};
    CHK_RET(EidInfoMgr::GetInstance(devPhyId).GetEidInfoByAddr(ccuChannelPara.commAddr, eidInfo));
    const uint8_t dieId = static_cast<uint8_t>(eidInfo.dieId);
    const uint32_t feId = eidInfo.funcId;
    ChannelPara para{};
    para.feId = feId;
    para.jettyNum = ccuChannelPara.jettyNum;
    para.sqSize = ccuChannelPara.sqSize;
    return CcuComponent::GetInstance(deviceLogicId).AllocChannels(dieId, para, ccuChannelInfos);
}

HcclResult CcuReleaseChannel(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t ccuChannelId)
{
    HCCL_INFO("[%s] new release request: deviceLogicId[%d], dieId[%u], "
        "ccuChannelId[%u].", __func__, deviceLogicId, dieId, ccuChannelId);

    return CcuComponent::GetInstance(deviceLogicId).ReleaseChannel(dieId, ccuChannelId);
}

// 以下为hcomm基础通信内部CCU流程使用的接口
HcclResult CcuDevMgrImp::GetCcuVersion(const int32_t deviceLogicId, CcuVersion &ccuVersion)
{
    ccuVersion = CcuResSpecifications::GetInstance(deviceLogicId).GetCcuVersion();
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDevMgrImp::GetCcuResourceSpaceBufInfo(const int32_t deviceLogicId, const uint8_t dieId,
    uint64_t &addr, uint64_t &size)
{
    return CcuComponent::GetInstance(deviceLogicId).GetCcuResourceSpaceBufInfo(dieId, addr, size);
}

HcclResult CcuDevMgrImp::GetCcuResourceSpaceTokenInfo(const int32_t deviceLogicId, const uint8_t dieId,
    uint64_t &tokenId, uint64_t &tokenValue)
{
    return CcuComponent::GetInstance(deviceLogicId).GetCcuResourceSpaceTokenInfo(dieId, tokenId, tokenValue);
}

HcclResult CcuDevMgrImp::ConfigChannel(const int32_t deviceLogicId, const uint8_t dieId,
    ChannelCfg &cfg)
{
    return CcuComponent::GetInstance(deviceLogicId).ConfigChannel(dieId, cfg);
}

HcclResult CcuDevMgrImp::GetLoopChannelId(const int32_t deviceLogicId, const uint8_t srcDieId,
    const uint8_t dstDieId, uint32_t &channIdx)
{
    return CcuComponent::GetInstance(deviceLogicId).GetLoopChannelId(srcDieId, dstDieId, channIdx);
}

HcclResult CcuDevMgrImp::GetResource(const int32_t deviceLogicId,
    const CcuResHandle handle, CcuResRepository &ccuResRepo)
{
    return CcuResBatchAllocator::GetInstance(deviceLogicId).GetResource(handle, ccuResRepo);
}

HcclResult CcuDevMgrImp::AllocResHandle(const int32_t deviceLogicId, const CcuResReq resReq,
    CcuResHandle &handle)
{
    return CcuResBatchAllocator::GetInstance(deviceLogicId).AllocResHandle(resReq, handle);
}

HcclResult CcuDevMgrImp::ReleaseResHandle(const int32_t deviceLogicId, const CcuResHandle handle)
{
    return CcuResBatchAllocator::GetInstance(deviceLogicId).ReleaseResHandle(handle);
}

HcclResult CcuDevMgrImp::AllocIns(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t num, ResInfo &insInfo)
{
    return CcuComponent::GetInstance(deviceLogicId).AllocIns(dieId, num, insInfo);
}

HcclResult CcuDevMgrImp::ReleaseIns(const int32_t deviceLogicId, const uint8_t dieId,
    const ResInfo &insInfo)
{
    return CcuComponent::GetInstance(deviceLogicId).ReleaseIns(dieId, insInfo);
}

HcclResult CcuDevMgrImp::AllocCke(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t num, std::vector<ResInfo> &ckeInfos)
{
    return CcuComponent::GetInstance(deviceLogicId).AllocCke(dieId, num, ckeInfos);
}

HcclResult CcuDevMgrImp::ReleaseCke(const int32_t deviceLogicId, const uint8_t dieId,
    const std::vector<ResInfo> &ckeInfos)
{
    return CcuComponent::GetInstance(deviceLogicId).ReleaseCke(dieId, ckeInfos);
}

HcclResult CcuDevMgrImp::AllocXn(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t num, std::vector<ResInfo>& xnInfos)
{
    return CcuComponent::GetInstance(deviceLogicId).AllocXn(dieId, num, xnInfos);
}

HcclResult CcuDevMgrImp::ReleaseXn(const int32_t deviceLogicId, const uint8_t dieId,
    const std::vector<ResInfo> &xnInfos)
{
    return CcuComponent::GetInstance(deviceLogicId).ReleaseXn(dieId, xnInfos);
}

HcclResult CcuDevMgrImp::GetMissionKey(const int32_t deviceLogicId, const uint8_t dieId,
    uint32_t &missionKey)
{
    return CcuResSpecifications::GetInstance(deviceLogicId).GetMissionKey(dieId, missionKey);
}

HcclResult CcuDevMgrImp::GetInstructionNum(const int32_t deviceLogicId, const uint8_t dieId,
    uint32_t &instrNum)
{
    return CcuResSpecifications::GetInstance(deviceLogicId).GetInstructionNum(dieId, instrNum);
}

HcclResult CcuDevMgrImp::GetXnBaseAddr(const uint32_t devLogicId, const uint8_t dieId,
    uint64_t& xnBaseAddr)
{
    return CcuResSpecifications::GetInstance(devLogicId).GetXnBaseAddr(dieId, xnBaseAddr);
}

HcclResult CheckDieValid(const char *funcName, const int32_t devLogicId, const uint8_t dieId,
    const std::array<bool, CCU_MAX_IODIE_NUM> &dieEnableFlags)
{
    CHK_PRT_RET(dieId >= CCU_MAX_IODIE_NUM,
        HCCL_ERROR("[%s] failed, dieId[%u] is invalid, shoudle be in [0-%u), devLogicId[%d].",
            funcName, dieId, CCU_MAX_IODIE_NUM, devLogicId),
        HcclResult::HCCL_E_PARA);

    CHK_PRT_RET(!dieEnableFlags[dieId],
        HCCL_ERROR("[%s] failed, dieId[%u] is disable, devLogicId[%d].",
            funcName, dieId, devLogicId),
        HcclResult::HCCL_E_PARA);

    return HcclResult::HCCL_SUCCESS;
}

}; // namespace hcomm