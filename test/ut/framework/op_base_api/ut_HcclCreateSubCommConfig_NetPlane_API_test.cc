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
/**
 * @brief 构造用于 netplane 计算与 subcomm 初始化的 OXC 2.0 ranktable。
 * @return nlohmann::json
 */
nlohmann::json BuildNetPlaneOxcRankTable()
{
    auto buildRank = [](int rankId, int localId, int deviceId, const char *planeId) {
        return nlohmann::json::object({
            {"rank_id", rankId},
            {"local_id", localId},
            {"device_id", deviceId},
            {"server_id", "server0"},
            {"level_list", nlohmann::json::array({
                nlohmann::json::object({
                    {"net_layer", 0},
                    {"net_instance_id", "server0-l0"},
                    {"net_type", "TOPO_FILE_DESC"},
                    {"net_attr", ""},
                    {"rank_addr_list", nlohmann::json::array({
                        nlohmann::json::object({
                            {"addr", "000000000000002000100000df000401"},
                            {"addr_type", "EID"},
                            {"plane_id", planeId},
                            {"ports", nlohmann::json::array({"0/0"})}
                        })
                    })}
                }),
                nlohmann::json::object({
                    {"net_layer", 1},
                    {"net_instance_id", "server0"},
                    {"net_type", "CLOS"},
                    {"net_attr", ""},
                    {"rank_addr_list", nlohmann::json::array({
                        nlohmann::json::object({
                            {"addr", "10.10.10.1"},
                            {"addr_type", "IPV4"},
                            {"plane_id", "cluster-a"},
                            {"ports", nlohmann::json::array()}
                        })
                    })}
                })
            })}
        });
    };

    return nlohmann::json::object({
        {"status", "completed"},
        {"version", "2.0"},
        {"task_id", "oxc_netplane_ut_001"},
        {"rank_count", 2},
        {"rank_list", nlohmann::json::array({
            buildRank(0, 0, 0, "plane-0"),
            buildRank(1, 1, 1, "plane-0")
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
}  // namespace

class HcclCreateSubCommNetPlaneTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
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
