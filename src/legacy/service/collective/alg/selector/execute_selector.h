/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: selector执行器文件头
 * Author: yinding
 * Create: 2024-02-04
 */
#ifndef HCCLV2_COLL_ALG_SELECTOR_EXECUTION
#define HCCLV2_COLL_ALG_SELECTOR_EXECUTION

#include <string>

#include "types.h"
#include "dev_type.h"
#include "rank_gph.h"
#include "coll_alg_params.h"
#include "coll_operator.h"

namespace Hccl {
class ExecuteSelector {
public:
    ExecuteSelector &SetVirtualTopo(RankGraph *rankGraph);
    ExecuteSelector &SetDevType(DevType devType);
    ExecuteSelector &SetMyRank(RankId myRank);
    ExecuteSelector &SetRankSize(u32 rankSize);
    ExecuteSelector &SetSeverId(std::string severId);
    ExecuteSelector &SetDeviceNumPerSever(u32 deviceNumPerSever);
    ExecuteSelector &SetServerNum(u32 serverNum);
    ExecuteSelector &SetOpConfig(OpExecuteConfig opConfig);
    HcclResult       Run(const CollAlgOperator &op, CollAlgParams &params, std::string &primQueueGenName);

protected:
    RankGraph *rankGraph_ = nullptr;
    OpExecuteConfig opConfig_;
    DevType      devType_;
    u32          myRank_;
    u32          rankSize_;
    std::string  severId_;
    u32          deviceNumPerSever_;
    u32          serverNum_;
};
} // namespace Hccl
#endif