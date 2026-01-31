/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: NewRankTable header file
 * Create: 2025-6-12
 */

#ifndef CHANGED_RANK_INFO_H
#define CHANGED_RANK_INFO_H

#include <vector>
#include <string>
#include <unordered_map>
#include "nlohmann/json.hpp"
#include "new_rank_info.h"
#include "invalid_params_exception.h"
#include "orion_adapter_rts.h"
#include "exception_util.h"

namespace Hccl {
class ChangedRankInfo {
public:
    ChangedRankInfo(){};
    std::string              version;
    u32                      rankCount{0};
    std::vector<NewRankInfo> ranks;
    void                     Dump() const;
    std::string              Describe() const;
    void                     Deserialize(const nlohmann::json &changedRankInfoJson);
};

} // namespace Hccl

#endif //CHANGED_RANK_INFO_H
