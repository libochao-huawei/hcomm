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
#include <algorithm>
#include "rank_graph.h"

namespace {
RankTable_t BuildOxcNetPlaneRankTable()
{
    RankTable_t rankTable;
    rankTable.version = OXC_CLUSTER_VERSION;
    rankTable.rankNum = 2;

    RankInfo_t rank0;
    rank0.rankId = 0;
    rank0.serverId = "server0";
    rank0.deviceInfo.devicePhyId = 0;
    rank0.netPlaneId = 2;
    rank0.netPlaneNum = 4;

    RankInfo_t rank1;
    rank1.rankId = 1;
    rank1.serverId = "server0";
    rank1.deviceInfo.devicePhyId = 1;
    rank1.netPlaneId = 3;
    rank1.netPlaneNum = 4;

    rankTable.rankList = {rank0, rank1};
    return rankTable;
}

nlohmann::json BuildOxcInitRankTable()
{
    auto buildDevice = [](uint32_t rankId, uint32_t deviceId, uint32_t superDeviceId) {
        return nlohmann::json::object({
            {"device_id", std::to_string(deviceId)},
            {"super_device_id", std::to_string(superDeviceId)},
            {"rankid", std::to_string(rankId)},
            {"device_ip", std::string("10.10.0.") + std::to_string(10 + rankId)}
        });
    };

    nlohmann::json serverList = nlohmann::json::array();
    nlohmann::json superPodList = nlohmann::json::array({
        nlohmann::json::object({{"super_pod_id", "pod0"}, {"server_list", nlohmann::json::array()}}),
        nlohmann::json::object({{"super_pod_id", "pod1"}, {"server_list", nlohmann::json::array()}})
    });
    for (uint32_t serverIdx = 0; serverIdx < 4; ++serverIdx) {
        const std::string serverId = "server" + std::to_string(serverIdx);
        const uint32_t superDeviceBase = (serverIdx < 2) ? 100U : 200U;
        serverList.push_back(nlohmann::json::object({
            {"server_id", serverId},
            {"host_ip", std::string("192.168.10.") + std::to_string(10 + serverIdx)},
            {"device", nlohmann::json::array({
                buildDevice(serverIdx * 2, 0, superDeviceBase + serverIdx * 2),
                buildDevice(serverIdx * 2 + 1, 1, superDeviceBase + serverIdx * 2 + 1)
            })}
        }));
        superPodList[serverIdx < 2 ? 0 : 1]["server_list"].push_back(nlohmann::json::object({{"server_id", serverId}}));
    }

    return nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "oxc_comm_init_a2_ut"},
        {"server_list", serverList},
        {"super_pod_list", superPodList},
        {"oxc_group_list", nlohmann::json::array({
            nlohmann::json::object({{"group_id", "group0"}, {"rank_list", nlohmann::json::array({
                nlohmann::json::object({{"rank_id", "0"}}),
                nlohmann::json::object({{"rank_id", "1"}}),
                nlohmann::json::object({{"rank_id", "2"}}),
                nlohmann::json::object({{"rank_id", "3"}})
            })}}),
            nlohmann::json::object({{"group_id", "group1"}, {"rank_list", nlohmann::json::array({
                nlohmann::json::object({{"rank_id", "4"}}),
                nlohmann::json::object({{"rank_id", "5"}}),
                nlohmann::json::object({{"rank_id", "6"}}),
                nlohmann::json::object({{"rank_id", "7"}})
            })}})
        })}
    });
}

void WriteOxcInitRankTableFile(const char *fileName)
{
    Ut_Clusterinfo_File_Create(fileName, BuildOxcInitRankTable());
}

void InitCommConfig(HcclCommConfig &config, const char *commName)
{
    HcclCommConfigInit(&config);
    ASSERT_EQ(strcpy_s(config.hcclCommName, sizeof(config.hcclCommName), commName), EOK);
}

void ExpectOxcA2Ready(HcclComm comm, uint32_t expectedNetPlaneId, uint32_t expectedNetPlaneNum)
{
    uint32_t netPlaneId = UINT32_MAX;
    uint32_t netPlaneNum = 0;
    ASSERT_EQ(HcclGetNetPlaneId(comm, &netPlaneId), HCCL_SUCCESS);
    ASSERT_EQ(HcclGetNetPlaneNum(comm, &netPlaneNum), HCCL_SUCCESS);
    EXPECT_EQ(netPlaneId, expectedNetPlaneId);
    EXPECT_EQ(netPlaneNum, expectedNetPlaneNum);

    uint32_t *layers = nullptr;
    uint32_t layerNum = 0;
    ASSERT_EQ(HcclRankGraphGetLayers(comm, &layers, &layerNum), HCCL_SUCCESS);
    ASSERT_NE(layers, nullptr);
    std::vector<uint32_t> layerVec(layers, layers + layerNum);
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL1), layerVec.end());
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL2), layerVec.end());
}

