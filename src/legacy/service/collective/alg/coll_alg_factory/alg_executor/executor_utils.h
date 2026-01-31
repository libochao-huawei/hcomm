/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库Executor公共头文件
 * Author: shenyutian
 * Create: 2024-08-02
 */

#ifndef HCCLV2_EXECUTOR_UTILS
#define HCCLV2_EXECUTOR_UTILS

#include "data_type.h"
#include "log.h"
#include "virtual_topo.h"
#include "template_utils.h"

namespace Hccl {

const std::vector<BasePortType> DEFAULT_LINK_PRIORITY
    = {BasePortType(PortDeploymentType::P2P, ConnectProtoType::HCCS),
       BasePortType(PortDeploymentType::P2P, ConnectProtoType::UB),
       BasePortType(PortDeploymentType::P2P, ConnectProtoType::PCIE),
       BasePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB),
       BasePortType(PortDeploymentType::DEV_NET, ConnectProtoType::RDMA),
       BasePortType(PortDeploymentType::DEV_NET, ConnectProtoType::TCP),
       BasePortType(PortDeploymentType::HOST_NET, ConnectProtoType::RDMA),
       BasePortType(PortDeploymentType::HOST_NET, ConnectProtoType::TCP)};

bool IsEnableCounterNotifyByDevType(const RankId myRank, const DevType devType);

struct TemplateDataParams{
    BuffInfo buffInfo;
    u64 sliceSize{0};
    u64 inputSliceStride{0};
    u64 outputSliceStride{0};
    u64 repeatNum{0};
    u64 inputRepeatStride{0};
    u64 outputRepeatStride{0};
    u64 tailSize{0};
};

HcclResult InitOpInfo(const CollAlgOperator &op, OpType &opType, ReduceOp &redOp, u32 &root);
HcclResult InitDataInfo(const CollAlgOperator &op, DataType &dataType, DataType &outputDataType, u64 &dataCount);

// link prepare
const std::vector<NetInstance::Path> GetPathsFromRankGraph(const RankGraph *rankGraph,
                                                           const RankId srcRank, const RankId dstRank);
HcclResult  AddToResLinks(const RankId vNeighborRank, const LinkData &linkData, ResLinks &resLinks);
HcclResult  PrepResLinks(const RankId myRank, const RankGraph *rankGraph,
                         const std::vector<BasePortType> &linkPriority, const LinkReq &linkReq,
                         ResLinks &resLinks); // host
HcclResult  PrepResLinks(const RankId myRank, const LinkReq &linkReq, ConnectedLinkMgr *linkMgr,
                         ResLinks &resLinks); // aicpu
HcclResult  CalcResLinks(const RankId myRank, const RankGraph *rankGraph,
                         const std::vector<BasePortType> &linkPriority, const LinkReq &linkReq,
                         std::vector<LinkData> &links);
HcclResult  CalcLinkInfo(const RankId myRank, const RankGraph *rankGraph, const LinkReq &linkReq,
                         std::vector<std::pair<u32, RankId>> &algTempLinksInfo);
} // namespace Hccl

#endif // !HCCLV2_EXECUTOR_UTILS