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
/**
 * @brief 构造一份覆盖第一阶段 parser 关键路径的 OXC 2.0 ranktable 测试样例。
 * @param invalidRankCount 是否制造 rank_count 与 rank_list 大小不一致。
 * @param missingLevelList 是否删除首个 rank 的 level_list。
 * @param missingRankAddrList 是否删除首个 level 的 rank_addr_list。
 * @return nlohmann::json 测试输入。
 *
 * @note 正向路径覆盖 `task_id / rank_list / level_list / rank_addr_list`；
 *       负向路径通过参数开关删除关键字段或制造计数不一致。
 */
nlohmann::json BuildOxcRankTable(bool invalidRankCount = false, bool missingLevelList = false,
    bool missingRankAddrList = false)
{
    nlohmann::json level0Addr = {
        {"addr", "000000000000002000100000df000401"},
        {"addr_type", "EID"},
        {"plane_id", "0"},
        {"ports", nlohmann::json::array({"0/0"})}
    };
    nlohmann::json level1Addr = {
        {"addr", "10.10.10.1"},
        {"addr_type", "IPV4"},
        {"plane_id", "cluster-a"},
        {"ports", nlohmann::json::array()}
    };
    nlohmann::json level2Addr = {
        {"addr", "000000000000000000100000df0004b5"},
        {"addr_type", "EID"},
        {"plane_id", "0"},
        {"ports", nlohmann::json::array({"0/1", "0/2"})}
    };
    nlohmann::json level3Addr = {
        {"addr", ""},
        {"addr_type", "IPV4"},
        {"plane_id", "CLUSTER"},
        {"ports", nlohmann::json::array()}
    };

    nlohmann::json level0 = {
        {"net_layer", 0},
        {"net_instance_id", "server0-l0"},
        {"net_type", "TOPO_FILE_DESC"},
        {"net_attr", ""},
        {"rank_addr_list", nlohmann::json::array({level0Addr})}
    };
    nlohmann::json level1 = {
        {"net_layer", 1},
        {"net_instance_id", "server0"},
        {"net_type", "CLOS"},
        {"net_attr", ""},
        {"rank_addr_list", nlohmann::json::array({level1Addr})}
    };
    nlohmann::json level2 = {
        {"net_layer", 2},
        {"net_instance_id", "0"},
        {"net_type", "OXC_Mesh"},
        {"net_attr", ""},
        {"rank_addr_list", nlohmann::json::array({level2Addr})}
    };
    nlohmann::json level3 = {
        {"net_layer", 3},
        {"net_instance_id", "CLUSTER1"},
        {"net_type", "CLOS"},
        {"net_attr", ""},
        {"rank_addr_list", nlohmann::json::array({level3Addr})}
    };

    if (missingRankAddrList) {
        level0.erase("rank_addr_list");
    }

    nlohmann::json rank0 = {
        {"rank_id", 0},
        {"local_id", 0},
        {"device_id", 0},
        {"server_id", "server0"},
        {"level_list", nlohmann::json::array({level0, level1, level2, level3})}
    };
    nlohmann::json rank1 = {
        {"rank_id", 1},
        {"local_id", 1},
        {"device_id", 1},
        {"server_id", "server0"},
        {"level_list", nlohmann::json::array({level0, level1, level2, level3})}
    };

    if (missingLevelList) {
        rank0.erase("level_list");
    }

    return {
        {"status", "completed"},
        {"version", "2.0"},
        {"task_id", "oxc_task_ut_001"},
        {"rank_count", invalidRankCount ? 3 : 2},
        {"rank_list", nlohmann::json::array({rank0, rank1})}
    };
}

/**
 * @brief 构造 A3 world-comm 主线所需的 OXC 2.0 ranktable。
 * @param missingSuperPodId 是否删除 `super_pod_id`。
 * @param missingSuperDeviceId 是否删除 `super_device_id`。
 * @param duplicateSuperDeviceId 是否制造同一 super_pod_id 下的 super_device_id 冲突。
 * @return nlohmann::json
 */