/**
 * @brief 描述 representative stronger assertions 所需的 netplane 期望。
 */
struct LayeredMemberExpectation {
    uint32_t netPlaneId;
    uint32_t netPlaneNum;
    std::vector<uint32_t> level1Ranks;
    std::vector<uint32_t> level2Ranks;
    uint32_t level1NegativeSrc;
    uint32_t level1NegativeDst;
};

/**
 * @brief 返回 rank=1 视角下的 OXC layered 成员期望。
 * @return const LayeredMemberExpectation&
 */
const LayeredMemberExpectation &GetRank1LayeredExpectation()
{
    static const LayeredMemberExpectation expectation {
        1U, 2U, {1U, 3U}, {1U, 5U}, 1U, 5U
    };
    return expectation;
}

/**
 * @brief 返回 rank=4 视角下的 OXC layered 成员期望。
 * @return const LayeredMemberExpectation&
 */
const LayeredMemberExpectation &GetRank4LayeredExpectation()
{
    static const LayeredMemberExpectation expectation {
        0U, 2U, {4U, 6U}, {0U, 4U}, 4U, 0U
    };
    return expectation;
}

/**
 * @brief 在共享 smoke 断言基础上，补充 restored Layered 可见性断言。
 * @param comm 通信域句柄。
 * @param expectation 预期的 netplane 信息。
 */
void ExpectSingleLayeredTopoInstRanks(HcclComm comm, uint32_t layer, const std::vector<uint32_t> &expectedRanks)
{
    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(comm, layer, &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_NE(topoInsts, nullptr);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(topoInsts[0], 0U);

    uint32_t *ranks = nullptr;
    uint32_t rankNum = 0;
    ASSERT_EQ(HcclRankGraphGetRanksByTopoInst(comm, layer, topoInsts[0], &ranks, &rankNum), HCCL_SUCCESS);
    ASSERT_NE(ranks, nullptr);
    ASSERT_EQ(rankNum, expectedRanks.size());
    std::vector<uint32_t> rankVec(ranks, ranks + rankNum);
    EXPECT_EQ(rankVec, expectedRanks);
}

void ExpectNoLayeredLinks(HcclComm comm, uint32_t layer, uint32_t srcRank, uint32_t dstRank)
{
    CommLink *links = nullptr;
    uint32_t linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(comm, layer, srcRank, dstRank, &links, &linkNum), HCCL_SUCCESS);
    EXPECT_EQ(links, nullptr);
    EXPECT_EQ(linkNum, 0U);
}

void ExpectOxcA2ReadyWithRestoredLayeredMainline(HcclComm comm, const LayeredMemberExpectation &expectation)
{
    ExpectOxcA2Ready(comm, expectation.netPlaneId, expectation.netPlaneNum);
    ExpectSingleLayeredTopoInstRanks(comm, HCCL_NETLAYER_LAYERED_LEVEL1, expectation.level1Ranks);
    ExpectSingleLayeredTopoInstRanks(comm, HCCL_NETLAYER_LAYERED_LEVEL2, expectation.level2Ranks);
    ExpectNoLayeredLinks(comm, HCCL_NETLAYER_LAYERED_LEVEL1,
        expectation.level1NegativeSrc, expectation.level1NegativeDst);
}

void RunClusterInfoEntryCase(const char *rankTableFile, uint32_t rank, int deviceId, uint32_t expectedNetPlaneId,
    uint32_t expectedNetPlaneNum)
{
    Ut_Device_Set(deviceId);
    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFile, rank, &localComm), HCCL_SUCCESS);
    ASSERT_NE(localComm, nullptr);
    ExpectOxcA2Ready(localComm, expectedNetPlaneId, expectedNetPlaneNum);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

/**
 * @brief 通过 file 入口执行 representative stronger assertions。
 * @param rankTableFile ranktable 文件路径。
 * @param rank 初始化使用的 rank。
 * @param deviceId 当前设备 ID。
 * @param expectation layered 成员期望。
 */
void RunClusterInfoEntryCaseWithRestoredLayeredMainline(const char *rankTableFile, uint32_t rank, int deviceId,
    const LayeredMemberExpectation &expectation)
{
    Ut_Device_Set(deviceId);
    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFile, rank, &localComm), HCCL_SUCCESS);
    ASSERT_NE(localComm, nullptr);
    ExpectOxcA2ReadyWithRestoredLayeredMainline(localComm, expectation);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

