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
#include "hcom_common.h"
#include "topoinfo_plane_transformer.h"
#include "topoinfo_ranktable_partition.h"

HcclResult HcclCreateSubCommConfigInner(hccl::hcclComm *globalComm, uint32_t rankNum, uint32_t *rankIds,
    uint32_t subCommRankId, hccl::CommConfig &commConfig, HcclComm *subComm);

namespace {
int g_mockDeviceLogicId = 0;
/**
 * @brief 构造用于 netplane 计算与 subcomm 初始化的 OXC 1.4 风格 ranktable。
 * @return nlohmann::json
 */
nlohmann::json BuildNetPlaneOxcRankTable()
{
    return nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "oxc_netplane_ut_001"},
        {"server_list", nlohmann::json::array({
            nlohmann::json::object({
                {"server_id", "server0"},
                {"host_ip", "192.168.20.10"},
                {"device", nlohmann::json::array({
                    nlohmann::json::object({
                        {"device_id", "0"},
                        {"super_device_id", "0"},
                        {"rankid", "0"},
                        {"device_ip", "10.20.0.10"},
                        {"backup_device_ip", "10.21.0.10"},
                        {"device_port", 16600},
                        {"backup_device_port", 17600}
                    }),
                    nlohmann::json::object({
                        {"device_id", "1"},
                        {"super_device_id", "1"},
                        {"rankid", "1"},
                        {"device_ip", "10.20.0.11"},
                        {"backup_device_ip", "10.21.0.11"},
                        {"device_port", 16601},
                        {"backup_device_port", 17601}
                    })
                })}
            })
        })},
        {"super_pod_list", nlohmann::json::array({
            nlohmann::json::object({
                {"super_pod_id", "pod0"},
                {"server_list", nlohmann::json::array({nlohmann::json::object({{"server_id", "server0"}})})}
            })
        })},
        {"oxc_group_list", nlohmann::json::array({
            nlohmann::json::object({
                {"group_id", "group0"},
                {"rank_list", nlohmann::json::array({
                    nlohmann::json::object({{"rank_id", "0"}}),
                    nlohmann::json::object({{"rank_id", "1"}})
                })}
            })
        })}
    });
}

/**
 * @brief 为 `HcclCreateSubCommConfigInner` 构造可逆向读取的轻量 global comm。
 * @param globalComm 输出：供 inner 链路读取的 global comm。
 * @param globalParams 输出：由 parser 产出的全局通信参数。
 * @param globalRankTable 输出：由 parser 产出的全局 ranktable。
 */
void PrepareGlobalComm(hccl::hcclComm &globalComm, hccl::HcclCommParams &globalParams, hccl::RankTable_t &globalRankTable)
{
    HcclResult ret = CfgGetClusterInfo(BuildNetPlaneOxcRankTable().dump(), "0", globalParams, globalRankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::CommConfig globalConfig("global_netplane_ut");
    globalComm.communicator_.reset(new hccl::HcclCommunicator(globalConfig));
    ASSERT_NE(globalComm.communicator_, nullptr);

    hccl::HcclCommunicator *communicator = globalComm.communicator_.get();
    communicator->userRank_ = globalParams.rank;
    communicator->realUserRank_ = globalParams.userRank;
    communicator->userRankSize_ = globalParams.totalRanks;
    communicator->deviceLogicId_ = globalParams.logicDevId;
    communicator->deviceType_ = globalParams.deviceType;
    communicator->identifier_ = "global_netplane_ut";
    communicator->serverNum_ = globalRankTable.serverNum;
    communicator->superPodNum_ = globalRankTable.superPodNum;
    communicator->nicDeployment_ = globalRankTable.nicDeploy;
    communicator->servRankInfo_.clear();
    for (const auto &rankInfo : globalRankTable.rankList) {
        communicator->servRankInfo_[rankInfo.serverId].emplace_back(rankInfo);
    }
}

/**
 * @brief 为 inner 链路中的重资源步骤打桩，仅保留 netplane 解析与配置注入逻辑。
 */
void MockSubCommInitForNetPlane()
{
    MOCKER(InitOtherInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(InitWorkflowMode)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcomSetGroupTopoInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclNslbDp::SetCommInfo_NoRankTable)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcclNslbDp::SendTableFir)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::init,
        HcclResult (hccl::hcclComm::*)(hccl::HcclCommParams &, const hccl::CommConfig &, const hccl::RankTable_t &))
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetDeterministicConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetQpQosAttr)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetAivModeConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetOnlyAivModeConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetAicpuUnfoldConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetExecTimeOutConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetAlgoConfig)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::hcclComm::SetIndependentOpConfig)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));
}

