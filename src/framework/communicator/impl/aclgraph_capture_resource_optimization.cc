/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_communicator.h"
#include "aicpu_operator_pub.h"

namespace hccl {

// ============================================================================
// ACL Graph Capture 资源复用优化
// ============================================================================

// CollectRemoteRanks: 遍历 transportRequests，收集唯一远程 rankId 及其 {level, ring} 映射
HcclResult HcclCommunicator::CollectRemoteRanks(const OpCommTransport &transport,
    std::set<u32> &uniqueRanks,
    std::unordered_map<u32, std::vector<std::pair<u32, u32>>> &rankLinkMap)
{
    // 遍历所有 level/ring/transportRequest
    for (u32 level = 0; level < transport.size(); level++) {
        for (u32 ring = 0; ring < transport[level].size(); ring++) {
            for (u32 idx = 0; idx < transport[level][ring].transportRequests.size(); idx++) {
                auto &req = transport[level][ring].transportRequests[idx];
                if (req.isValid) {
                    uniqueRanks.insert(req.remoteUserRank);
                    rankLinkMap[req.remoteUserRank].push_back({level, ring});
                }
            }
        }
    }
    HCCL_INFO("[CollectRemoteRanks] collected %u unique ranks, %zu link map entries",
        uniqueRanks.size(), rankLinkMap.size());
    return HCCL_SUCCESS;
}

// CalculateSerializationBlockSize: 计算序列化连续内存块大小
void HcclCommunicator::CalculateSerializationBlockSize(u32 rankCount,
    u64 &headerSize, u64 &perRankSize, u64 &blockSize)
{
    headerSize = sizeof(ZeroCopyTransportHeader)
        + rankCount * sizeof(ZeroCopyTransportRankEntry);
    perRankSize = sizeof(HcclRankRelationResV2) + sizeof(HccltagRemoteResV2);
    blockSize = headerSize + rankCount * perRankSize;
    HCCL_INFO("[CalculateSerializationBlockSize] rankCount[%u] headerSize[%llu] perRankSize[%llu] blockSize[%llu]",
        rankCount, headerSize, perRankSize, blockSize);
}

// AllocateAndZeroBlocks: 分配并清零 HOST/Device 内存块
HcclResult HcclCommunicator::AllocateAndZeroBlocks(u64 blockSize,
    HostMem &hostBlock, DeviceMem &devBlock)
{
    HCCL_INFO("[AllocateAndZeroBlocks] blockSize[%llu]", blockSize);
    CHK_PRT_RET(blockSize == 0, HCCL_ERROR("[AllocateAndZeroBlocks] blockSize is zero"), HCCL_E_PARA);

    hostBlock = HostMem::alloc(blockSize);
    devBlock = DeviceMem::alloc(blockSize);
    CHK_PRT_RET(hostBlock.ptr() == nullptr || devBlock.ptr() == nullptr,
        HCCL_ERROR("[AllocateAndZeroBlocks] alloc memory failed"), HCCL_E_INTERNAL);

    // 清零 HOST 块（序列化源数据）
    CHK_SAFETY_FUNC_RET(memset_s(hostBlock.ptr(), blockSize, 0, blockSize));
    // 清零 Device 块（H2D 拷贝目标）
    CHK_RET(hrtMemSet(devBlock.ptr(), blockSize, blockSize));

    HCCL_INFO("[AllocateAndZeroBlocks] hostBlock[%p] devBlock[%p] blockSize[%llu] zeroed",
        hostBlock.ptr(), devBlock.ptr(), blockSize);
    return HCCL_SUCCESS;
}

// FillOneRankTransport: 从 LINK 对象提取 transport 参数，填充 rank 的序列化节点
HcclResult HcclCommunicator::FillOneRankTransport(u32 rankId, const std::string &tag,
    const OpCommTransport &transport,
    const std::vector<std::pair<u32, u32>> &linkPairs,
    HcclRankRelationResV2 *rankNode, HccltagRemoteResV2 *tagNode,
    const CaptureTransportEntry &entry)
{
    CHK_PRT_RET(rankNode == nullptr || tagNode == nullptr,
        HCCL_ERROR("[FillOneRankTransport] null pointer"), HCCL_E_PARA);

    // 边界检查：rankId 必须在 rankInfoList_ 范围内
    CHK_PRT_RET(rankId >= rankInfoList_.size(),
        HCCL_ERROR("[FillOneRankTransport] invalid rankId[%u], size[%zu]", rankId, rankInfoList_.size()),
        HCCL_E_PARA);

    HCCL_INFO("[FillOneRankTransport] rankId[%u] tag[%s] linkPairs[%zu]", rankId, tag.c_str(), linkPairs.size());

    // 填充 rank 基本信息
    rankNode->remoteUsrRankId = rankId;
    rankNode->remoteWorldRank = rankInfoList_[rankId].worldRank;

    // 填充 tag 字符串（校验长度防止截断）
    CHK_PRT_RET(tag.length() >= TAG_MAX_LENGTH,
        HCCL_ERROR("[FillOneRankTransport] tag length[%zu] exceeds TAG_MAX_LENGTH[%u]",
            tag.length(), TAG_MAX_LENGTH),
        HCCL_E_PARA);
    CHK_SAFETY_FUNC_RET(memcpy_s(tagNode->tag, sizeof(tagNode->tag),
        tag.c_str(), tag.length() + 1));

    // 遍历 linkPairs 中的 {level, ring}，过滤出属于当前 rankId 的 link
    // linkRoce 槽位分配：[0, LINK_ROCE_MAX_NUM)，避免多链路覆盖
    u32 roceSlot = 0;
    u32 p2pCount = 0;
    for (auto it = linkPairs.begin(); it != linkPairs.end(); ++it) {
        u32 level = it->first;
        u32 ring = it->second;
        for (u32 idx = 0; idx < transport[level][ring].links.size(); idx++) {
            auto &link = transport[level][ring].links[idx];
            if (link == nullptr) continue;
            // 只处理属于当前 rankId 的 link
            if (link->GetRemoteRank() != rankId) continue;

            auto &req = transport[level][ring].transportRequests[idx];
            if (!req.isValid) continue;

            // 构建临时 HccltagRemoteResV3 指向本节点的 tagNode
            HccltagRemoteResV3 tagResV3;
            tagResV3.tagRemoteResPtr = tagNode;

            if (req.isUsedRdma) {
                // 主链路（RoCE），使用递增槽位避免覆盖
                if (roceSlot < LINK_ROCE_MAX_NUM) {
                    CHK_RET(BuildOpRemoteLinkRoceResParam(link, tagResV3,
                        (roceSlot % 2 == 1), (roceSlot >= 2), false));
                    roceSlot++;
                    HCCL_DEBUG("[FillOneRankTransport] rank[%u] roce slot[%u] level[%u] ring[%u] idx[%u]",
                        rankId, roceSlot - 1, level, ring, idx);
                } else {
                    HCCL_WARNING("[FillOneRankTransport] rank[%u] roceSlot exhausted, skip link idx[%u]",
                        rankId, idx);
                }
                // 备份链路（如有）
                if (roceSlot < LINK_ROCE_MAX_NUM && !entry.transportBackUp.empty()) {
                    auto &backUpLinks = entry.transportBackUp[level][ring].links;
                    if (idx < backUpLinks.size() && backUpLinks[idx] != nullptr) {
                        CHK_RET(BuildOpRemoteLinkRoceResParam(backUpLinks[idx], tagResV3,
                            (roceSlot % 2 == 1), (roceSlot >= 2), false));
                        roceSlot++;
                        HCCL_DEBUG("[FillOneRankTransport] rank[%u] backup roce slot[%u] level[%u] ring[%u] idx[%u]",
                            rankId, roceSlot - 1, level, ring, idx);
                    }
                }
            } else {
                // P2P 链路
                CHK_RET(BuildOpRemoteLinkP2pResParam(link, tagResV3, req.linkType));
                p2pCount++;
                HCCL_DEBUG("[FillOneRankTransport] rank[%u] p2p level[%u] ring[%u] idx[%u] linkType[%d]",
                    rankId, level, ring, idx, static_cast<u32>(req.linkType));
            }
        }
    }
    HCCL_INFO("[FillOneRankTransport] rank[%u] done roceSlots[%u] p2pCount[%u]", rankId, roceSlot, p2pCount);
    return HCCL_SUCCESS;
}

// BuildDeviceRankLinkedList: 设置 device 侧 rankNode ↔ tagNode 双向链表指针
HcclResult HcclCommunicator::BuildDeviceRankLinkedList(
    HcclRankRelationResV2 *rankNode, HccltagRemoteResV2 *tagNode,
    u8 *devRankNode, u8 *devTagNode)
{
    CHK_PRT_RET(rankNode == nullptr || tagNode == nullptr,
        HCCL_ERROR("[BuildDeviceRankLinkedList] null pointer"), HCCL_E_PARA);

    rankNode->nextTagRes.nextDevice = reinterpret_cast<u64>(devTagNode);
    rankNode->nextTagRes.preDevice = reinterpret_cast<u64>(devTagNode);
    tagNode->nextTagRes.nextDevice = reinterpret_cast<u64>(devRankNode);
    tagNode->nextTagRes.preDevice = reinterpret_cast<u64>(devRankNode);
    HCCL_INFO("[BuildDeviceRankLinkedList] rankNode[%p] tagNode[%p] devRankNode[%p] devTagNode[%p]",
        rankNode, tagNode, devRankNode, devTagNode);
    return HCCL_SUCCESS;
}

// SerializeTransportToDeviceMem: 将 transport 序列化为连续 DeviceMem 块
HcclResult HcclCommunicator::SerializeTransportToDeviceMem(const std::string &tag,
    const CaptureTransportEntry &entry)
{
    HCCL_INFO("[SerializeTransportToDeviceMem] start tag[%s]", tag.c_str());

    // Step 1: 收集远程 rank
    std::set<u32> uniqueRanks;
    std::unordered_map<u32, std::vector<std::pair<u32, u32>>> rankLinkMap;
    CHK_RET(CollectRemoteRanks(entry.transport, uniqueRanks, rankLinkMap));

    // Step 2: 计算内存大小
    u32 rankCount = static_cast<u32>(uniqueRanks.size());
    u64 headerSize, perRankSize, blockSize;
    CalculateSerializationBlockSize(rankCount, headerSize, perRankSize, blockSize);

    // Step 3: 分配内存
    HostMem hostBlock;
    DeviceMem devBlock;
    CHK_RET(AllocateAndZeroBlocks(blockSize, hostBlock, devBlock));

    // Step 4: 获取基地址
    u8 *hostBase = static_cast<u8 *>(hostBlock.ptr());
    u8 *devBase = static_cast<u8 *>(devBlock.ptr());
    ZeroCopyTransportHeader *header = reinterpret_cast<ZeroCopyTransportHeader *>(hostBase);
    // rank 节点和 tag 节点在紧凑数组之后
    u64 rankNodesOffset = headerSize;

    // Step 5: 逐 rank 填充
    u32 rankIndex = 0;
    for (auto &rankId : uniqueRanks) {
        u64 rankOffset = rankNodesOffset + rankIndex * perRankSize;
        auto *rankNode = reinterpret_cast<HcclRankRelationResV2 *>(hostBase + rankOffset);
        auto *tagNode = reinterpret_cast<HccltagRemoteResV2 *>(hostBase + rankOffset + sizeof(HcclRankRelationResV2));

        // 填充 rank 的 transport 参数
        CHK_RET(FillOneRankTransport(rankId, tag, entry.transport, rankLinkMap[rankId],
            rankNode, tagNode, entry));

        // 设置 device 侧环形链表指针
        CHK_RET(BuildDeviceRankLinkedList(rankNode, tagNode,
            devBase + rankOffset, devBase + rankOffset + sizeof(HcclRankRelationResV2)));

        // 写入紧凑数组
        header->entries[rankIndex].rankId = rankId;
        header->entries[rankIndex].addr = reinterpret_cast<u64>(devBase + rankOffset);
        HCCL_DEBUG("[SerializeTransportToDeviceMem] rank[%u] offset[%llu] devAddr[%p]", rankId, rankOffset, devBase + rankOffset);
        rankIndex++;
    }
    header->rankSize = rankCount;

    // Step 6: H2D 拷贝并存入 map
    CHK_RET(hrtMemSyncCopy(devBlock.ptr(), blockSize, hostBlock.ptr(), blockSize,
        HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
    transportDeviceMemMap_[tag] = std::move(devBlock);

    HCCL_INFO("[SerializeTransportToDeviceMem] tag[%s] rankCount[%u] blockSize[%llu] devBlock[%p] success",
        tag.c_str(), rankCount, blockSize, transportDeviceMemMap_[tag].ptr());
    return HCCL_SUCCESS;
}

// ============================================================================
// 清理流程
// ============================================================================

// DestroyCaptureIbvTransportPublic: 清理 capture 资源（transport + deviceMem + resMap）
HcclResult HcclCommunicator::DestroyCaptureIbvTransportPublic(const std::string &captureTag)
{
    HCCL_INFO("[DestroyCaptureIbvTransportPublic] start captureTag[%s]", captureTag.c_str());

    // 1. 清理 transport 资源（Zero Copy 模式）
    auto transportIt = captureTransportMap_.find(captureTag);
    if (transportIt != captureTransportMap_.end()) {
        DestroyOpTransportResponse(transportIt->second.transport);
        if (IsEnableBackupLink()) {
            DestroyOpTransportResponse(transportIt->second.transportBackUp);
        }
        captureTransportMap_.erase(transportIt);
        HCCL_INFO("[DestroyCaptureIbvTransportPublic] captureTag[%s] transport erased from captureTransportMap_", captureTag.c_str());
    }

    // 2. 清理 device 内存（DeviceMem 析构自动释放）
    auto memIt = transportDeviceMemMap_.find(captureTag);
    if (memIt != transportDeviceMemMap_.end()) {
        transportDeviceMemMap_.erase(memIt);
        HCCL_INFO("[DestroyCaptureIbvTransportPublic] captureTag[%s] deviceMem erased from transportDeviceMemMap_", captureTag.c_str());
    }

    // 3. 清理 resMap_ 中的增量资源（HOST 展开模式）
    auto resIt = resMap_.find(captureTag);
    if (resIt != resMap_.end()) {
        DestroyAlgResource(resIt->second);
        resMap_.erase(resIt);
        HCCL_INFO("[DestroyCaptureIbvTransportPublic] captureTag[%s] resource erased from resMap_", captureTag.c_str());
    }

    HCCL_INFO("[DestroyCaptureIbvTransportPublic] captureTag[%s] cleaned", captureTag.c_str());
    return HCCL_SUCCESS;
}

}  // namespace hccl
