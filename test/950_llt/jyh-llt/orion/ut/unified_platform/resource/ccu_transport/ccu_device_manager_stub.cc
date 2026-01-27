/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: ccu device resource manager
 * Create: 2025-02-22
 */

#include "ccu_device_manager.h"

namespace Hccl {

HcclResult CcuAllocChannels(const int32_t deviceLogicId, const CcuChannelPara &ccuChannelPara,
    std::vector<CcuChannelInfo> &ccuChannelInfos)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuReleaseChannel(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t ccuChannelId)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetCcuResourceSpaceBufInfo(const int32_t deviceLogicId, const uint8_t dieId,
    uint64_t &addr, uint64_t &size)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetCcuResourceSpaceTokenInfo(const int32_t deviceLogicId, const uint8_t dieId,
    uint64_t &tokenId, uint64_t &tokenValue)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::ConfigChannel(const int32_t deviceLogicId, const uint8_t dieId,
    ChannelCfg &cfg)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetLoopChannelId(const int32_t deviceLogicId, const uint8_t srcDieId,
    const uint8_t dstDieId, uint32_t &channIdx)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetResource(const int32_t deviceLogicId,
    const CcuResHandle handle, CcuResRepository &ccuResRepo)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::AllocResHandle(const int32_t deviceLogicId, const CcuResReq resReq,
    CcuResHandle &handle)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::ReleaseResHandle(const int32_t deviceLogicId, const CcuResHandle handle)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::AllocIns(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t num, ResInfo &insInfo)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::ReleaseIns(const int32_t deviceLogicId, const uint8_t dieId,
    ResInfo &insInfo)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::AllocCke(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t num, std::vector<ResInfo> &ckeInfos)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::ReleaseCke(const int32_t deviceLogicId, const uint8_t dieId,
    std::vector<ResInfo> &ckeInfos)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::AllocXn(const int32_t deviceLogicId, const uint8_t dieId,
    const uint32_t num, vector<ResInfo>& xnInfos)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::ReleaseXn(const int32_t deviceLogicId, const uint8_t dieId,
    vector<ResInfo> &xnInfos)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetMissionKey(const int32_t deviceLogicId, const uint8_t dieId,
    uint32_t &missionKey)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetInstructionNum(const int32_t deviceLogicId, const uint8_t dieId,
    uint32_t &instrNum)
{
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuDeviceManager::GetXnBaseAddr(const uint32_t devLogicId, const uint8_t dieId,
    uint64_t& xnBaseAddr)
{
    return HcclResult::HCCL_SUCCESS;
}

std::string ResInfo::Describe() const
{
    return StringFormat("ResInfo[startId=%u, num=%u]", startId, num);
}

}; // namespace Hccl