/**
 * @brief 为 A3 OXC world-comm / subcomm 初始化提供稳定的本端 deviceLogicId。
 * @param deviceLogicId 输出：当前逻辑设备 ID。
 * @return HcclResult
 */
HcclResult HrtGetDeviceA3Stub(s32 *deviceLogicId)
{
    CHK_PTR_NULL(deviceLogicId);
    *deviceLogicId = g_mockDeviceLogicId;
    return HCCL_SUCCESS;
}

/**
 * @brief 为 A3 OXC UT 提供超节点模式所需的 SERVER_ID / SDID 运行时信息。
 * @param deviceId 当前设备 ID。
 * @param hcclModuleType 查询的模块类型。
 * @param hcclInfoType 查询的信息类型。
 * @param val 输出：模拟返回值。
 * @return HcclResult
 */
HcclResult HrtGetDeviceInfoA3Stub(u32 deviceId, HcclRtDeviceModuleType hcclModuleType,
    HcclRtDeviceInfoType hcclInfoType, s64 &val)
{
    (void)deviceId;
    (void)hcclModuleType;
    if (hcclInfoType == HcclRtDeviceInfoType::HCCL_INFO_TYPE_SERVER_ID) {
        val = 0x1234;
        return HCCL_SUCCESS;
    }

    if (hcclInfoType == HcclRtDeviceInfoType::HCCL_INFO_TYPE_SDID) {
        val = 100 + g_mockDeviceLogicId;
        return HCCL_SUCCESS;
    }

    val = 0;
    return HCCL_SUCCESS;
}

/**
 * @brief 构造 A3 subcomm public runtime-ready 用的 OXC 1.4 风格 ranktable。
 * @return nlohmann::json
 */
