/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: scatter自适应算法选择文件头
 * Author: libiaozhi
 * Create: 2025-04-19
 */
#ifndef HCCLV2_SCATTER_AUTO_SELECTOR
#define HCCLV2_SCATTER_AUTO_SELECTOR

#include "auto_selector_base.h"
#include "coll_alg_params.h"
#include "coll_operator.h"
#include "virtual_topo.h"

namespace Hccl {

class ScatterAutoSelector : public AutoSelectorBase {
private:
    SelectorStatus SelectCcuMsAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
                                 const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                 std::string                                 &primQueueGenName) const override;
    SelectorStatus SelectCcuScheduleAlgo(const TopoInfo &topoInfo,
                                 const CollAlgOperator &op,
                                 const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                 std::string &primQueueGenName) const override;
    SelectorStatus SelectAicpuAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op,
                                   const std::map<OpType, std::vector<HcclAlgoType>> &configAlgMap,
                                   std::string                                 &primQueueGenName) const override;
    SelectorStatus SelectAivAlgo(const TopoInfo &topoInfo, const CollAlgOperator &op, const std::map<OpType, 
                                 std::vector<HcclAlgoType>> &configAlgMap, std::string &primQueueGenName) const override;
};

} // namespace Hccl
#endif