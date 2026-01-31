/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_mc2_compont.h"
#include "hccl_aiv_utils.h"
#include <algorithm>
#include <unordered_set>
#include "mc2_context.h"
#include "coll_service_device_mode.h"
#include "aiv_ins.h"

namespace Hccl {

constexpr u32 AIV_FAKE_OP_TPYE = 1;

AivMc2Compont::~AivMc2Compont()
{
}

void AivMc2Compont::AllocCommResource(void *mc2Tiling, void **commContext)
{
    auto tilingVersion = *static_cast<uint32_t *>(mc2Tiling);
    HCCL_INFO("[AivMc2Compont:%s] Tiling version [%u]", __func__, tilingVersion);
    if (tilingVersion != UNKNOWN_TILING_V1 && tilingVersion != UNKNOWN_TILING_V2) {
        THROW<NotSupportException>(StringFormat("Tiling version not support, version[%u]", tilingVersion));
    }

    if (comm->GetRankSize() == 1) {
        THROW<NotSupportException>(StringFormat("Comm[%s] rank size is 1, Mc2 not support", comm->GetId().c_str()));
    }

    if (tilingVersion == UNKNOWN_TILING_V1) {
        AivMC2AllocCommRes(reinterpret_cast<Mc2Tiling *>(mc2Tiling));
    } else {
        AivMC2AllocCommResV2(reinterpret_cast<Mc2InitTilingInner *>(mc2Tiling));
    }
    
    GenerateCommContext(commContext);
}

void AddCclBuffer(HcclCombinOpParam &combinOpParam, uint64_t bufferSize, uint64_t bufferAddr, RankId rankId)
{
    combinOpParam.winSize            = bufferSize;
    auto winInSize                   = bufferSize / 2;
    combinOpParam.windowsIn[rankId]  = bufferAddr;
    combinOpParam.windowsOut[rankId] = bufferAddr + winInSize;

    HCCL_RUN_INFO("hcclCombinOpParam info: bufferSize = [%llu], windowsIn[%d] = [%llu], windowsOut[%d] = [%llu]",
                  combinOpParam.winSize, rankId, combinOpParam.windowsIn[rankId], rankId,
                  combinOpParam.windowsOut[rankId]);
}

void AivMc2Compont::GenerateCommContext(void **commContext)
{
    if (combinOpParamBuffer != nullptr) {
        *commContext = reinterpret_cast<void *>(combinOpParamBuffer->GetAddr());
        return;
    }
    
    workspaceBuffer                    = std::make_shared<DevBuffer>(MC2_WORKSPACE_SIZE);

    HcclCombinOpParam combinOpParam{0};
    combinOpParam.workSpace     = static_cast<uint64_t>(workspaceBuffer->GetAddr());
    combinOpParam.workSpaceSize = MC2_WORKSPACE_SIZE;
    combinOpParam.rankId        = comm->GetMyRank();
    combinOpParam.rankDim       = comm->GetRankSize();

    HCCL_RUN_INFO("hcclCombinOpParam info: workSpace = [%llu], rankId = [%u], rankDim = [%u]",
                  combinOpParam.workSpace, combinOpParam.rankId, combinOpParam.rankDim);
    // add cclbuffer info
    if (comm->GetCclBuffer() == nullptr) {
        THROW<Hccl::InternalException>(StringFormat("Cannot get CCL Buffer to fill window!"));
    }

    // winIn winOut 放到 commContext
    uint64_t commCclBufferSize = static_cast<uint64_t>(comm->GetCclBuffer()->GetSize());
    uint64_t commCclBufferAddr = static_cast<uint64_t>(comm->GetCclBuffer()->GetAddr());
    AddCclBuffer(combinOpParam, commCclBufferSize, commCclBufferAddr, comm->GetMyRank());

    // 获取ubmemoryTranMgr，遍历所有ubmemoryTran，拿到CclBuffer，一分为2，放到windowsIn、windowsOut
    auto rankId2RmtIpcRmaBufList = comm->GetUbMemoryTransportMgr()->GetRmtRankId2RmtIpcRmaBufList();
    for (auto& rankId2RmtIpcRmaBuf : rankId2RmtIpcRmaBufList) {
        auto     rmtRank       = rankId2RmtIpcRmaBuf.first;
        auto     rmtMemBuffer  = rankId2RmtIpcRmaBuf.second;
        uint64_t rmtBufferSize = static_cast<uint64_t>(rmtMemBuffer->GetSize());
        uint64_t rmtBufferAddr = static_cast<uint64_t>(rmtMemBuffer->GetAddr());
        AddCclBuffer(combinOpParam, rmtBufferSize, rmtBufferAddr, rmtRank);
    }

    auto paramSize      = sizeof(HcclCombinOpParam);
    combinOpParamBuffer = std::make_shared<DevBuffer>(paramSize);
    HrtMemcpy(reinterpret_cast<void *>(combinOpParamBuffer->GetAddr()), paramSize, static_cast<void *>(&combinOpParam),
              paramSize, RT_MEMCPY_HOST_TO_DEVICE);
    *commContext = reinterpret_cast<void *>(combinOpParamBuffer->GetAddr());
}

void AivMc2Compont::AivMC2AllocCommRes(Mc2Tiling *mc2TilingPtr) const
{
    HCCL_INFO("AivMC2AllocCommRes start");
    auto insQueue = make_shared<InsQueue>();

    auto                            collService = dynamic_cast<CollServiceDeviceMode *>(comm->GetCollService());
    auto                            aivLinks    = comm->GetFullMeshLinks();
    
    for (auto &link : aivLinks) {
        HCCL_INFO("[AivMc2Compont][AivMC2AllocCommRes] aivLink[%s]", link.Describe().c_str());
    }
    
    Mc2CommConfig *commConfigPtr = reinterpret_cast<Mc2CommConfig *>(
        reinterpret_cast<uint8_t *>(mc2TilingPtr) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(Mc2ServerCfg));
    const auto &commConfig = *(commConfigPtr);
    FillCollOperator(commConfig);

    AivOpArgs aivArgs {};

    std::unique_ptr<Instruction> aivIns = std::make_unique<AivInstruction>(aivLinks, aivArgs);

    insQueue->Append(std::move(aivIns));

    comm->GetSocketManager().BatchCreateSockets(aivLinks);

    // 对insQueue中ccuIns进行预处理 创建transport
    collService->GetAivInsPreprocessor()->Preprocess(insQueue);
    HCCL_INFO("AivMC2AllocCommRes success");
}

void AivMc2Compont::AivMC2AllocCommResV2(Mc2InitTilingInner *mc2TilingPtr) const
{
        HCCL_INFO("AivMC2AllocCommRes start");
    auto insQueue = make_shared<InsQueue>();

    auto                            collService = dynamic_cast<CollServiceDeviceMode *>(comm->GetCollService());
    auto                            aivLinks    = comm->GetFullMeshLinks();
    
    for (auto &link : aivLinks) {
        HCCL_INFO("[AivMc2Compont][AivMC2AllocCommRes] aivLink[%s]", link.Describe().c_str());
    }

    const auto  offset = mc2TilingPtr->offset[0];
    const auto &commConfig
        = *(reinterpret_cast<const Mc2CcTilingInner *>(reinterpret_cast<const uint8_t *>(mc2TilingPtr) + offset));
    FillCollOperatorV2(commConfig);

    AivOpArgs aivArgs {};

    std::unique_ptr<Instruction> aivIns = std::make_unique<AivInstruction>(aivLinks, aivArgs);

    insQueue->Append(std::move(aivIns));

    comm->GetSocketManager().BatchCreateSockets(aivLinks);

    // 对insQueue中ccuIns进行预处理 创建transport
    collService->GetAivInsPreprocessor()->Preprocess(insQueue);
    HCCL_INFO("AivMC2AllocCommResV2 success");
}

// 为保证后续流程统一，构造一个虚拟算子
void AivMc2Compont::FillCollOperator(const Mc2CommConfig &config) const
{
    uint32_t dataCount = 1024;
    CollOpParams opParams;
    opParams.opType         = MC2OpType(static_cast<AicpuComType>(config.opType));
    opParams.reduceOp       = MC2ReduceType(static_cast<HcclReduceOp>(config.reduceType));
    opParams.dataType       = MC2DataType(static_cast<HcclDataType>(config.dataType));
    opParams.outputDataType = MC2DataType(static_cast<HcclDataType>(config.outputDataType));
    opParams.count          = dataCount;
    std::string opTag       = comm->GetId();
    comm->CovertToCurrentCollOperator(opTag, opParams, OpMode::OPBASE);
}

void AivMc2Compont::FillCollOperatorV2(const Mc2CcTilingInner &config) const
{
    uint32_t dataCount = 1024;
    CollOpParams opParams;
    opParams.opType         = MC2OpType(static_cast<AicpuComType>(AIV_FAKE_OP_TPYE));
    opParams.reduceOp       = MC2ReduceType(static_cast<HcclReduceOp>(config.reduceType));
    opParams.dataType       = MC2DataType(static_cast<HcclDataType>(config.srcDataType));
    opParams.outputDataType = MC2DataType(static_cast<HcclDataType>(config.dstDataType));
    opParams.count          = dataCount;
    std::string opTag       = comm->GetId();

    comm->CovertToCurrentCollOperator(opTag, opParams, OpMode::OPBASE);
}

} // namespace Hccl