nlohmann::json BuildOxcA3SubCommRankTable()
{
    auto buildDevice = [](uint32_t rankId, uint32_t deviceId, uint32_t superDeviceId) {
        return nlohmann::json::object({
            {"device_id", std::to_string(deviceId)},
            {"super_device_id", std::to_string(superDeviceId)},
            {"rankid", std::to_string(rankId)},
            {"device_ip", std::string("10.10.0.") + std::to_string(10 + rankId)},
            {"backup_device_ip", std::string("10.20.0.") + std::to_string(10 + rankId)},
            {"device_port", 16666 + rankId},
            {"backup_device_port", 17666 + rankId}
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
        {"task_id", "oxc_subcomm_runtime_ut"},
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

/**
 * @brief 将 A3 OXC ranktable 写入 UT 临时文件。
 * @param fileName ranktable 文件名。
 */
void WriteOxcA3SubCommRankTableFile(const char *fileName)
{
    Ut_Clusterinfo_File_Create(fileName, BuildOxcA3SubCommRankTable());
}

/**
 * @brief 初始化 subcomm public API 使用的通信域配置。
 * @param config 输出配置。
 * @param commName 通信域名称。
 */
void InitA3SubCommConfig(HcclCommConfig &config, const char *commName)
{
    HcclCommConfigInit(&config);
    ASSERT_EQ(strcpy_s(config.hcclCommName, sizeof(config.hcclCommName), commName), EOK);
    config.hcclOpExpansionMode = COMM_CONFIG_OPEXPANSION_AICPU;
}

/**
 * @brief 获取通信域句柄内绑定的 communicator 实例。
 * @param comm HcclComm 句柄。
 * @return hccl::HcclCommunicator&
 */
hccl::HcclCommunicator &GetCommunicatorFromComm(HcclComm comm)
{
    auto *commHandle = static_cast<hccl::hcclComm *>(comm);
    EXPECT_NE(commHandle, nullptr);
    EXPECT_NE(commHandle->communicator_.get(), nullptr);
    return *(commHandle->communicator_.get());
}

/**
 * @brief 为 A3 world-comm / subcomm 初始化补齐 communicator 主链所需桩函数。
 */
void MockA3CommunicatorInit()
{
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

/**
 * @brief 断言 subcomm 已具备 public runtime-ready 所需的 A3 / OXC 语义。
 * @param subComm 子通信域句柄。
 * @param expectedRank 当前 rank 在子通信域中的局部 rank。
 * @param expectedRankSize 子通信域规模。
 * @param expectedNetPlaneId 期望的并行平面 ID。
 * @param expectedNetPlaneNum 期望的并行平面总数。
 */
void ExpectSubCommRuntimeReady(HcclComm subComm, uint32_t expectedRank, uint32_t expectedRankSize,
    uint32_t expectedNetPlaneId, uint32_t expectedNetPlaneNum)
{
    ASSERT_NE(subComm, nullptr);

    uint32_t rankId = UINT32_MAX;
    uint32_t rankSize = 0;
    uint32_t netPlaneId = UINT32_MAX;
    uint32_t netPlaneNum = 0;
    ASSERT_EQ(HcclGetRankId(subComm, &rankId), HCCL_SUCCESS);
    ASSERT_EQ(HcclGetRankSize(subComm, &rankSize), HCCL_SUCCESS);
    ASSERT_EQ(HcclGetNetPlaneId(subComm, &netPlaneId), HCCL_SUCCESS);
    ASSERT_EQ(HcclGetNetPlaneNum(subComm, &netPlaneNum), HCCL_SUCCESS);
    EXPECT_EQ(rankId, expectedRank);
    EXPECT_EQ(rankSize, expectedRankSize);
    EXPECT_EQ(netPlaneId, expectedNetPlaneId);
    EXPECT_EQ(netPlaneNum, expectedNetPlaneNum);

    auto &communicator = GetCommunicatorFromComm(subComm);
    HcclTopoAttr topoAttr = communicator.GetTopoAttr();
    EXPECT_TRUE(topoAttr.isOxcMode);
    EXPECT_TRUE(communicator.useSuperPodMode_);
    EXPECT_EQ(communicator.superPodNum_, 2U);
    EXPECT_TRUE(topoAttr.useSuperPodMode);
    EXPECT_EQ(topoAttr.superPodNum, 2U);
}
}  // namespace

class HcclCreateSubCommNetPlaneTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        g_mockDeviceLogicId = 0;
        Ut_Device_Set(0);
        DevType deviceType = DevType::DEV_TYPE_910B;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        GetHcclOpInfoCtx().opGroup2CommMap.clear();
        BaseInit::TearDown();
        GlobalMockObject::verify();
        remove(rankTableFileName);
    }
};

class HcclCreateSubCommNetPlaneRuntimeTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        g_mockDeviceLogicId = 0;
        Ut_Device_Set(0);
        SetIfProfile(false);

        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType)
            .stubs()
            .with(outBound(deviceType))
            .will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDevice)
            .defaults()
            .will(invoke(HrtGetDeviceA3Stub));
        MOCKER(hrtGetDeviceInfo)
            .defaults()
            .will(invoke(HrtGetDeviceInfoA3Stub));
        MockA3CommunicatorInit();
    }

    void TearDown() override
    {
        GetHcclOpInfoCtx().opGroup2CommMap.clear();
        BaseInit::TearDown();
        GlobalMockObject::verify();
        remove(rankTableFileName);
    }
};

TEST_F(HcclCreateSubCommNetPlaneTest, Ut_TopoinfoPlaneTransformer_When_ParsePlane_Expect_ReturnValueIsValid)
{
    HcclCommParams params;
    RankTable_t globalRankTable;
    HcclResult ret = CfgGetClusterInfo(BuildNetPlaneOxcRankTable().dump(), "0", params, globalRankTable);
    ASSERT_EQ(ret, HCCL_SUCCESS);

    hccl::TopoinfoRanktablePartition partition(params, globalRankTable);
    RankTable_t subRankTable;
    uint32_t rankIds[] = {0, 1};
    ret = partition.GenerateSubRankTable(2, rankIds, subRankTable);
    ASSERT_EQ(ret, HCCL_SUCCESS);

    u32 netPlaneId = 0;
    u32 netPlaneNum = 0;
    ret = hccl::TopoinfoPlaneTransformer::ParsePlane(globalRankTable, subRankTable, 0, netPlaneId, netPlaneNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneId, 0U);
    EXPECT_EQ(netPlaneNum, 2U);
}

