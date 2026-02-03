/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "base_selector.h"

#include "virtual_topo.h"
#include "coll_operator.h"

namespace Hccl {
BaseSelector &BaseSelector::SetVirtualTopo(RankGraph *rankGraph)
{
    rankGraph_ = rankGraph;
    return *this;
}

BaseSelector &BaseSelector::SetDevType(DevType devType)
{
    devType_ = devType;
    return *this;
}

BaseSelector &BaseSelector::SetMyRank(RankId myRank)
{
    myRank_ = myRank;
    return *this;
}

BaseSelector &BaseSelector::SetRankSize(u32 rankSize)
{
    rankSize_ = rankSize;
    return *this;
}

BaseSelector &BaseSelector::SetSeverId(std::string severId)
{
    severId_ = severId;
    return *this;
}

BaseSelector &BaseSelector::SetDeviceNumPerSever(u32 deviceNumPerSever)
{
    deviceNumPerSever_ = deviceNumPerSever;
    return *this;
}

BaseSelector &BaseSelector::SetServerNum(u32 serverNum)
{
    serverNum_ = serverNum;
    return *this;
}

BaseSelector &BaseSelector::SetOpConfig(OpExecuteConfig opConfig)
{
    opConfig_ = opConfig;
    return *this;
}

RankGraph *BaseSelector::GetVirtualTopo()
{
    return rankGraph_;
}

DevType BaseSelector::GetDevType()
{
    return devType_;
}

RankId BaseSelector::GetMyRank() const
{
    return myRank_;
}

u32 BaseSelector::GetRankSize() const
{
    return rankSize_;
}

std::string BaseSelector::GetSeverId()
{
    return severId_;
}

u32 BaseSelector::GetDeviceNumPerSever() const
{
    return deviceNumPerSever_;
}

u32 BaseSelector::GetServerNum() const
{
    return serverNum_;
}

u32 BaseSelector::Gcd(u32 a, u32 b) const
{
    while (b != 0) {
        a %= b;
        std::swap(a, b);
    }
    return a;
}

u32 BaseSelector::GcdOfArray(const std::vector<u32> &numbers) const
{
    if (numbers.empty()) {
        return 0;
    }
    u32 result = numbers[0];
    for (size_t i = 1; i < numbers.size(); ++i) {
        result = Gcd(result, numbers[i]);  // C++17 及以上推荐使用 std::gcd
    }
    return result;
}

u32 BaseSelector::GetLevel0Gcd() {
    std::vector<u32> instSizeList = {};
    u32 listSize = 0;
    rankGraph_->GetNetInstanceList(0, instSizeList, listSize);
    return GcdOfArray(instSizeList);
}

bool BaseSelector::IsAsymmetricTopoShapeLevel1Nhr(
    const std::vector<std::vector<u32>> &localIdPerBoard, u32 gcdRankSizeLevel0) const
{
    // Level0的gcd为1分支
    if (gcdRankSizeLevel0 == 1) {
        return true;
    }
    // Pod形状不规则分支
    if (localIdPerBoard.size() > 1) {
        if (!IsTopoShapeLevel0Regular(localIdPerBoard)) {
            return true;
        }
    }
    return false;
}

bool BaseSelector::IsTopoShapeLevel0Regular(const std::vector<std::vector<u32>> &localIdPerBoard) const
{
    u32 rankSizeOfFirstBoard = localIdPerBoard[0].size();
    u32 rankSize = 8;
    for (u32 boardIdx = 1; boardIdx < localIdPerBoard.size(); ++boardIdx) {
        // 条件1：与第一行rank数是否一致
        if (localIdPerBoard[boardIdx].size() != rankSizeOfFirstBoard) {
            return false;
        }
        // 条件2：同一slot内rank数差异是否能被8整除
        for (u32 slotIdx = 0; slotIdx < rankSizeOfFirstBoard; ++slotIdx) {
            if ((localIdPerBoard[boardIdx][slotIdx] - localIdPerBoard[0][slotIdx]) % rankSize != 0) {
                return false;
            }
        }
    }
    return true;
}

Level0Shape BaseSelector::CalcTopoShapeLevel0(TopoInfo &topoInfo)
{
    HCCL_DEBUG("[BaseSelector][%s] start", __func__);
    std::vector<std::vector<RankId>> rankOnSameBoardVector;
    rankOnSameBoardVector.resize(RANK_SIZE_EIGHT, {});
    std::vector<std::vector<RankId>> rankOnSameSlotVector;
    rankOnSameSlotVector.resize(RANK_SIZE_EIGHT, {});
    std::vector<std::vector<u32>> localIdOnSameBoardVector;
    localIdOnSameBoardVector.resize(RANK_SIZE_EIGHT, {});
    std::vector<u32> numRanksPerBoard;
    std::vector<std::vector<u32>> localIdPerBoard;  // 新增用于记录每行的ranks的localId，用于判断是否形状规则

    const NetInstance *netInstance = rankGraph_->GetNetInstanceByRankId(0, myRank_);
    std::set<RankId> rankSet = netInstance->GetRankIds();
    HCCL_DEBUG("[BaseSelector][%s] start, rankSet size[%zu]", __func__, rankSet.size());
    for (RankId rankId : rankSet) {
        u32 localId = rankGraph_->GetReplacedLocalId(rankId);
        rankOnSameBoardVector[localId / RANK_SIZE_EIGHT].push_back(rankId);
        rankOnSameSlotVector[localId % RANK_SIZE_EIGHT].push_back(rankId);
        localIdOnSameBoardVector[localId / RANK_SIZE_EIGHT].push_back(localId);
    }

    for (u32 i = 0; i < RANK_SIZE_EIGHT; i++) {
        if (rankOnSameBoardVector[i].size() != 0) {
            numRanksPerBoard.push_back(rankOnSameBoardVector[i].size());  //  举例，{3,3,3,3},4行3列
            localIdPerBoard.push_back(localIdOnSameBoardVector[i]);  // 举例，{{0,1,2},{8,9,10},{16,17,18},{24,25,26}}
        }
    }

    HCCL_DEBUG("[BaseSelector][%s] start, numRanksPerBoard size[%d]", __func__, numRanksPerBoard.size());
    if (rankGraph_->GetLevelNum() == 1 || rankGraph_->IsSymmetric(0)) {
        if (numRanksPerBoard.size() == 1 || numRanksPerBoard[0] == 1) {
            return Level0Shape::MESH_1D;
        } else {
            return Level0Shape::MESH_2D;
        }
    }
    u32 gcdRankSizeLevel0 = GetLevel0Gcd();
    // Level1Nhr分支
    if (IsAsymmetricTopoShapeLevel1Nhr(localIdPerBoard, gcdRankSizeLevel0)) {
        topoInfo.Level1Nhr = true;
        return Level0Shape::MESH_1D;
    }
    // Pod形状规则且gcd不为1，判断Level0形状分支
    u32 dim0Size = numRanksPerBoard[0];   // 每行有多少个rank，即列数
    if (gcdRankSizeLevel0 <= dim0Size) {  // Level0为单行
        return Level0Shape::MESH_1D;
    } else if (gcdRankSizeLevel0 % dim0Size != 0) {  // Level0不规则
        topoInfo.Level0Nhr = true;
        return Level0Shape::MESH_1D;
    } else {  // Level0多行且规则
        return Level0Shape::MESH_2D;
    }
    HCCL_DEBUG("[BaseSelector][%s] end", __func__);
}

void BaseSelector::CalcTopoShape(TopoInfo &topoInfo)
{
    std::set<u32> levelSet = rankGraph_->GetLevels(myRank_);
    topoInfo.levelNum = levelSet.size();
    topoInfo.level0Shape = CalcTopoShapeLevel0(topoInfo);
}

u32 BaseSelector::GetNumRanksPerBoard() const
{
    std::vector<std::vector<RankId>> rankOnSameBoardVector;
    rankOnSameBoardVector.resize(RANK_SIZE_EIGHT, {});
    std::vector<u32> numRanksPerBoard;

    const NetInstance *netInstance = rankGraph_->GetNetInstanceByRankId(0, myRank_);
    std::set<RankId> rankSet = netInstance->GetRankIds();
    HCCL_DEBUG("[BaseSelector][%s] start, rankSet size[%zu]", __func__, rankSet.size());
    for (RankId rankId : rankSet) {
        u32 localId = rankGraph_->GetLocalId(rankId);
        rankOnSameBoardVector[localId / RANK_SIZE_EIGHT].push_back(rankId);
    }
    for (u32 i = 0; i < RANK_SIZE_EIGHT; i++) {
        if (rankOnSameBoardVector[i].size() != 0) {
            numRanksPerBoard.push_back(rankOnSameBoardVector[i].size());
        }
    }
    HCCL_DEBUG("[BaseSelector][%s] start, numRanksPerBoard size[%d], numRanksPerBoard[0] is [%d2]",
        __func__, numRanksPerBoard.size(), numRanksPerBoard[0]);
    return numRanksPerBoard[0];
}

bool BaseSelector::IsInputOutputOverlap(const std::shared_ptr<Buffer> &inputMem, const std::shared_ptr<Buffer> &outputMem) const
{
    CHK_PRT_RET(inputMem == nullptr || outputMem == nullptr,
        HCCL_INFO("[Algo][BaseSelector][IsInputOutputOverlap] The input or output buffer is null. Not overlap."),
        false);

    u64 inputStart = inputMem->GetAddr();
    u64 outputStart = outputMem->GetAddr();

    CHK_PRT_RET(inputStart == 0 || outputStart == 0,
        HCCL_INFO("[Algo][BaseSelector][IsInputOutputOverlap] The input or output buffer addr is null. Not overlap."),
        false);

    u64 inputDataSize = inputMem->GetSize();
    u64 outputDataSize = outputMem->GetSize();

    CHK_PRT_RET(inputDataSize == 0 || outputDataSize == 0,
        // 不存在overlap情况
        HCCL_INFO("[Algo][BaseSelector][IsInputOutputOverlap] The input or output buffer size is 0. Not overlap."),
        false);

    u64 inputEnd = inputStart + inputDataSize - 1;
    u64 outputEnd = outputStart + outputDataSize - 1;

    HCCL_DEBUG("[Algo][BaseSelector][IsInputOutputOverlap] inputStart[%llu], inputEnd[%llu], outputStart[%llu], "
               "outputEnd[%llu].",
        inputStart,
        inputEnd,
        outputStart,
        outputEnd);

    CHK_PRT_RET(inputStart <= outputEnd && outputStart <= inputEnd,
        HCCL_INFO("[Algo][BaseSelector][IsInputOutputOverlap] inputStart[%llu], inputEnd[%llu], outputStart[%llu], "
                  "outputEnd[%llu]. Overlap detected.",
            inputStart,
            inputEnd,
            outputStart,
            outputEnd),
        true);

    HCCL_DEBUG("[Algo][BaseSelector][IsInputOutputOverlap]No overlap between input and output memory.");
    return false;
}
}  // namespace Hccl
