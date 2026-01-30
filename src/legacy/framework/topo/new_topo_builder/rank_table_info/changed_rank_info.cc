/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: NewRankTable header file
 * Create: 2025-6-12
 */
#include "changed_rank_info.h"
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include "sal.h"
#include "json_parser.h"
#include "invalid_params_exception.h"
#include "orion_adapter_rts.h"
#include "exception_util.h"
#include "log.h"

namespace Hccl {

std::string ChangedRankInfo::Describe() const
{
    return StringFormat("ChangedRankInfo[version=%s, rankCount=%u, ranks size=%d]", version.c_str(), rankCount,
                        ranks.size());
}

void ChangedRankInfo::Dump() const
{
    HCCL_DEBUG("ChangedRankInfo Dump:");
    HCCL_DEBUG("%s", Describe().c_str());
    HCCL_DEBUG("ranks:");
    for (const auto& rank : ranks) {
        HCCL_DEBUG(rank.Describe().c_str());
        for (const auto& levelInfo : rank.rankLevelInfos) {
            HCCL_DEBUG("    %s", levelInfo.Describe().c_str());
        }
    }
}

constexpr int HCCL_DECIMAL = 10;
void ChangedRankInfo::Deserialize(const nlohmann::json &changedRankInfoJson)
{
    std::string msgVersion   = "[ChangedRankInfo] error occurs when parser object of propName \"version\"";
    std::string msgRankcount = "[ChangedRankInfo] error occurs when parser object of propName \"rank_count\"";
    TRY_CATCH_THROW(InvalidParamsException, msgVersion, version = GetJsonProperty(changedRankInfoJson, "version"););
    TRY_CATCH_THROW(InvalidParamsException, msgRankcount, rankCount = GetJsonPropertyUInt(changedRankInfoJson, "rank_count"););

    nlohmann::json rankJsons;
    std::string    msgRanklist = "error occurs when parser object of propName \"rank_list\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRanklist,
                         GetJsonPropertyList(changedRankInfoJson, "rank_list", rankJsons););
    for (auto &rankJson : rankJsons) {
        NewRankInfo rankInfo;
        rankInfo.Deserialize(rankJson);
        ranks.emplace_back(rankInfo);
    }
}
} // namespace Hccl