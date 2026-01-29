/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 自适应算法选择基类文件头
 * Author: libiaozhi
 * Create: 2025-06-12
 */
#ifndef HCCLV2_AUTO_SELECTOR
#define HCCLV2_AUTO_SELECTOR

#include "base_selector.h"
#include "coll_alg_params.h"
#include "coll_operator.h"
#include "virtual_topo.h"

namespace Hccl {

constexpr uint64_t SMALL_COUNT_512KB = 512*1024; // Byte, UB协议一次传输的最大size
constexpr uint64_t LARGE_COUNT_1024KB = 1024*1024; // Byte, 可掩盖多mission尾块开销
class AutoSelectorBase : public BaseSelector {
public:
    SelectorStatus Select(const CollAlgOperator &op, CollAlgParams &params,
                          std::string &primQueueGenName) override;
    bool IsDefaultAlg(const HcclAlgoType algoType) const;
    bool IsSmallData(const u64 dataSize) const;
    bool IsLargeData(const u64 dataSize) const;
    virtual SelectorStatus SelectCcuMsAlgo(const TopoInfo &topoInfo,
                                 const CollAlgOperator &op,
                                 const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                 std::string &primQueueGenName) const;
    virtual SelectorStatus SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                 const CollAlgOperator &op,
                                 const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                 std::string &primQueueGenName) const;
    virtual SelectorStatus SelectAicpuAlgo(const TopoInfo &topoInfo,
                                   const CollAlgOperator &op,
                                   const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                   std::string &primQueueGenName) const;
    virtual SelectorStatus SelectAivAlgo(const TopoInfo &topoInfo,
                                   const CollAlgOperator &op,
                                   const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                   std::string &primQueueGenName) const;
    bool IsStarsState(const OpExecuteConfig &opExecuteConfig) const;
protected:
    u64 dataSize_;
};

} // namespace Hccl
#endif