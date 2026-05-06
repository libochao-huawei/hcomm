/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"

namespace {
class ScopedStubLogLevel {
public:
    explicit ScopedStubLogLevel(s32 logLevel) : previousLevel_(log_level_get_stub())
    {
        log_level_set_stub(logLevel);
    }

    ~ScopedStubLogLevel()
    {
        log_level_set_stub(previousLevel_);
    }

private:
    s32 previousLevel_;
};

std::string CaptureCfgGetClusterInfoStdout(const std::string &rankTable, const std::string &identify,
    HcclCommParams &params, RankTable_t &rankTableInfo)
{
    testing::internal::CaptureStdout();
    HcclResult ret = CfgGetClusterInfo(rankTable, identify, params, rankTableInfo);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    return output;
}

nlohmann::json BuildRankRef(const std::string &rankId)
{
    return nlohmann::json::object({{"rank_id", rankId}});
}

nlohmann::json BuildDevice(const std::string &deviceId, const std::string &superDeviceId, const std::string &rankId,
    const std::string &deviceIp, const std::string &backupDeviceIp = std::string(),
    int devicePort = -1, int backupDevicePort = -1)
{
    nlohmann::json device = nlohmann::json::object({
        {"device_id", deviceId},
        {"super_device_id", superDeviceId},
        {"rankid", rankId}
    });
    if (!deviceIp.empty()) {
        device["device_ip"] = deviceIp;
    }
    if (!backupDeviceIp.empty()) {
        device["backup_device_ip"] = backupDeviceIp;
    }
    if (devicePort >= 0) {
        device["device_port"] = devicePort;
    }
    if (backupDevicePort >= 0) {
        device["backup_device_port"] = backupDevicePort;
    }
    return device;
}

nlohmann::json BuildServer(const std::string &serverId, const std::string &hostIp, const nlohmann::json &devices)
{
    nlohmann::json server = nlohmann::json::object({
        {"server_id", serverId},
        {"device", devices}
    });
    if (!hostIp.empty()) {
        server["host_ip"] = hostIp;
    }
    return server;
}

nlohmann::json BuildOxcRankTable(bool missingServerList = false, bool missingGroupList = false,
    bool duplicateRankId = false, bool missingGroupCoverage = false, bool duplicateGroupedRank = false)
{
    nlohmann::json devices = nlohmann::json::array();
    devices.push_back(BuildDevice("0", "0", "0", "10.10.0.10"));
    devices.push_back(BuildDevice("1", "1", duplicateRankId ? "0" : "1", "10.10.0.11"));

    nlohmann::json rankTable = nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "oxc_task_ut_001"},
        {"super_pod_list", nlohmann::json::array({
            nlohmann::json::object({
                {"super_pod_id", "pod0"},
                {"server_list", nlohmann::json::array({nlohmann::json::object({{"server_id", "server0"}})})}
            })
        })}
    });

    if (!missingServerList) {
        rankTable["server_list"] = nlohmann::json::array({BuildServer("server0", "192.168.10.10", devices)});
    }

    if (!missingGroupList) {
        nlohmann::json groupRanks = nlohmann::json::array({BuildRankRef("0")});
        if (!missingGroupCoverage) {
            groupRanks.push_back(BuildRankRef(duplicateGroupedRank ? "0" : "1"));
        }
        rankTable["oxc_group_list"] = nlohmann::json::array({
            nlohmann::json::object({{"group_id", "group0"}, {"rank_list", groupRanks}})
        });
    }

    return rankTable;
}

nlohmann::json BuildLegacyOxc2p0RankTable()
{
    return nlohmann::json::object({
        {"status", "completed"},
        {"version", "2.0"},
        {"task_id", "legacy_oxc_2_0"},
        {"rank_count", 1},
        {"rank_list", nlohmann::json::array({
            nlohmann::json::object({
                {"rank_id", 0},
                {"local_id", 0},
                {"device_id", 0},
                {"server_id", "server0"},
                {"level_list", nlohmann::json::array()}
            })
        })}
    });
}

