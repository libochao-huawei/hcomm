/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_RANK_INFO_DETECT_CLIENT_H
#define HCCLV2_RANK_INFO_DETECT_CLIENT_H

#include "socket.h"
#include "new_rank_info.h"
#include "rank_table_info.h"
#include "json_parser.h"
#include "socket_agent.h"
#include "root_handle_v2.h"

namespace Hccl {

const u32 RANKINFO_DETECT_AGENT_WAIT_ERROR_BROADCAST_TIME = 20;

class RankInfoDetectClient {
public:
    RankInfoDetectClient(u32 devPhyId, u32 rankSize, u32 rankId, const std::shared_ptr<Socket> &clientSocket)
        : devPhyId_(devPhyId), rankSize_(rankSize), rankId_(rankId), clientSocket_(clientSocket), socketAgent_(clientSocket.get())
    {
    }
    ~RankInfoDetectClient();

    HcclResult Setup(RankTableInfo &rankTable, u32 hostPort);

private:
    u32                             devPhyId_{0};
    u32                             rankSize_{0};
    u32                             rankId_{0};
    std::shared_ptr<Socket>         clientSocket_{nullptr};
    u32                             currentStep_{0};
    RankTableInfo                   rankTable_{};
    SocketAgent                     socketAgent_;

    HcclResult Connect();
    HcclResult CheckStatus();
    HcclResult SendAgentIdAndRankSize();
    HcclResult SendLocalRankTable(const RankTableInfo &localRankTable);
    HcclResult ConstructRankTable(RankTableInfo &localRankTable);
    HcclResult VerifyRankTable();
    HcclResult RecvRankTable();
    HcclResult RecvRankTableMsg(vector<char> &rankInfoMsg);
    HcclResult ParseRankTable(vector<char> &rankInfoMsg);
    HcclResult GetLocalRankTableJson(const nlohmann::json &parseJson, nlohmann::json &localRankTableJson);
    HcclResult GetLocalDevInfoJson(const nlohmann::json &parseJson, nlohmann::json &localDevInfoJson);
    HcclResult ConstructSingleRank(RankTableInfo &localRankTable);
    HcclResult GetLocalTlsStatus(TlsStatus &tlsStatus) const;
    HcclResult VerifyTlsConsistency() const;
    void GenerateTlsStatusStr(std::string &tlsStatusStr, const std::vector<u32> &tlsStatusRanks) const;
    void ReportTlsConfigurationError(
        const std::string &tlsInconsistentTlsType, const std::string &tlsEnableRankStr,
        const std::string &tlsDisableRankStr, const std::string &tlsUnknownRankStr) const;
    HcclResult TearDown();
};

} // namespace Hccl
#endif // HCCLV2_RANK_INFO_DETECT_CLIENT_H
