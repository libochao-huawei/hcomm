/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: RankInfo header file
 * Create: 2024-12-16
 */

#ifndef NEW_RANK_INFO_H
#define NEW_RANK_INFO_H

#include <vector>
#include <string>

#include "types.h"
#include "nlohmann/json.hpp"
#include "rank_level_info.h"
#include "control_plane.h"
namespace Hccl {
constexpr unsigned int MAX_VALUE_DEVICEID = 64;
constexpr unsigned int MAX_VALUE_DEVICEPORT = 65536;   
constexpr unsigned int MIN_VALUE_DEVICEPORT = 1;
constexpr unsigned int MAX_LEVEL_lIST  = 8; 
class NewRankInfo{
public:
    NewRankInfo() {};
    ~NewRankInfo() {};
    
    u32                        rankId{0};
    u32                        deviceId{0};
    u32                        localId{0};
    u32                        replacedLocalId{0};
    u32                        devicePort{MAX_VALUE_DEVICEPORT};
    std::vector<RankLevelInfo> rankLevelInfos{};
    ControlPlane               controlPlane{};
    std::string                Describe() const;
    void                       Deserialize(const nlohmann::json &newRankInfoJson);
    explicit                   NewRankInfo(BinaryStream &binStream);
    void GetBinStream(bool isContainLoaId, BinaryStream &binStream) const;
};

} // namespace Hccl

#endif // NEW_RANK_INFO_H