nlohmann::json BuildOxcA3RankTable(bool missingSuperPodList = false, bool missingSuperDeviceId = false,
    bool duplicateSuperDeviceId = false, bool missingDeviceIp = false, bool invalidDevicePort = false,
    bool missingBackupDeviceIp = false, bool missingBackupDevicePort = false)
{
    auto buildServer = [&](const std::string &serverId, const std::string &hostIp,
                           const std::string &superPodId, const std::string &rankId,
                           const std::string &superDeviceId, const std::string &deviceIp,
                           const std::string &backupDeviceIp, int devicePort, int backupDevicePort) {
        nlohmann::json device = BuildDevice("0", superDeviceId, rankId, deviceIp, backupDeviceIp,
            invalidDevicePort ? -1 : devicePort, missingBackupDevicePort ? -1 : backupDevicePort);
        if (invalidDevicePort) {
            device["device_port"] = "invalid_port";
        }
        if (missingSuperDeviceId) {
            device.erase("super_device_id");
        }
        if (missingDeviceIp) {
            device.erase("device_ip");
        }
        if (missingBackupDeviceIp) {
            device.erase("backup_device_ip");
        }
        return std::make_pair(
            BuildServer(serverId, hostIp, nlohmann::json::array({device})),
            nlohmann::json::object({
                {"super_pod_id", superPodId},
                {"server_list", nlohmann::json::array({nlohmann::json::object({{"server_id", serverId}})})}
            })
        );
    };

    auto server0 = buildServer("server0", "192.168.10.10", "pod0", "0", "100", "10.10.0.10",
        "10.20.0.10", 16666, 17666);
    auto server1 = buildServer("server1", "192.168.10.11", "pod1", "1",
        duplicateSuperDeviceId ? "100" : "200", "10.10.0.11", "10.20.0.11", 16667, 17667);

    nlohmann::json rankTable = nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "oxc_a3_ranktable_ut"},
        {"server_list", nlohmann::json::array({server0.first, server1.first})},
        {"oxc_group_list", nlohmann::json::array({
            nlohmann::json::object({{"group_id", "group0"}, {"rank_list", nlohmann::json::array({BuildRankRef("0")})}}),
            nlohmann::json::object({{"group_id", "group1"}, {"rank_list", nlohmann::json::array({BuildRankRef("1")})}})
        })}
    });

    if (!missingSuperPodList) {
        rankTable["super_pod_list"] = nlohmann::json::array({server0.second, server1.second});
    }
    return rankTable;
}

nlohmann::json BuildSingleServerOxcA3RankTable()
{
    nlohmann::json rankTable = BuildOxcRankTable();
    rankTable["server_list"][0]["device"][0]["super_device_id"] = "100";
    rankTable["server_list"][0]["device"][1]["super_device_id"] = "101";
    rankTable["super_pod_list"][0]["super_pod_id"] = "pod0";
    return rankTable;
}