nlohmann::json BuildOxcA3RankTable(bool missingSuperPodId = false, bool missingSuperDeviceId = false,
    bool duplicateSuperDeviceId = false)
{
    auto buildRank = [](uint32_t rankId, uint32_t localId, uint32_t deviceId, const std::string &serverId,
                         const std::string &groupId, const std::string &hostIp, const std::string &deviceIp,
                         const std::string &backupDeviceIp, const std::string &superPodId, uint32_t superDeviceId) {
        nlohmann::json rank = {
            {"rank_id", rankId},
            {"local_id", localId},
            {"device_id", deviceId},
            {"server_id", serverId},
            {"host_ip", hostIp},
            {"device_ip", deviceIp},
            {"backup_device_ip", backupDeviceIp},
            {"device_port", 16666 + rankId},
            {"backup_device_port", 17666 + rankId},
            {"super_pod_id", superPodId},
            {"super_device_id", superDeviceId},
            {"level_list", nlohmann::json::array({
                nlohmann::json::object({
                    {"net_layer", 0},
                    {"net_instance_id", serverId + "-l0"},
                    {"net_type", "TOPO_FILE_DESC"},
                    {"net_attr", ""},
                    {"rank_addr_list", nlohmann::json::array({
                        nlohmann::json::object({
                            {"addr", "000000000000002000100000df000401"},
                            {"addr_type", "EID"},
                            {"plane_id", "plane-l0"},
                            {"ports", nlohmann::json::array({"0/0"})}
                        })
                    })}
                }),
                nlohmann::json::object({
                    {"net_layer", 1},
                    {"net_instance_id", groupId},
                    {"net_type", "CLOS"},
                    {"net_attr", ""},
                    {"rank_addr_list", nlohmann::json::array({
                        nlohmann::json::object({
                            {"addr", "10.10.10.1"},
                            {"addr_type", "IPV4"},
                            {"plane_id", groupId},
                            {"ports", nlohmann::json::array()}
                        })
                    })}
                }),
                nlohmann::json::object({
                    {"net_layer", 2},
                    {"net_instance_id", "0"},
                    {"net_type", "OXC_Mesh"},
                    {"net_attr", ""},
                    {"rank_addr_list", nlohmann::json::array({
                        nlohmann::json::object({
                            {"addr", "000000000000000000100000df0004b5"},
                            {"addr_type", "EID"},
                            {"plane_id", "0"},
                            {"ports", nlohmann::json::array({"0/1", "0/2"})}
                        })
                    })}
                }),
                nlohmann::json::object({
                    {"net_layer", 3},
                    {"net_instance_id", "CLUSTER1"},
                    {"net_type", "CLOS"},
                    {"net_attr", ""},
                    {"rank_addr_list", nlohmann::json::array({
                        nlohmann::json::object({
                            {"addr", ""},
                            {"addr_type", "IPV4"},
                            {"plane_id", "CLUSTER"},
                            {"ports", nlohmann::json::array()}
                        })
                    })}
                })
            })}
        };
        return rank;
    };

    nlohmann::json rank0 = buildRank(0, 0, 0, "server0", "group0", "192.168.10.10", "10.10.0.10", "10.20.0.10",
        "pod0", 100);
    nlohmann::json rank1 = buildRank(1, 0, 0, "server1", "group1", "192.168.10.11", "10.10.0.11", "10.20.0.11",
        "pod1", duplicateSuperDeviceId ? 100 : 200);

    if (missingSuperPodId) {
        rank0.erase("super_pod_id");
    }
    if (missingSuperDeviceId) {
        rank0.erase("super_device_id");
    }

    return {
        {"status", "completed"},
        {"version", "2.0"},
        {"task_id", "oxc_a3_ranktable_ut"},
        {"rank_count", 2},
        {"rank_list", nlohmann::json::array({rank0, rank1})}
    };
}

/**
 * @brief 校验 OXC 2.0 ranktable 中的四层 level_list 已被完整保存。
 * @param rankTable parser 输出的内部 ranktable。
 */
void ExpectOxcMeshLevelLayout(const RankTable_t &rankTable)
{
    ASSERT_EQ(rankTable.rankList.size(), 2U);
    ASSERT_EQ(rankTable.rankList[0].levelList.size(), 4U);

    const auto &level0 = rankTable.rankList[0].levelList[0];
    EXPECT_EQ(level0.netLayer, 0U);
    EXPECT_EQ(level0.netType, std::string("TOPO_FILE_DESC"));
    ASSERT_EQ(level0.rankAddrList.size(), 1U);
    EXPECT_EQ(level0.rankAddrList[0].addrType, std::string("EID"));

    const auto &level1 = rankTable.rankList[0].levelList[1];
    EXPECT_EQ(level1.netLayer, 1U);
    EXPECT_EQ(level1.netType, std::string("CLOS"));

    const auto &level2 = rankTable.rankList[0].levelList[2];
    EXPECT_EQ(level2.netLayer, 2U);
    EXPECT_EQ(level2.netType, std::string("OXC_Mesh"));
    EXPECT_EQ(level2.netInstanceId, std::string("0"));
    ASSERT_EQ(level2.rankAddrList.size(), 1U);
    EXPECT_EQ(level2.rankAddrList[0].planeId, std::string("0"));

    const auto &level3 = rankTable.rankList[0].levelList[3];
    EXPECT_EQ(level3.netLayer, 3U);
    EXPECT_EQ(level3.netType, std::string("CLOS"));
    EXPECT_EQ(level3.netInstanceId, std::string("CLUSTER1"));
}
} // namespace

class HcclCommInitClusterInfoOxcTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        Ut_Device_Set(0);
        // 让 parser 路径稳定落到 910B 语义上，避免设备类型差异影响第一阶段 UT 结论。
        DevType deviceType = DevType::DEV_TYPE_910B;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
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
        Ut_Device_Set(0);
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_OxcRankTable2_0_Expect_ParseSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(rankTable.taskId, std::string("oxc_task_ut_001"));
    EXPECT_EQ(rankTable.rankNum, 2);
    EXPECT_EQ(rankTable.deviceNum, 2);
    EXPECT_EQ(rankTable.serverNum, 1U);
    EXPECT_EQ(rankTable.superPodNum, 0U);
    EXPECT_EQ(params.rank, 0);
    EXPECT_EQ(params.userRank, 0);
    EXPECT_EQ(params.totalRanks, 2);
    EXPECT_EQ(params.serverId, std::string("server0"));
    EXPECT_EQ(rankTable.rankList[0].serverId, std::string("server0"));
    EXPECT_EQ(rankTable.rankList[0].superPodId, std::string(""));
    EXPECT_EQ(rankTable.rankList[0].oxcGroupId, std::string("server0"));
    // 校验 parser 确实把包含 OXC_Mesh 在内的四层 OXC level_list 保存到了内部结构中。
    ExpectOxcMeshLevelLayout(rankTable);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfoWithoutDev_When_OxcRankTable2_0_Expect_ParseSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();

    HcclResult ret = CfgGetClusterInfoWithoutDev(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(rankTable.taskId, std::string("oxc_task_ut_001"));
    EXPECT_EQ(params.totalRanks, 2);
    ExpectOxcMeshLevelLayout(rankTable);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_RankCountMismatch_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(true, false, false);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_LevelListMissing_Expect_ReturnIsNotSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(false, true, false);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_RankAddrListMissing_Expect_ReturnIsNotSuccess)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable(false, false, true);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_LevelListIsNotIncreasing_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    rankTableJson["rank_list"][0]["level_list"][2]["net_layer"] = 1;

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_Level2NetTypeIsNotOxcMesh_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    rankTableJson["rank_list"][0]["level_list"][2]["net_type"] = "CLOS";

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_Level2NetInstanceIdIsNotZero_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    rankTableJson["rank_list"][0]["level_list"][2]["net_instance_id"] = "mesh-1";

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_RankMissLevel2_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    rankTableJson["rank_list"][1]["level_list"].erase(2);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest, Ut_CfgGetClusterInfo_When_RankLevelTypeLayoutMismatch_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    rankTableJson["rank_list"][1]["level_list"][1]["net_type"] = "TOPO_FILE_DESC";

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitClusterInfoOxcTest,
    Ut_InjectWorldCommNetPlaneInfo_When_OxcRankTable2_0_Expect_RuntimeCommConfigCarriesCurrentRankPlane)
{
    RankTable_t rankTable;
    HcclCommParams params;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    ASSERT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_SUCCESS);

    CommConfig runtimeCommConfig("ut_world_netplane");
    HcclResult ret = InjectWorldCommNetPlaneInfo(0, rankTable, runtimeCommConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(runtimeCommConfig.GetConfigNetPlaneInfoSet());
    EXPECT_EQ(runtimeCommConfig.GetConfigNetPlaneId(), 0U);
    EXPECT_EQ(runtimeCommConfig.GetConfigNetPlaneNum(), 2U);
    ASSERT_EQ(rankTable.rankList.size(), 2U);
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

TEST_F(HcclCommInitClusterInfoOxcTest,
    Ut_InjectWorldCommNetPlaneInfo_When_RuntimeCommConfigAlreadySet_Expect_DoNotOverrideExplicitSeed)
{
    RankTable_t rankTable;
    HcclCommParams params;
    nlohmann::json rankTableJson = BuildOxcRankTable();
    ASSERT_EQ(CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable), HCCL_SUCCESS);

    CommConfig runtimeCommConfig("ut_world_netplane_seeded");
    runtimeCommConfig.SetConfigNetPlane(7, 9);
    HcclResult ret = InjectWorldCommNetPlaneInfo(0, rankTable, runtimeCommConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(runtimeCommConfig.GetConfigNetPlaneId(), 7U);
    EXPECT_EQ(runtimeCommConfig.GetConfigNetPlaneNum(), 9U);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test, Ut_CfgGetClusterInfo_When_A3OxcRankTable_Expect_ParseSuperPodAndIps)
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
    Ut_CfgGetClusterInfo_When_A3OxcRankTableMissSuperPodId_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(true, false, false);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableMissSuperDeviceId_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, true, false);

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcclCommInitClusterInfoOxcA3Test,
    Ut_CfgGetClusterInfo_When_A3OxcRankTableHasDuplicateSuperDeviceIdInSamePod_Expect_ReturnIsHCCL_E_PARA)
{
    HcclCommParams params;
    RankTable_t rankTable;
    nlohmann::json rankTableJson = BuildOxcA3RankTable(false, false, true);
    rankTableJson["rank_list"][1]["super_pod_id"] = "pod0";

    HcclResult ret = CfgGetClusterInfo(rankTableJson.dump(), "0", params, rankTable);
    EXPECT_EQ(ret, HCCL_E_PARA);
}
