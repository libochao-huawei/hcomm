/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef LAYERED_BASE_PUB_H
#define LAYERED_BASE_PUB_H

#include <memory>
#include <vector>

#include "alg_template_base_pub.h"
#include "coll_alg_param.h"
#include "coll_native_executor_base.h"
#include "comm_utils.h"
#include "topo_matcher.h"

namespace hccl {

struct LayeredParam {
    u64 baseOffset = 0;
    DeviceMem inputMem;
    DeviceMem outputMem;
    DeviceMem scratchMem;

    AlgType algType;
    HcclAlgoInfo algoAttr;
    HcclTopoInfo topoAttr;

    // Current HcclAlgoInfo does not carry OXC layered inter algorithm selection. Keep the
    // prep-slice adaptation local instead of widening TopoMatcher/HcclAlgoInfo contracts.
    HcclAlgoType interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;

    u64 count = 0;

    std::vector<Slice> outerDataSlices;
    std::vector<Slice> innerDataSlices;
    std::vector<Slice> crossDataSlices;
    SubCommInfo outerCommInfo;
    SubCommInfo innerCommInfo;
    SubCommInfo crossCommInfo;
    AlgResourceResponse *algRes = nullptr;
};

class LayeredBase : public ExecutorBase {
public:
    explicit LayeredBase(const HcclDispatcher dispatcher);
    ~LayeredBase() override;

    static bool IsSupportLayered(u32 groupSize, AlgType algType);
    static bool IsSupportAlgType(AlgType algType);
    static CommType GetLevel1CommType(const HcclAlgoInfo &algoInfo, AlgType algType);
    static CommType GetLevel2CommType(const HcclAlgoInfo &algoInfo, AlgType algType);
    // Prefer this overload for OXC layered level2 selection; HcclAlgoInfo intentionally has no interAlgType.
    static CommType GetLevel2CommType(HcclAlgoType interAlgType);

    virtual HcclResult Prepare(const OpParam &opParam, ExecMem &execMem, const LayeredParam &layeredParam);

protected:
    std::shared_ptr<LayeredParam> layeredParam_;

    HcclAlgoInfo algoAttr_;
    HcclTopoInfo topoAttr_;
    AlgType algType_;
    HcclAlgoType interAlgType_ = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    Stream stream_;

    u64 count_ = 0;
    u32 dataUnit_ = 0;
    HcclDataType dataType_ = HCCL_DATA_TYPE_RESERVED;
    HcclReduceOp reduceType_ = HCCL_REDUCE_RESERVED;

    u64 baseOffset_ = 0;
    DeviceMem inputMem_;
    DeviceMem outputMem_;
    DeviceMem scratchMem_;

    u32 outerCommRank_ = 0;
    u32 outerCommSize_ = 0;
    SubCommInfo outerCommInfo_;

    u32 innerCommRank_ = 0;
    u32 innerCommSize_ = 0;
    SubCommInfo innerCommInfo_;

    u32 crossCommRank_ = 0;
    u32 crossCommSize_ = 0;
    SubCommInfo crossCommInfo_;

    std::vector<Slice> outerDataSlices_;
    std::vector<Slice> innerDataSlices_;
    std::vector<Slice> crossDataSlices_;

    Slice innerSlice_;
    u64 innerCount_ = 0;
    u64 innerBytes_ = 0;
    u64 innerOffset_ = 0;
    DeviceMem innerCclIn_;
    DeviceMem innerCclOut_;

    Slice crossSlice_;
    u64 crossCount_ = 0;
    u64 crossBytes_ = 0;
    u64 crossOffset_ = 0;
    DeviceMem crossCclIn_;
    DeviceMem crossCclOut_;
};
}  // namespace hccl

#endif /* LAYERED_BASE_PUB_H */