nlohmann::json BuildOxcNetPlaneRankTable()
{
    auto buildServerWithTwoRanks = [](const std::string &serverId, const std::string &hostIp,
                                      const std::string &rankId0, const std::string &rankId1,
                                      const std::string &sdid0, const std::string &sdid1) {
        nlohmann::json devices = nlohmann::json::array();
        devices.push_back(BuildDevice("0", sdid0, rankId0, std::string("10.10.0.") + std::to_string(10 + std::stoi(rankId0))));
        devices.push_back(BuildDevice("1", sdid1, rankId1, std::string("10.10.0.") + std::to_string(10 + std::stoi(rankId1))));
        return BuildServer(serverId, hostIp, devices);
    };

    nlohmann::json serverList = nlohmann::json::array();
    serverList.push_back(buildServerWithTwoRanks("server0", "192.168.10.10", "0", "1", "100", "101"));
    serverList.push_back(buildServerWithTwoRanks("server1", "192.168.10.11", "2", "3", "102", "103"));
    serverList.push_back(buildServerWithTwoRanks("server2", "192.168.10.12", "4", "5", "200", "201"));
    serverList.push_back(buildServerWithTwoRanks("server3", "192.168.10.13", "6", "7", "202", "203"));

    nlohmann::json pod0Servers = nlohmann::json::array();
    pod0Servers.push_back(nlohmann::json::object({{"server_id", "server0"}}));
    pod0Servers.push_back(nlohmann::json::object({{"server_id", "server1"}}));
    nlohmann::json pod1Servers = nlohmann::json::array();
    pod1Servers.push_back(nlohmann::json::object({{"server_id", "server2"}}));
    pod1Servers.push_back(nlohmann::json::object({{"server_id", "server3"}}));

    nlohmann::json group0Ranks = nlohmann::json::array();
    group0Ranks.push_back(BuildRankRef("0"));
    group0Ranks.push_back(BuildRankRef("1"));
    group0Ranks.push_back(BuildRankRef("2"));
    group0Ranks.push_back(BuildRankRef("3"));
    nlohmann::json group1Ranks = nlohmann::json::array();
    group1Ranks.push_back(BuildRankRef("4"));
    group1Ranks.push_back(BuildRankRef("5"));
    group1Ranks.push_back(BuildRankRef("6"));
    group1Ranks.push_back(BuildRankRef("7"));

    return nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "oxc_comm_init_a2_ut"},
        {"server_list", serverList},
        {"super_pod_list", nlohmann::json::array({
            nlohmann::json::object({{"super_pod_id", "pod0"}, {"server_list", pod0Servers}}),
            nlohmann::json::object({{"super_pod_id", "pod1"}, {"server_list", pod1Servers}})
        })},
        {"oxc_group_list", nlohmann::json::array({
            nlohmann::json::object({{"group_id", "group0"}, {"rank_list", group0Ranks}}),
            nlohmann::json::object({{"group_id", "group1"}, {"rank_list", group1Ranks}})
        })}
    });
}

HcclResult HrtGetDeviceA3Stub(s32 *deviceLogicId)
{
    CHK_PTR_NULL(deviceLogicId);
    *deviceLogicId = 0;
    return HCCL_SUCCESS;
}

class HcclCommInitClusterInfoOxcTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        SetIfProfile(false);
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

