/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: RankTableInfo implement
 * Create: 2024-12-16
 */

#include "rank_table_info.h"

#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <string>
#include "sal.h"
#include "json_parser.h"
#include "invalid_params_exception.h"
#include "types.h"
#include "const_val.h"
#include "dev_type.h"
#include "exception_util.h"

namespace Hccl {

void RankTableInfo::Check()
{
    if (version != "2.0") {
        HCCL_ERROR("[RankTableInfo::%s] failed with version [%s] is not \"2.0\".", __func__ , version.c_str());
        THROW<InvalidParamsException>(
            StringFormat("[RankTableInfo::%s] failed with version is not \"2.0\" in ranktable file.", __func__));
    }

    if (rankCount > MAX_RANKCOUNT) {
        THROW<InvalidParamsException>(StringFormat(
            "[RankTableInfo::%s] failed with rankCount [%u] exceeds maximum limit of [%u]",
            __func__, rankCount, MAX_RANKCOUNT));
    }

    if (rankCount == 0) {
        THROW<InvalidParamsException>(StringFormat(
            "[RankTableInfo::%s] failed with rankCount [%u] exceeds minimum limit of [%u]",__func__, rankCount, 0));
    }

    if (rankCount != ranks.size()) {
        THROW<InvalidParamsException>(StringFormat("[RankTableInfo::%s] failed with rankCount is not equal "
                                                   "to rank_list size. version[%s], rankCount[%u], ranks.size[%u]",
                                                   __func__, version.c_str(), rankCount, ranks.size()));
    }

    std::unordered_set<u32> rankIdSet;
    std::unordered_set<u32> localIdSet;
    u32 recordedReplaceLocalId{UNDEFIEND_LOCAL_ID};
    for (auto &rank : ranks) {
        if (static_cast<u32>(rank.rankId) >= rankCount) {
            THROW<InvalidParamsException>(StringFormat("[Parse][ClusterInfo][RankTableInfo::%s] failed with rank_id is "
                                                       "out of range. version[%s], rankCount[%u], rank_id[%d]",
                                                       __func__, version.c_str(), rankCount, rank.rankId));
        }
        if (rankIdSet.count(rank.rankId) > 0) {
            THROW<InvalidParamsException>(StringFormat("[Parse][ClusterInfo][RankTableInfo::%s] failed with rank_id is "
                                                       "repeat. version[%s], rankCount[%u], rank_id[%d]",
                                                       __func__, version.c_str(), rankCount, rank.rankId));
        }
        rankIdSet.insert(rank.rankId);

        if (rank.localId != BACKUP_LOCAL_ID && rank.localId != rank.replacedLocalId) {
            THROW<InvalidParamsException>(StringFormat("[Parse][ClusterInfo][RankTableInfo::Check] "
            "failed with replacedLocalId[%u] not equal to localId[%u].", rank.replacedLocalId, rank.localId));
        } else if (rank.localId == BACKUP_LOCAL_ID) {
            if (recordedReplaceLocalId == UNDEFIEND_LOCAL_ID) {
                recordedReplaceLocalId = rank.replacedLocalId;
            } else {
                THROW<InvalidParamsException>(StringFormat("[Parse][ClusterInfo][RankTableInfo::Check] "
                                                           "multiple replaced rank is configured"));
            }
        } else {
            localIdSet.emplace(rank.localId);
        }
    }

    for (u32 rankRange = 0; rankRange < rankCount; rankRange++) {
        if (rankIdSet.find(rankRange) == rankIdSet.end()) {
            THROW<InvalidParamsException>(StringFormat("[Parse][ClusterInfo][RankTableInfo::%s] failed with rank_id is "
                                                       "not continuous. version[%s], rankCount[%u], rankRange[%d]",
                                                           __func__, version.c_str(), rankCount, rankRange));
        }
    }

    std::vector<std::unordered_map<std::string, u32>> verifyRankAddr;
    for (auto &rank : ranks) {
        for (auto &levelInfo : rank.rankLevelInfos) {
            InsertToRank(levelInfo.netInstId, levelInfo.rankAddrs.size(), verifyRankAddr, levelInfo.netLayer);
        }
    }

    if(localIdSet.find(recordedReplaceLocalId) != localIdSet.end()) {
        THROW<InvalidParamsException>(StringFormat("[Parse][ClusterInfo][RankTableInfo::%s] failed with configuring "
                                                   "same local_id[%u] with replaced one simutaneously",
                                                    __func__, recordedReplaceLocalId));
    }
}

constexpr int HCCL_DECIMAL = 10;
void RankTableInfo::Deserialize(const nlohmann::json &rankTableInfoJson, bool isCheck)
{
    std::string msgVersion   = "error occurs when parser object of propName \"version\"";
    TRY_CATCH_THROW(InvalidParamsException, msgVersion, version = GetJsonProperty(rankTableInfoJson, "version"););
    std::string msgStatus    = "error occurs when parser object of propName \"status\"";

    std::string detourStr;
    std::string msgDetour   = "error occurs when parser object of propName \"detour\"";
    TRY_CATCH_THROW(InvalidParamsException, msgDetour, detourStr = GetJsonProperty(rankTableInfoJson, "detour", false););
    if (detourStr == "true") {
        detour = true;
    } else if (detourStr == "false" || detourStr == "") {
        detour = false;
    } else {
        THROW<InvalidParamsException>(StringFormat("Invalid detour value [%s]", detourStr.c_str()));
    }

    std::string msgRankcount = "error occurs when parser object of propName \"rank_count\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRankcount, rankCount = GetJsonPropertyUInt(rankTableInfoJson, "rank_count"););

    nlohmann::json rankJsons;
    std::string    msgRanklist = "error occurs when parser object of propName \"rank_list\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRanklist,
                         GetJsonPropertyList(rankTableInfoJson, "rank_list", rankJsons););
    for (auto &rankJson : rankJsons) {
        NewRankInfo rankInfo;
        rankInfo.Deserialize(rankJson);
        ranks.emplace_back(rankInfo);
    }
   