TEST_F(HcclCreateSubCommNetPlaneTest, Ut_HcclCreateSubCommConfigInner_When_NetPlaneEnabled_Expect_CommConfigCarriesNetPlane)
{
    hccl::HcclCommParams globalParams;
    hccl::RankTable_t globalRankTable;
    hccl::hcclComm globalComm;
    HcclComm subComm = nullptr;
    hccl::CommConfig commConfig("sub_netplane_ut");
    uint32_t rankIds[] = {0, 1};

    PrepareGlobalComm(globalComm, globalParams, globalRankTable);
    MockSubCommInitForNetPlane();

    HcclResult ret = HcclCreateSubCommConfigInner(&globalComm, 2, rankIds, 0, commConfig, &subComm);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_NE(subComm, nullptr);
    EXPECT_TRUE(commConfig.GetConfigNetPlaneInfoSet());
    EXPECT_EQ(commConfig.GetConfigNetPlaneId(), 0U);
    EXPECT_EQ(commConfig.GetConfigNetPlaneNum(), 2U);

    /**
     * @brief 当前用例只把 inner 链路验证到“平面结果已注入 CommConfig”为止。
     *
     * @note 由于该用例对 `hcclComm::init` 做了轻量打桩，返回后的 `subComm`
     *       不等价于真实 runtime 已完整初始化的通信域。下面的 API 查询通过
     *       使用同一份 `CommConfig` 手工构造 communicator，目的是补证
     *       “CommConfig 中注入的 netplane 结果可被 comm/API 层消费”，
     *       而不是宣称当前用例已经覆盖真实 runtime 自然闭环。
     */
    hccl::hcclComm *subCommHandle = static_cast<hccl::hcclComm *>(subComm);
    subCommHandle->communicator_.reset(new hccl::HcclCommunicator(commConfig));

    u32 netPlaneId = 0;
    u32 netPlaneNum = 0;
    ret = HcclGetNetPlaneId(subComm, &netPlaneId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneId, 0U);

    ret = HcclGetNetPlaneNum(subComm, &netPlaneNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneNum, 2U);
}

TEST_F(HcclCreateSubCommNetPlaneTest,
    Ut_TopoinfoRanktablePartition_When_GetRankTableStrForOxcSubComm_Expect_OxcMetadataAndLevelListPreserved)
{
    HcclCommParams params;
    RankTable_t globalRankTable;
    ASSERT_EQ(CfgGetClusterInfo(BuildNetPlaneOxcRankTable().dump(), "0", params, globalRankTable), HCCL_SUCCESS);

    hccl::TopoinfoRanktablePartition partition(params, globalRankTable);
    RankTable_t subRankTable;
    uint32_t rankIds[] = {0, 1};
    ASSERT_EQ(partition.GenerateSubRankTable(2, rankIds, subRankTable), HCCL_SUCCESS);
    EXPECT_EQ(subRankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(subRankTable.taskId, std::string("oxc_netplane_ut_001"));

    std::string subRankTableString;
    ASSERT_EQ(partition.GetRankTableStr(subRankTable, subRankTableString), HCCL_SUCCESS);

    HcclCommParams reparsedParams;
    RankTable_t reparsedRankTable;
    ASSERT_EQ(CfgGetClusterInfo(subRankTableString, "0", reparsedParams, reparsedRankTable), HCCL_SUCCESS);
    EXPECT_EQ(reparsedRankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(reparsedRankTable.taskId, std::string("oxc_netplane_ut_001"));
    ASSERT_EQ(reparsedRankTable.rankList.size(), 2U);
    EXPECT_EQ(reparsedRankTable.rankList[0].oxcGroupId, std::string("group0"));
}

TEST_F(HcclCreateSubCommNetPlaneTest,
    Ut_TopoinfoRanktablePartition_When_OriginalSuperPodRanksBecomeNonContiguous_Expect_SplitSuperPodCountPreserved)
{
    HcclCommParams params;
    RankTable_t globalRankTable;
    ASSERT_EQ(CfgGetClusterInfo(BuildOxcA3SubCommRankTable().dump(), "0", params, globalRankTable), HCCL_SUCCESS);

    hccl::TopoinfoRanktablePartition partition(params, globalRankTable);
    RankTable_t subRankTable;
    uint32_t rankIds[] = {0, 4, 2};
    ASSERT_EQ(partition.GenerateSubRankTable(3, rankIds, subRankTable), HCCL_SUCCESS);
    EXPECT_EQ(subRankTable.superPodNum, 3U);
    ASSERT_EQ(subRankTable.rankList.size(), 3U);
    EXPECT_EQ(subRankTable.rankList[0].superPodId, std::string("pod0"));
    EXPECT_EQ(subRankTable.rankList[1].superPodId, std::string("pod1"));
    EXPECT_EQ(subRankTable.rankList[2].superPodId, std::string("pod0_HCCLSPLIT_0"));
}

TEST_F(HcclCreateSubCommNetPlaneRuntimeTest,
    Ut_HcclComm_When_GetCommRankTableFromA3OxcWorldComm_Expect_OxcMetadataPreserved)
{
    WriteOxcA3SubCommRankTableFile(rankTableFileName);

    HcclComm globalComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &globalComm), HCCL_SUCCESS);

    auto *globalCommHandle = static_cast<hccl::hcclComm *>(globalComm);
    ASSERT_NE(globalCommHandle, nullptr);

    RankTable_t exportedRankTable;
    ASSERT_EQ(globalCommHandle->GetCommRankTable(exportedRankTable), HCCL_SUCCESS);
    EXPECT_EQ(exportedRankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_EQ(exportedRankTable.taskId, std::string("oxc_subcomm_runtime_ut"));
    EXPECT_EQ(exportedRankTable.rankNum, 8U);

    Ut_Comm_Destroy(reinterpret_cast<void *&>(globalComm));
}

TEST_F(HcclCreateSubCommNetPlaneRuntimeTest,
    Ut_HcclCreateSubCommConfig_When_A3OxcPlane0Selector_Expect_PublicSubCommRuntimeReady)
{
    WriteOxcA3SubCommRankTableFile(rankTableFileName);

    HcclComm globalComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &globalComm), HCCL_SUCCESS);

    HcclCommConfig config;
    InitA3SubCommConfig(config, "ut_a3_subcomm_plane0");
    uint32_t rankIds[] = {0, 2, 4, 6};
    HcclComm subComm = nullptr;
    ASSERT_EQ(HcclCreateSubCommConfig(&globalComm, 4, rankIds, 100, 0, &config, &subComm), HCCL_SUCCESS);
    ExpectSubCommRuntimeReady(subComm, 0U, 4U, 0U, 2U);

    Ut_Comm_Destroy(reinterpret_cast<void *&>(subComm));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(globalComm));
}