void RunClusterInfoConfigEntryCase(const char *rankTableFile, uint32_t rank, int deviceId, const char *commName,
    uint32_t expectedNetPlaneId, uint32_t expectedNetPlaneNum)
{
    Ut_Device_Set(deviceId);
    HcclCommConfig config;
    InitCommConfig(config, commName);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoConfig(rankTableFile, rank, &config, &localComm), HCCL_SUCCESS);
    ASSERT_NE(localComm, nullptr);
    ExpectOxcA2Ready(localComm, expectedNetPlaneId, expectedNetPlaneNum);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

/**
 * @brief 通过 config 入口执行 representative stronger assertions。
 * @param rankTableFile ranktable 文件路径。
 * @param rank 初始化使用的 rank。
 * @param deviceId 当前设备 ID。
 * @param commName 通信域名称。
 * @param expectation layered 成员期望。
 */
void RunClusterInfoConfigEntryCaseWithRestoredLayeredMainline(const char *rankTableFile, uint32_t rank, int deviceId,
    const char *commName, const LayeredMemberExpectation &expectation)
{
    Ut_Device_Set(deviceId);
    HcclCommConfig config;
    InitCommConfig(config, commName);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoConfig(rankTableFile, rank, &config, &localComm), HCCL_SUCCESS);
    ASSERT_NE(localComm, nullptr);
    ExpectOxcA2ReadyWithRestoredLayeredMainline(localComm, expectation);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

void RunClusterInfoMemConfigEntryCase(const std::string &rankTableString, uint32_t rank, int deviceId,
    const char *commName, uint32_t expectedNetPlaneId, uint32_t expectedNetPlaneNum)
{
    Ut_Device_Set(deviceId);
    HcclCommConfig config;
    InitCommConfig(config, commName);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), rank, &config, &localComm), HCCL_SUCCESS);
    ASSERT_NE(localComm, nullptr);
    ExpectOxcA2Ready(localComm, expectedNetPlaneId, expectedNetPlaneNum);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

/**
 * @brief 通过 memconfig 入口执行 representative stronger assertions。
 * @param rankTableString ranktable JSON 字符串。
 * @param rank 初始化使用的 rank。
 * @param deviceId 当前设备 ID。
 * @param commName 通信域名称。
 * @param expectation layered 成员期望。
 */
void RunClusterInfoMemConfigEntryCaseWithRestoredLayeredMainline(const std::string &rankTableString, uint32_t rank,
    int deviceId, const char *commName, const LayeredMemberExpectation &expectation)
{
    Ut_Device_Set(deviceId);
    HcclCommConfig config;
    InitCommConfig(config, commName);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), rank, &config, &localComm), HCCL_SUCCESS);
    ASSERT_NE(localComm, nullptr);
    ExpectOxcA2ReadyWithRestoredLayeredMainline(localComm, expectation);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}
}

class HcclGetNetPlaneInfoTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        SetIfProfile(false);
        DevType deviceType = DevType::DEV_TYPE_910B;
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(deviceType))
            .will(returnValue(HCCL_SUCCESS));

        MOCKER(HcclNetInit)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER(HcclSetIfProfile)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER(CallMsprofReportHostApi)
            .stubs()
            .with(any(), any(), any(), any(), any(), any())
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitNetResource)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitDispatcher)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitTransportManager)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitMemoryManager)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitCombinOpara)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::RegisterRanksToDca)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::GetHDCommunicate)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::LoadAICPUKernel)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::LoadCustomKernel)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitHDCommunicate)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitOpRetry)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitOpResPara)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&HcclCommunicator::InitOneSidedService)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&hccl::hcclComm::SetGlobalWorkSpace)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&hccl::hcclComm::CreateOpBasedResources)
            .stubs()
            .with(any(), any(), any())
            .will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclGetNetPlaneInfoTest, Ut_HcclGetNetPlaneId_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    u32 netPlaneId = 0;
    HcclResult ret = HcclGetNetPlaneId(nullptr, &netPlaneId);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetNetPlaneInfoTest, Ut_HcclGetNetPlaneId_When_NetPlaneIdIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    hccl::hcclComm commHandle;
    HcclResult ret = HcclGetNetPlaneId(&commHandle, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetNetPlaneInfoTest, Ut_HcclGetNetPlaneNum_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    u32 netPlaneNum = 0;
    HcclResult ret = HcclGetNetPlaneNum(nullptr, &netPlaneNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetNetPlaneInfoTest, Ut_HcclGetNetPlaneId_When_Normal_Expect_ReturnValueIsValid)
{
    hccl::CommConfig commConfig("ut_netplane");
    commConfig.SetConfigNetPlane(3, 8);

    hccl::hcclComm commHandle;
    commHandle.communicator_.reset(new hccl::HcclCommunicator(commConfig));

    u32 netPlaneId = 0;
    HcclResult ret = HcclGetNetPlaneId(&commHandle, &netPlaneId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneId, 3U);
}

TEST_F(HcclGetNetPlaneInfoTest, Ut_HcclGetNetPlaneNum_When_Normal_Expect_ReturnValueIsValid)
{
    hccl::CommConfig commConfig("ut_netplane");
    commConfig.SetConfigNetPlane(3, 8);

    hccl::hcclComm commHandle;
    commHandle.communicator_.reset(new hccl::HcclCommunicator(commConfig));

    u32 netPlaneNum = 0;
    HcclResult ret = HcclGetNetPlaneNum(&commHandle, &netPlaneNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneNum, 8U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommunicator_When_CommConfigHasNoNetPlane_Expect_FallbackFromRankTableIsValid)
{
    hccl::CommConfig commConfig("ut_netplane_fallback");
    hccl::HcclCommunicator communicator(commConfig);
    communicator.userRank_ = 1;

    RankTable_t rankTable = BuildOxcNetPlaneRankTable();
    HcclResult ret = communicator.InitNetPlaneInfo(rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(communicator.netPlaneInfoValid_);
    EXPECT_EQ(communicator.netPlaneId_, 3U);
    EXPECT_EQ(communicator.netPlaneNum_, 4U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommunicator_When_CommConfigHasExplicitNetPlane_Expect_FallbackDoesNotOverrideSeededValue)
{
    hccl::CommConfig commConfig("ut_netplane_precedence");
    commConfig.SetConfigNetPlane(7, 9);
    hccl::HcclCommunicator communicator(commConfig);
    communicator.userRank_ = 1;

    RankTable_t rankTable = BuildOxcNetPlaneRankTable();
    HcclResult ret = communicator.InitNetPlaneInfo(rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(communicator.netPlaneInfoValid_);
    EXPECT_EQ(communicator.netPlaneId_, 7U);
    EXPECT_EQ(communicator.netPlaneNum_, 9U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfo_When_OxcRankTable2_0_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoEntryCaseWithRestoredLayeredMainline(rankTableFileName, 1, 1, GetRank1LayeredExpectation());
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfo_When_OxcRankTable2_0_Rank0_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoEntryCase(rankTableFileName, 0, 0, 0U, 2U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfo_When_OxcRankTable2_0_Rank5_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoEntryCase(rankTableFileName, 5, 1, 1U, 2U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfo_When_OxcRankTable2_0_Rank4_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoEntryCaseWithRestoredLayeredMainline(rankTableFileName, 4, 0, GetRank4LayeredExpectation());
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoConfig_When_OxcRankTable2_0_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoConfigEntryCaseWithRestoredLayeredMainline(rankTableFileName, 1, 1, "ut_oxc_cfg_rank1",
        GetRank1LayeredExpectation());
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoConfig_When_OxcRankTable2_0_Rank0_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoConfigEntryCase(rankTableFileName, 0, 0, "ut_oxc_cfg_rank0", 0U, 2U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoConfig_When_OxcRankTable2_0_Rank5_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoConfigEntryCase(rankTableFileName, 5, 1, "ut_oxc_cfg_rank5", 1U, 2U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoConfig_When_OxcRankTable2_0_Rank4_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    WriteOxcInitRankTableFile(rankTableFileName);
    RunClusterInfoConfigEntryCaseWithRestoredLayeredMainline(rankTableFileName, 4, 0, "ut_oxc_cfg_rank4",
        GetRank4LayeredExpectation());
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_OxcRankTable2_0_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    std::string rankTableString = BuildOxcInitRankTable().dump();
    RunClusterInfoMemConfigEntryCaseWithRestoredLayeredMainline(rankTableString, 1, 1, "ut_oxc_memcfg_rank1",
        GetRank1LayeredExpectation());
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_OxcRankTable2_0_Rank0_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    std::string rankTableString = BuildOxcInitRankTable().dump();
    RunClusterInfoMemConfigEntryCase(rankTableString, 0, 0, "ut_oxc_memcfg_rank0", 0U, 2U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_OxcRankTable2_0_Rank5_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    std::string rankTableString = BuildOxcInitRankTable().dump();
    RunClusterInfoMemConfigEntryCase(rankTableString, 5, 1, "ut_oxc_memcfg_rank5", 1U, 2U);
}

TEST_F(HcclGetNetPlaneInfoTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_OxcRankTable2_0_Rank4_Expect_NetPlaneAndLayeredRankGraphRestored)
{
    std::string rankTableString = BuildOxcInitRankTable().dump();
    RunClusterInfoMemConfigEntryCaseWithRestoredLayeredMainline(rankTableString, 4, 0, "ut_oxc_memcfg_rank4",
        GetRank4LayeredExpectation());
}