class HcclCommInitClusterInfoOxcA3Test : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        SetIfProfile(false);
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDevice).defaults().will(invoke(HrtGetDeviceA3Stub));
        uint32_t devicePhyId = 0;
        MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), outBound(devicePhyId)).will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};
} // namespace

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_OxcRankTable1_4_Expect_ParseSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(rankTable.taskId, std::string("oxc_task_ut_001"));
    EXPECT_EQ(rankTable.rankNum, 2U);
    EXPECT_EQ(rankTable.deviceNum, 2U);
    EXPECT_EQ(rankTable.serverNum, 1U);
    EXPECT_EQ(rankTable.superPodNum, 1U);
    EXPECT_EQ(params.rank, 0U);
    EXPECT_EQ(params.userRank, 0U);
    EXPECT_EQ(params.totalRanks, 2U);
    EXPECT_EQ(params.serverId, std::string("server0"));
    EXPECT_EQ(rankTable.rankList[0].serverId, std::string("server0"));
    EXPECT_EQ(rankTable.rankList[0].superPodId, std::string("pod0"));
    EXPECT_EQ(rankTable.rankList[0].oxcGroupId, std::string("group0"));
    ASSERT_EQ(rankTable.rankList[0].levelList.size(), 4U);
    EXPECT_EQ(rankTable.rankList[0].levelList[1].netInstanceId, std::string("group0"));
    EXPECT_EQ(rankTable.rankList[0].levelList[2].netType, std::string("OXC_Mesh"));
    EXPECT_EQ(rankTable.rankList[0].levelList[2].rankAddrList[0].planeId, std::string("group0"));
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfoWithoutDev_When_OxcRankTable1_4_Expect_ParseSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();

    HcclResult ret = CfgGetClusterInfoWithoutDev(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(rankTable.taskId, std::string("oxc_task_ut_001"));
    EXPECT_EQ(params.totalRanks, 2U);
    EXPECT_EQ(rankTable.rankList[1].oxcGroupId, std::string("group0"));
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_ServerListMissing_Expect_ReturnIsNotSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(true, false);

    EXPECT_NE(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_SUCCESS);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_OxcGroupListMissing_Expect_ReturnIsNotSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(false, true);

    EXPECT_NE(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_SUCCESS);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_RankIdDuplicated_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(false, false, true);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_OxcGroupCoverageMissing_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(false, false, false, true);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_OxcGroupHasDuplicatedRank_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(false, false, false, false, true);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_LegacyOxcRankTable2_0_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    HcclCommParams params;
    RankTable_t rankTable;
    EXPECT_EQ(CfgGetClusterInfo(BuildLegacyOxc2p0RankTable().dump(), "0", params, rankTable), HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclCommInitClusterInfoOxcTest,
    Ut_InjectWorldCommNetPlaneInfo_When_OxcRankTable1_4_Expect_RuntimeCommConfigCarriesCurrentRankPlane)
{
    RankTable_t rankTable;
    HcclCommParams params;
    nlohmann::json rankTableJson = BuildOxcNetPlaneRankTable();
    ASSERT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_SUCCESS);

    CommConfig runtimeCommConfig("ut_world_netplane");
    HcclResult ret = InjectWorldCommNetPlaneInfo(0, rankTable, runtimeCommConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(runtimeCommConfig.GetConfigNetPlaneInfoSet());
    EXPECT_EQ(runtimeCommConfig.GetConfigNetPlaneId(), 0U);
    EXPECT_EQ(runtimeCommConfig.GetConfigNetPlaneNum(), 2U);
    ASSERT_EQ(rankTable.rankList.size(), 8U);
    EXPECT_EQ(rankTable.rankList[0].netPlaneId, 0U);
    EXPECT_EQ(rankTable.rankList[0].netPlaneNum, 2U);
}

TEST_F(HcclCommInitClusterInfoOxcTest,
    Ut_InjectWorldCommNetPlaneInfo_When_NonOxcRankTable_Expect_RuntimeCommConfigKeepUnset)
{
    RankTable_t rankTable;
    rankTable.version = HCCL_CLUSTER_VERSION;
    rankTable.rankList.resize(1);
    rankTable.rankList[0].rankId = 0;

    CommConfig runtimeCommConfig("ut_world_netplane_non_oxc");
    HcclResult ret = InjectWorldCommNetPlaneInfo(0, rankTable, runtimeCommConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(runtimeCommConfig.GetConfigNetPlaneInfoSet());
}

TEST_F(HcclCommInitClusterInfoOxcA3Test, Ut_CfgGetClusterInfo_When_A3OxcRankTable1_4_Expect_ParseSuperPodAndIps)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable();

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(rankTable.rankList.size(), 2U);
    EXPECT_EQ(rankTable.superPodNum, 2U);
    EXPECT_EQ(rankTable.rankList[0].superPodId, std::string("pod0"));
    EXPECT_EQ(rankTable.rankList[1].superPodId, std::string("pod1"));
    EXPECT_EQ(rankTable.rankList[0].superDeviceId, 100U);
    EXPECT_EQ(rankTable.rankList[1].superDeviceId, 200U);
    EXPECT_EQ(rankTable.rankList[0].superPodIdx, 0U);
    EXPECT_EQ(rankTable.rankList[1].superPodIdx, 1U);
    EXPECT_FALSE(rankTable.rankList[0].hostIp.IsInvalid());
    ASSERT_EQ(rankTable.rankList[0].deviceInfo.deviceIp.size(), 1U);
    EXPECT_FALSE(rankTable.rankList[0].deviceInfo.deviceIp[0].IsInvalid());
    ASSERT_EQ(rankTable.rankList[0].deviceInfo.backupDeviceIp.size(), 1U);
    EXPECT_FALSE(rankTable.rankList[0].deviceInfo.backupDeviceIp[0].IsInvalid());
    EXPECT_EQ(rankTable.rankList[0].deviceInfo.port, 16666U);
    EXPECT_EQ(rankTable.rankList[0].deviceInfo.backupPort, 17666U);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTable1_4_Expect_InfoLogSummaryWithoutFullDetail)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_INFO);
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable();

    std::string output = CaptureCfgGetClusterInfoStdout(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Summary] rank_num[2] server_num[2] super_pod_num[2] group_num[2]."),
        std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Summary][Rank] rank_id[0] server_id[server0] super_pod_id[pod0] super_device_id[100] group_id[group0]."),
        std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Summary][Rank] rank_id[1] server_id[server1] super_pod_id[pod1] super_device_id[200] group_id[group1]."),
        std::string::npos);
    EXPECT_EQ(output.find("[OXC_HCOMM][Parse][Detail][Rank]"), std::string::npos);
    EXPECT_EQ(output.find("level_list["), std::string::npos);
    EXPECT_EQ(output.find("rank_addr_list["), std::string::npos);
    EXPECT_EQ(output.find("device_port[16666]"), std::string::npos);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTable1_4_Expect_DebugLogContainsFullDetail)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_DEBUG);
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable();

    std::string output = CaptureCfgGetClusterInfoStdout(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Detail][Rank] rank_id[0] server_id[server0] super_pod_id[pod0] super_device_id[100] group_id[group0]"),
        std::string::npos);
    EXPECT_NE(output.find("level_source[synthesized]"), std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Detail][Level] rank_id[0] net_layer[0] net_type[TOPO_FILE_DESC] net_instance_id[server0-l0]."),
        std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Detail][RankAddr] rank_id[0] net_layer[0] addr[server0] addr_type[IPV4] plane_id[server0-dev0] ports[]."),
        std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Detail][RankAddr] rank_id[0] net_layer[1] addr[server0] addr_type[IPV4] plane_id[group0] ports[]."),
        std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Detail][Ports] rank_id[0] device_port[16666] backup_device_port[17666]."),
        std::string::npos);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableMissDeviceIpAcrossServers_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, false, false, true);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_NOT_FOUND);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableDevicePortInvalid_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, false, false, false, true);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableMissBackupDeviceIpAndPort_Expect_ParseSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, false, false, false, false, true, true);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(rankTable.rankList.size(), 2U);
    EXPECT_TRUE(rankTable.rankList[0].deviceInfo.backupDeviceIp.empty());
    EXPECT_EQ(rankTable.rankList[0].deviceInfo.backupPort, HCCL_INVALID_PORT);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_SingleServerA3OxcRankTableMissHostAndDeviceIp_Expect_ParseSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildSingleServerOxcA3RankTable();
    rankTableJson["server_list"][0].erase("host_ip");
    rankTableJson["server_list"][0]["device"][0].erase("device_ip");
    rankTableJson["server_list"][0]["device"][1].erase("device_ip");

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(rankTable.serverNum, 1U);
    EXPECT_TRUE(rankTable.rankList[0].hostIp.IsInvalid());
    ASSERT_EQ(rankTable.rankList[0].deviceInfo.deviceIp.size(), 1U);
    EXPECT_TRUE(rankTable.rankList[0].deviceInfo.deviceIp[0].IsInvalid());
    EXPECT_TRUE(rankTable.rankList[1].hostIp.IsInvalid());
    ASSERT_EQ(rankTable.rankList[1].deviceInfo.deviceIp.size(), 1U);
    EXPECT_TRUE(rankTable.rankList[1].deviceInfo.deviceIp[0].IsInvalid());
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableMissSuperPodList_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(true, false, false);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_NOT_FOUND);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableMissSuperDeviceId_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, true, false);

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_NOT_FOUND);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableHasDuplicateSuperDeviceIdInSamePod_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, false, true);
    rankTableJson["super_pod_list"][1]["super_pod_id"] = "pod0";

    EXPECT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_E_PARA);
}