TEST_F(HcclCreateSubCommNetPlaneRuntimeTest,
    Ut_HcclCreateSubCommConfig_When_A3OxcPlane1Selector_Expect_PublicSubCommRuntimeReady)
{
    g_mockDeviceLogicId = 1;
    Ut_Device_Set(1);
    WriteOxcA3SubCommRankTableFile(rankTableFileName);

    HcclComm globalComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 1, &globalComm), HCCL_SUCCESS);

    HcclCommConfig config;
    InitA3SubCommConfig(config, "ut_a3_subcomm_plane1");
    uint32_t rankIds[] = {1, 3, 5, 7};
    HcclComm subComm = nullptr;
    ASSERT_EQ(HcclCreateSubCommConfig(&globalComm, 4, rankIds, 101, 0, &config, &subComm), HCCL_SUCCESS);
    ExpectSubCommRuntimeReady(subComm, 0U, 4U, 1U, 2U);

    Ut_Comm_Destroy(reinterpret_cast<void *&>(subComm));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(globalComm));
}

TEST_F(HcclCreateSubCommNetPlaneRuntimeTest,
    Ut_HcclCreateSubCommConfig_When_A3OxcSelectorIsNonUniform_Expect_PublicSubCommDegradesToSinglePlane)
{
    WriteOxcA3SubCommRankTableFile(rankTableFileName);

    HcclComm globalComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &globalComm), HCCL_SUCCESS);

    HcclCommConfig config;
    InitA3SubCommConfig(config, "ut_a3_subcomm_fallback");
    uint32_t rankIds[] = {0, 1, 4, 5};
    HcclComm subComm = nullptr;
    ASSERT_EQ(HcclCreateSubCommConfig(&globalComm, 4, rankIds, 102, 0, &config, &subComm), HCCL_SUCCESS);
    ExpectSubCommRuntimeReady(subComm, 0U, 4U, 0U, 1U);

    Ut_Comm_Destroy(reinterpret_cast<void *&>(subComm));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(globalComm));
}
