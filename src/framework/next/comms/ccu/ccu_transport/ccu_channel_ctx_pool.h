/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_CHANNELCTX_POOLS_H
#define CCU_CHANNELCTX_POOLS_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "ccu_jetty_.h"
#include "hash_utils.h"
#include "virtual_topo.h"
#include "hccl_rank_graph.h"

namespace hcomm {

class CcuChannelCtxPool final {
public:
    explicit CcuChannelCtxPool(int32_t devLogicId, const CommAddr &srcCommAddr);
    ~CcuChannelCtxPool();

    using CcuChannelCtx = std::pair<CcuChannelInfo, std::vector<CcuJetty *>>;
    using ChannelCtxHandle = std::pair<uint8_t, uint32_t>;
    HcclResult AllocChannelCtx(const Hccl::LinkData &link, uint32_t sqSize, ChannelCtxHandle &handle);
    HcclResult GetChannelCtx(const ChannelCtxHandle &handle, CcuChannelCtx &ctx) const;
    HcclResult ReleaseChannelCtx(const ChannelCtxHandle &handle);

private:
    struct ResIdHash {
        std::size_t operator()(const std::pair<uint8_t, uint32_t> &p) const
        {
            return Hccl::HashCombine({p.first, p.second});
        }
    };

    using DieJettyKey = std::pair<uint8_t, uint32_t>;

    struct ResourceBatch;

    struct ChannelInfoEntry {
        CcuChannelCtx ctx;
        ResourceBatch *batch{nullptr};
        Hccl::LinkData link;
    };

    struct ResourceBatch {
        uint32_t sqSize{0};
        std::vector<ChannelCtxHandle> allChannels;
        std::vector<ChannelCtxHandle> availableChannels;
        std::unordered_map<ChannelCtxHandle, CcuChannelCtx, ResIdHash> channelCtxMap_;
        std::unordered_map<DieJettyKey, std::unique_ptr<CcuJetty>, ResIdHash> jettys;
        uint32_t activeChannelCount{0};
        std::unordered_set<Hccl::LinkData> allocatedLinks;
        int32_t devLogicId_{0};

        ResourceBatch(uint32_t sqSize, int32_t devLogicId) : sqSize(sqSize), devLogicId_(devLogicId) {};
        ~ResourceBatch();
        HcclResult Init(const CommAddr &srcCommAddr, const std::vector<CcuChannelInfo> &channelInfos);
    };

private:
    ResourceBatch *FindReusableBatch(const Hccl::LinkData &link, uint32_t sqSize) const;
    HcclResult CreateNewBatch(uint32_t actualSqSize, ResourceBatch *&batchPtr);

private:
    int32_t devLogicId_{0};
    CommAddr srcCommAddr_{};
    std::vector<std::unique_ptr<ResourceBatch>> batches_;
    std::unordered_map<ChannelCtxHandle, ChannelInfoEntry, ResIdHash> channelInfoMap_;
};

} // namespace hcomm
#endif // CCU_CHANNELCTX_POOLS_H