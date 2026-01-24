/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: 算法模板InsAlgTemplateBase类头文件
 * Author: shenyutian
 * Create: 2024-04-30
 */

#ifndef HCCLV2_INS_ALG_TEMPLATE_BASE
#define HCCLV2_INS_ALG_TEMPLATE_BASE

#include <memory>
#include <map>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include "const_val.h"
#include "template_utils.h"
#include "instruction.h"
#include "ins_queue.h"

namespace Hccl {
using InsQuePtr = std::shared_ptr<InsQueue>;

class InsAlgTemplateBase {
public:
    explicit InsAlgTemplateBase(const RankId virtualRank, const u32 tempRankSize,
                                const std::vector<std::vector<RankId>> &tempVTopo,
                                const std::map<RankId, u32>            &tempVirtRankMap);
    virtual ~InsAlgTemplateBase();

    virtual std::string Describe() const = 0;

    virtual HcclResult Run(const TempFuncs &tempFuncs, const RankSliceInfo &sliceInfoVec,
                                 const BuffInfo &buffInfo, const ResLinks &tempLinks,
                                 std::vector<InsQuePtr> &tempInsQues);

    // init reduceInfo
    void InitReduceInfo(const ReduceOp &redOp, const DataType &dataType);

    void SetDataType(const DataType &dataType);

    void SetReduceOp(const ReduceOp &redOp);

    void SetDmaMode(const DmaMode dmaMode);

    void SetRoot(const u32 root);

    void SetCollOp(const CollAlgOperator &op);

    void SetLoadInfo(const CollAlgParams &params) const;

    // calculate slices
    virtual HcclResult CalcSliceInfo(const AllignInfo &allignInfo, const u64 dataSize, RankSliceInfo &sliceInfoVec);

    // calculate resources
    virtual HcclResult CalcRes(AlgTempResReq &tempResReq);
    virtual HcclResult CalcResDetour(const RankGraph *rankGraph, AlgTempResReq &tempResReq);
    virtual HcclResult CalcResDetour(ConnectedLinkMgr *linkMgr, AlgTempResReq &tempResReq);
    virtual uint64_t GetMaxSliceSize();
    virtual u64 CalcLoopMaxCount(ParamPool &paramPool);
    virtual HcclResult GetMaxTransPortDataSize(u64 &maxTransPortDataSize) const;
    virtual HcclResult CalBlockDim(u32& blockDim, u64 dataSize, u32 blockDimLimit);

    std::vector<std::tuple<QId, QId, u32>> CreateMasterSlaveQueNotifiesRequest(u32 queueNum, u32 pairNum = 1,
                                                                               QId masterId = 0) const;

    std::vector<std::tuple<QId, QId, u32>>
    CreateNotifiesRequestByMap(std::map<std::tuple<QId, QId>, u32> &notifyRequestMap) const;

    std::vector<std::tuple<QId, QId, u32>>
    MergeNotifiesRequest(const std::vector<std::vector<std::tuple<QId, QId, u32>>> &notifiesRequests) const;

    // Sync
    HcclResult PreSync(const u32 queIdx, std::vector<InsQuePtr> &syncInsQues) const;
    HcclResult PostSync(const u32 queIdx, std::vector<InsQuePtr> &syncInsQues) const;
    HcclResult PreSyncInterQueues(std::vector<InsQuePtr> &syncInsQues) const;
    HcclResult PostSyncInterQueues(std::vector<InsQuePtr> &syncInsQues) const;

protected:
    HcclResult PostCopyOpbase(const UsrData &usrData, std::vector<InsQuePtr> &tempInsQues) const;
    HcclResult PreCopyOpbase(const UsrData &usrData, std::vector<InsQuePtr> &tempInsQues) const;

    HcclResult PrepBitMask(const u32 queNumPerNeighbor);

    OpMode opMode_;
    u32 root_ = 0;

    CollAlgOperator                  op_;
    RankId                           myRank_       = INVALID_RANKID;
    u32                              tempRankSize_ = 0;
    std::vector<std::vector<RankId>> tempVTopo_;
    std::map<RankId, u32>            tempVirtRankMap_;

    BuffInfo buffInfo_;

    u32      queNum_ = 0;
    ReduceOp redOp_;
    DataType dataType_;

    bool enableCounterNotify_ = false;

    DmaMode dmaMode_ = DmaMode::DEFAULT;

    bool enableDetour_    = false;
    u32  linkNumBtwPeers_ = 1;

    std::map<RankId, std::vector<u32>> rank2BitPos_;
};
} // namespace Hccl

#endif // !HCCLV2_INS_ALG_TEMPLATE_BASE