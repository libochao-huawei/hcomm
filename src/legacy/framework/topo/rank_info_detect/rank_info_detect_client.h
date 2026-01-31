/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: rank info detect declaration
 * Create: 2025-09-16
 */

#ifndef HCCLV2_RANK_INFO_DETECT_CLIENT_H
#define HCCLV2_RANK_INFO_DETECT_CLIENT_H

#include "socket.h"
#include "new_rank_info.h"
#include "rank_table_info.h"
#include "json_parser.h"
#include "internal_exception.h"
#include "timeout_exception.h"
#include "socket_exception.h"
#include "socket_agent.h"
#include "root_handle_v2.h"

namespace Hccl {

class RankInfoDetectClient {
public:
    RankInfoDetectClient(u32 devPhyId, u32 rankSize, u32 rankId, const std::shared_ptr<Socket> &clientSocket)
        : devPhyId_(devPhyId), rankSize_(rankSize), rankId_(rankId), clientSocket_(clientSocket), socketAgent_(clientSocket.get())
    {
    }
    ~RankInfoDetectClient();

    void Setup(RankTableInfo &rankTable);

private:
    u32                             devPhyId_{0};
    u32                             rankSize_{0};
    u32                             rankId_{0};
    std::shared_ptr<Socket>         clientSocket_{nullptr};
    u32                             currentStep_{0};
    RankTableInfo                   rankTable_{};
    SocketAgent                     socketAgent_;
    
    void Connect();
    void CheckStatus();
    void SendAgentIdAndRankSize();
    void SendLocalRankTable(const RankTableInfo &localRankTable);
    void ConstructRankTable(RankTableInfo &localRankTable);
    void VerifyRankTable();
    void RecvRankTable();
    void RecvRankTableMsg(vector<char> &rankInfoMsg);
    void ParseRankTable(vector<char> &rankInfoMsg);
    void GetLocalRankTableJson(const nlohmann::json &parseJson, nlohmann::json &localRankTableJson);
    void GetLocalDevInfoJson(const nlohmann::json &parseJson, nlohmann::json &localDevInfoJson);
    void ConstructSingleRank(RankTableInfo &localRankTable);
    void TearDown();
};

} // namespace Hccl
#endif // HCCLV2_RANK_INFO_DETECT_CLIENT_H
