/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_channel_ctx_pool.h"

#include <algorithm>

#include "ccu_dev_mgr_pub.h"
#include "orion_adpt_utils.h"

namespace hcomm {

constexpr uint32_t CCU_DEFAULT_REQUEST_SQ_SIZE = 128;
constexpr uint32_t CCU_DEFAULT_REQUEST_CHANNEL_NUM = 1;
constexpr uint32_t CCU_DEFAULT_REQUEST_JETTY_NUM = 0;

CcuChannelCtxPool::CcuChannelCtxPool(int32_t devLogicId, const CommAddr &srcCommAddr)
    : devLogicId_(devLogicId), srcCommAddr_(srcCommAddr)
{
}

CcuChannelCtxPool::~CcuChannelCtxPool()
{
    channelInfoMap_.clear();
    batches_.clear();
}

CcuChannelCtxPool::ResourceBatch::~ResourceBatch()
{
    for (const auto &key : allChannels) {
        (void)CcuReleaseChannel(devLogicId_, key.first, key.second);
    }
}

HcclResult CcuChannelCtxPool::ResourceBatch::Init(
    const CommAddr &srcCommAddr, const std::vector<CcuChannelInfo> &channelInfos)
{
    Hccl::IpAddress srcIpAddr;
    CHK_RET(CommAddrToIpAddress(srcCommAddr, srcIpAddr));
    const uint32_t channelNum = channelInfos.size();
    allChannels.reserve(channelNum);
    availableChannels.reserve(channelNum);
    for (const auto &channelInfo : channelInfos) {
        const auto dieId = channelInfo.dieId;
        const auto channelId = channelInfo.channelId;
        auto channelIdKey = std::make_pair(dieId, channelId);
        allChannels.emplace_back(dieId, channelId);
        availableChannels.emplace_back(dieId, channelId);

        std::vector<CcuJetty *> jettyPtrs;
        for (const auto &jettyInfo : channelInfo.jettyInfos) {
            const auto taJettyId = jettyInfo.taJettyId;
            const auto jettyIdKey = std::make_pair(dieId, taJettyId);
            if (jettys.find(jettyIdKey) != jettys.end()) {
                continue;
            }

            std::unique_ptr<CcuJetty> ccuJetty;
            CHK_RET(CcuCreateJetty(srcIpAddr, jettyInfo, ccuJetty));
            jettys[jettyIdKey] = std::move(ccuJetty);
            jettyPtrs.emplace_back(jettys[jettyIdKey].get());
        }
        channelCtxMap_.emplace(channelIdKey, std::make_pair(channelInfo, jettyPtrs));
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuChannelCtxPool::AllocChannelCtx(const Hccl::LinkData &link, uint32_t sqSize, ChannelCtxHandle &handle)
{
    Hccl::IpAddress locAddr;
    CHK_RET(CommAddrToIpAddress(srcCommAddr_, locAddr));
    CHK_PRT_RET(!(locAddr == link.GetLocalAddr()),
        HCCL_ERROR("[CcuChannelCtxPool][%s] link localAddr[%s] doesn't match "
            "pool locAddr[%s], devLogicId[%d].",
            __func__, link.GetLocalAddr().Describe().c_str(),
            locAddr.Describe().c_str(), devLogicId_),
        HcclResult::HCCL_E_PARA);

    uint32_t actualSqSize = (sqSize != 0xFFFFFFFF) ? sqSize : CCU_DEFAULT_REQUEST_SQ_SIZE;

    ResourceBatch *batchPtr = FindReusableBatch(link, actualSqSize);

    if (batchPtr == nullptr) {
        auto ret = CreateNewBatch(actualSqSize, batchPtr);
        CHK_PRT_RET(ret == HcclResult::HCCL_E_UNAVAIL,
            HCCL_WARNING("[CcuChannelCtxPool][%s] ccu resources unavailable, "
                         "locAddr[%s], devLogicId[%d], sqSize[%u].",
                __func__, locAddr.Describe().c_str(), devLogicId_, sqSize),
            ret);
        CHK_RET(ret);
    }

    handle = batchPtr->availableChannels.back();
    auto ctxIt = batchPtr->channelCtxMap_.find(handle);
    CHK_PRT_RET(ctxIt == batchPtr->channelCtxMap_.end(),
        HCCL_ERROR("[CcuChannelCtxPool][%s] handle[%u,%u] not found in batch channelCtxMap.", __func__, handle.first,
            handle.second),
        HcclResult::HCCL_E_INTERNAL);
    channelInfoMap_.insert({handle, {ctxIt->second, batchPtr, link}});

    batchPtr->availableChannels.pop_back();
    batchPtr->activeChannelCount++;
    batchPtr->allocatedLinks.insert(link);

    HCCL_INFO("[CcuChannelCtxPool][%s] allocated channelId[%u] of die[%u] to link[%s], "
              "devLogicId[%d], sqSize[%u].",
        __func__, handle.second, handle.first, link.Describe().c_str(), devLogicId_, sqSize);
    return HcclResult::HCCL_SUCCESS;
}

CcuChannelCtxPool::ResourceBatch *CcuChannelCtxPool::FindReusableBatch(
    const Hccl::LinkData &link, uint32_t sqSize) const
{
    for (const auto &batch : batches_) {
        if (batch->sqSize != sqSize) {
            continue;
        }
        if (batch->allocatedLinks.find(link) != batch->allocatedLinks.end()) {
            continue;
        }
        if (!batch->availableChannels.empty()) {
            return batch.get();
        }
    }
    return nullptr;
}

HcclResult CcuChannelCtxPool::CreateNewBatch(uint32_t actualSqSize, ResourceBatch *&batchPtr)
{
    Hccl::IpAddress locAddr;
    CHK_RET(CommAddrToIpAddress(srcCommAddr_, locAddr));

    const CcuChannelPara channelPara{
        srcCommAddr_, CCU_DEFAULT_REQUEST_CHANNEL_NUM, CCU_DEFAULT_REQUEST_JETTY_NUM, actualSqSize};

    std::vector<CcuChannelInfo> channelInfos;
    auto ret = CcuAllocChannels(devLogicId_, channelPara, channelInfos);
    CHK_PRT_RET(ret == HcclResult::HCCL_E_UNAVAIL,
        HCCL_WARNING("[CcuChannelCtxPool][%s] ccu resources unavailable, locAddr[%s], devLogicId[%d].", __func__,
            locAddr.Describe().c_str(), devLogicId_),
        ret);
    CHK_RET(ret);

    auto newBatch = std::unique_ptr<ResourceBatch>(new (std::nothrow) ResourceBatch(actualSqSize, devLogicId_));
    CHK_PTR_NULL(newBatch);
    ret = newBatch->Init(srcCommAddr_, channelInfos);
    if (ret != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuChannelCtxPool][%s] failed, try to release temp ccu resources, locAddr[%s], "
                   "devLogicId[%d].",
            __func__, locAddr.Describe().c_str(), devLogicId_);
        for (const auto &channelInfo : channelInfos) {
            (void)CcuReleaseChannel(devLogicId_, channelInfo.dieId, channelInfo.channelId);
        }
        newBatch->allChannels.clear();
        return ret;
    }

    batches_.push_back(std::move(newBatch));
    batchPtr = batches_.back().get();
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuChannelCtxPool::ReleaseChannelCtx(const ChannelCtxHandle &handle)
{
    auto it = channelInfoMap_.find(handle);
    CHK_PRT_RET(it == channelInfoMap_.end(),
        HCCL_ERROR("[CcuChannelCtxPool][%s] handle[%u,%u] not found.", __func__, handle.first, handle.second),
        HcclResult::HCCL_E_NOT_FOUND);

    ResourceBatch *batchPtr = it->second.batch;
    const auto &link = it->second.link;

    batchPtr->availableChannels.push_back(handle);
    batchPtr->activeChannelCount--;
    batchPtr->allocatedLinks.erase(link);

    channelInfoMap_.erase(it);

    if (batchPtr->activeChannelCount == 0) {
        batches_.erase(std::find_if(batches_.begin(), batches_.end(),
            [batchPtr](const auto &b) {
                return b.get() == batchPtr;
        }));
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuChannelCtxPool::GetChannelCtx(const ChannelCtxHandle &handle, CcuChannelCtx &ctx) const
{
    auto it = channelInfoMap_.find(handle);
    CHK_PRT_RET(it == channelInfoMap_.end(),
        HCCL_ERROR("[CcuChannelCtxPool][%s] handle[%u,%u] not found.", __func__, handle.first, handle.second),
        HCCL_E_NOT_FOUND);
    ctx = it->second.ctx;
    return HCCL_SUCCESS;
}

} // namespace hcomm