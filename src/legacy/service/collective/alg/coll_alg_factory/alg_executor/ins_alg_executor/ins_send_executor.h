/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法库InsSendExecutor类头文件
 * Author: meilinyan
 * Create: 2024-11-14
 */
#ifndef HCCLV2_INS_SEND_EXECUTOR_H
#define HCCLV2_INS_SEND_EXECUTOR_H

#include "ins_coll_alg_base.h"

namespace Hccl {

class InsSendExecutor : public InsCollAlgBase {
public:
    explicit InsSendExecutor();
    ~InsSendExecutor() override;

    std::string Describe() const override
    {
        return "Instruction based Send Executor.";
    }

    HcclResult Orchestrate(const RankGraph *rankGraph, const CollAlgOperator &op, const CollAlgParams &params,
                          InsQuePtr insQue) override;

    HcclResult CalcResOffload(const RankGraph *rankGraph, const u64 &dataSize,
                              CollOffloadOpResReq &resReq) override;

    HcclResult CalcRes(const RankGraph *rankGraph, CollAlgResReq &algResReq) override;

    HcclResult Orchestrate(const AlgTopoInfo &topoInfo, const CollAlgOperator &op, const CollAlgParams &params,
                             ConnectedLinkMgr *linkMgr, InsQuePtr insQue) override;
private:
    std::vector<RankId>              virtRanks_;
    std::map<RankId, u32>            virtRankMap_; // map<virtRank, virtRankOrder>
    std::vector<std::vector<RankId>> vTopo_;
};

} // namespace Hccl
#endif //HCCLV2_INS_SEND_EXECUTOR_H