    // check
    if (isCheck) {
        Check();
    }
}

void RankTableInfo::CheckAndInsert(const std::string &levelId, u32 rankAddrSize,
                                   std::unordered_map<std::string, u32> &idRankSizeMap) const
{
    if (idRankSizeMap.find(levelId) != idRankSizeMap.end() && idRankSizeMap[levelId] != rankAddrSize) {
        THROW<InvalidParamsException>(StringFormat("[RankTableInfo::%s] failed with the size of "
                                                   "rank_addrs with the same id is different. leveId[%s],"
                                                   "rankAddrSize[%u]",
                                                   __func__, levelId.c_str(), rankAddrSize));
    }
    idRankSizeMap[levelId] = rankAddrSize;
}

void RankTableInfo::InsertToRank(const std::string &levelId, u32 rankAddrSize,
                                 std::vector<std::unordered_map<std::string, u32>> &rankLists, u32 levelNum) const
{
    if (rankLists.size() <= levelNum) {
        rankLists.resize(levelNum + 1);
    }
    CheckAndInsert(levelId, rankAddrSize, rankLists[levelNum]);
}

std::string RankTableInfo::Describe() const
{
    return StringFormat("RankTableInfo[version=%s, rankCount=%u, ranks size=%d]", version.c_str(), rankCount,
                        ranks.size());
}

void RankTableInfo::Dump() const
{
    HCCL_DEBUG("RankTableInfo Dump:");
    HCCL_DEBUG("%s", Describe().c_str());
    HCCL_DEBUG("ranks:");
    for (const auto& rank : ranks) {
        HCCL_DEBUG("%s", rank.Describe().c_str());
        for (const auto& levelInfo : rank.rankLevelInfos) {
            HCCL_DEBUG("    %s", levelInfo.Describe().c_str());
        }
    }
}

RankTableInfo::RankTableInfo(BinaryStream& binaryStream){
    binaryStream >> version >> rankCount;
    size_t ranksSize = 0;
    binaryStream >> ranksSize;
    HCCL_INFO("[%s] version[%s] rankCount[%u] ranks size[%u]", __func__, version.c_str(), rankCount, ranksSize);
    for(u32 i = 0; i < ranksSize; i++){
        NewRankInfo rankInfo(binaryStream);
        ranks.emplace_back(rankInfo);
    }
    binaryStream>>detour;
}

void RankTableInfo::GetBinStream(bool isContainLocId, BinaryStream& binaryStream) const{
    if(ranks.size() == 0) {
        std::string msg = StringFormat("ranks size is zero.");
        THROW<InvalidParamsException>(msg);
    }
    HCCL_INFO("[%s] version[%s] rankCount[%u] ranks size[%u]", __func__, version.c_str(), rankCount, ranks.size());

    binaryStream << version  << rankCount;
    binaryStream << ranks.size();
    for(auto& it: ranks){
        it.GetBinStream(isContainLocId, binaryStream);
    }
    binaryStream<<detour;
}

vector<char> RankTableInfo::GetUniqueId(bool isContainLocId) const
{
    if(ranks.size() == 0) {
        std::string msg = StringFormat("ranks size is zero.");
        THROW<InvalidParamsException>(msg);
    }
    std::vector<char> result(0);

    BinaryStream binaryStream;
    binaryStream << version << rankCount;

    u32 ranksSize = ranks.size();
    binaryStream << ranksSize;
    for(auto& it: ranks) {
        it.GetBinStream(isContainLocId, binaryStream);
    }

    binaryStream.Dump(result);
    return result; 
}

void RankTableInfo::UpdateRankTable(const RankTableInfo &localRankInfo)
{
    // version
    if (rankCount == 0) {
        version = localRankInfo.version;
    } else {
        CHK_PRT_THROW(version != localRankInfo.version, 
            HCCL_ERROR("[%s] version[%s] error, local version[%s] .", __func__, version.c_str(), localRankInfo.version.c_str()), 
            InvalidParamsException, "updateRankTableInfo error");
    }

    // ranks size
    CHK_PRT_THROW(localRankInfo.ranks.size() == 0, HCCL_ERROR("[%s] ranks size is zero.", __func__), 
            InvalidParamsException, "updateRankTableInfo error");

    ranks.insert(ranks.end(), localRankInfo.ranks.begin(), localRankInfo.ranks.end());
    rankCount++;

    HCCL_INFO("[%s] success, current rankTableInfo[%s]", __func__, Describe().c_str());
}

} // namespace Hccl