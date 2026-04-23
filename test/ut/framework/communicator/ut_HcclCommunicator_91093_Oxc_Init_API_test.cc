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
#include "heartbeat.h"
#include "hccl_network.h"
#include "rank_graph.h"

bool IsOneSidedComm(HcclComm comm);
bool IsCommNameExistInOneSidedComms(s32 deviceLogicId, const std::string &commName);

namespace hccl {
void GetSocketTypeIn91093(const std::vector<RankInfo> &rankInfos, bool useSuperPodMode, u32 index, u32 nextOrPrevIndex,
    HcclSocketType &type);
}

namespace {
using namespace hccl;

/**
 * @brief 为 A3 OXC world-comm 初始化提供稳定的本端 deviceLogicId。
 * @param deviceLogicId 输出：当前逻辑设备 ID。
 * @return HcclResult
 */
HcclResult HrtGetDeviceA3Stub(s32 *deviceLogicId)
{
    CHK_PTR_NULL(deviceLogicId);
    *deviceLogicId = 0;
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
        val = 100;
        return HCCL_SUCCESS;
    }

    val = 0;
    return HCCL_SUCCESS;
}

/**
 * @brief 构造 A3 world-comm 主线使用的 OXC 1.4 风格 ranktable。
 * @return nlohmann::json
 *
 * @note 布局与第一阶段 OXC layered 用例保持一致：
 *       - 4 台 server；
 *       - 每台 server 2 个 rank；
 *       - 2 个 OXC group；
 *       - 2 个 super pod。
 */
nlohmann::json BuildOxcA3InitRankTable()
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
        {"task_id", "oxc_comm_init_a3_ut"},
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
void WriteOxcA3InitRankTableFile(const char *fileName)
{
    Ut_Clusterinfo_File_Create(fileName, BuildOxcA3InitRankTable());
}

/**
 * @brief 初始化 ClusterInfoConfig / MemConfig 路径使用的通信域配置。
 * @param config 输出配置。
 * @param commName 通信域名称。
 * @param enableRetry 是否打开 L2 retry 配置。
 */
void InitA3CommConfig(HcclCommConfig &config, const char *commName, bool enableRetry = false)
{
    HcclCommConfigInit(&config);
    ASSERT_EQ(strcpy_s(config.hcclCommName, sizeof(config.hcclCommName), commName), EOK);
    config.hcclOpExpansionMode = COMM_CONFIG_OPEXPANSION_AICPU;
    if (enableRetry) {
        ASSERT_EQ(strcpy_s(config.hcclRetryEnable, sizeof(config.hcclRetryEnable), "0,0,1"), EOK);
    }
}

/**
 * @brief 在单测作用域内临时设置环境变量，并在析构时恢复原值。
 */
class ScopedEnvVar {
public:
    /**
     * @brief 构造并覆盖目标环境变量。
     * @param name 环境变量名。
     * @param value 需要设置的值。
     */
    ScopedEnvVar(const char *name, const char *value) : name_(name)
    {
        const char *originValue = getenv(name);
        if (originValue != nullptr) {
            hadOrigin_ = true;
            originValue_ = originValue;
        }
        EXPECT_EQ(setenv(name_, value, 1), 0);
    }

    ~ScopedEnvVar()
    {
        if (hadOrigin_) {
            EXPECT_EQ(setenv(name_, originValue_.c_str(), 1), 0);
        } else {
            EXPECT_EQ(unsetenv(name_), 0);
        }
    }

private:
    const char *name_;
    bool hadOrigin_ {false};
    std::string originValue_;
};

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
 * @brief 断言指定 layered 层的首个 topo instance 可查询，且成员包含当前 rank。
 * @param comm 通信域句柄。
 * @param layer 需要检查的 layered 层。
 * @param userRank 当前视角 rank。
 */
void ExpectLayeredTopoInstContainsRank(HcclComm comm, uint32_t layer, uint32_t userRank)
{
    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(comm, layer, &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_NE(topoInsts, nullptr);
    EXPECT_GT(topoInstNum, 0U);

    uint32_t *ranks = nullptr;
    uint32_t rankNum = 0;
    ASSERT_EQ(HcclRankGraphGetRanksByTopoInst(comm, layer, topoInsts[0], &ranks, &rankNum), HCCL_SUCCESS);
    ASSERT_NE(ranks, nullptr);
    EXPECT_GT(rankNum, 0U);
    std::vector<uint32_t> rankVec(ranks, ranks + rankNum);
    EXPECT_NE(std::find(rankVec.begin(), rankVec.end(), userRank), rankVec.end());
}

/**
 * @brief 断言 world-comm 初始化后的 A3 OXC 通信域已经具备超节点语义，且 Layered 主线已被收口。
 * @param comm 通信域句柄。
 */
void ExpectOxcA3SocketRoute(HcclComm comm, HcclSocketType expectedSamePodCrossServerType)
{
    auto &communicator = GetCommunicatorFromComm(comm);

    HcclSocketType sameServerType = HcclSocketType::SOCKET_NIC;
    GetSocketTypeIn91093(communicator.rankInfoList_, communicator.useSuperPodMode_, 0, 1, sameServerType);
    EXPECT_EQ(sameServerType, HcclSocketType::SOCKET_VNIC);

    HcclSocketType samePodCrossServerType = HcclSocketType::SOCKET_VNIC;
    GetSocketTypeIn91093(communicator.rankInfoList_, communicator.useSuperPodMode_, 0, 2, samePodCrossServerType);
    EXPECT_EQ(samePodCrossServerType, expectedSamePodCrossServerType);

    HcclSocketType crossPodType = HcclSocketType::SOCKET_VNIC;
    GetSocketTypeIn91093(communicator.rankInfoList_, communicator.useSuperPodMode_, 0, 4, crossPodType);
    EXPECT_EQ(crossPodType, HcclSocketType::SOCKET_NIC);
}

/**
 * @brief 断言 heartbeat 在 inter-HCCS disable fallback 下实际消费 device_ip 构造 NIC 链路。
 * @param comm 已完成 world-comm 初始化的通信域句柄。
 */
void ExpectHeartbeatFallbackConsumesParameterPlaneIp(HcclComm comm)
{
    auto &communicator = GetCommunicatorFromComm(comm);
    ASSERT_GE(communicator.rankInfoList_.size(), 3U);

    auto &heartbeat = Heartbeat::GetInstance(0);
    heartbeat.rankId2LinkStatusMap_.clear();
    heartbeat.netDevCtxMap_.clear();
    heartbeat.uid_ = {};
    ASSERT_NE(
        snprintf_s(heartbeat.uid_.id, sizeof(heartbeat.uid_.id), sizeof(heartbeat.uid_.id) - 1, "%s/%d",
            communicator.rankInfoList_[0].serverId.c_str(), communicator.rankInfoList_[0].devicePhyId),
        -1);
    heartbeat.nicIp_ = communicator.rankInfoList_[0].nicIp[0];
    heartbeat.devicePhyId_ = communicator.rankInfoList_[0].devicePhyId;
    heartbeat.superDeviceId_ = communicator.rankInfoList_[0].superDeviceId;
    heartbeat.isUseRankPort_ = false;
    heartbeat.devPortSwitchOn_ = false;

    auto *netDevCtx = new NetDevContext();
    netDevCtx->nicType_ = NicType::DEVICE_NIC_TYPE;
    netDevCtx->devicePhyId_ = communicator.rankInfoList_[0].devicePhyId;
    netDevCtx->deviceLogicId_ = 0;
    netDevCtx->localIp_ = communicator.rankInfoList_[0].nicIp[0];
    heartbeat.netDevCtxMap_[heartbeat.nicIp_] = reinterpret_cast<HcclNetDevCtx>(netDevCtx);

    std::map<UIDType, ConnInfo> needConnectRank;
    RankInfo remoteRank = communicator.rankInfoList_[2];
    ASSERT_EQ(remoteRank.serverId, std::string("server1"));
    ASSERT_EQ(remoteRank.superPodId, communicator.rankInfoList_[0].superPodId);
    ASSERT_FALSE(remoteRank.nicIp.empty());
    ASSERT_FALSE(remoteRank.nicIp[0].IsInvalid());
    EXPECT_EQ(std::string(communicator.rankInfoList_[0].nicIp[0].GetReadableIP()), std::string("10.10.0.10"));
    EXPECT_EQ(std::string(remoteRank.nicIp[0].GetReadableIP()), std::string("10.10.0.12"));

    ASSERT_EQ(heartbeat.GetConnInfo(remoteRank, true, HcclSocketRole::SOCKET_ROLE_CLIENT, HcclSocketType::SOCKET_NIC,
        needConnectRank), HCCL_SUCCESS);
    ASSERT_EQ(needConnectRank.size(), 1U);

    const auto &connInfo = needConnectRank.begin()->second;
    ASSERT_NE(connInfo.socket, nullptr);
    EXPECT_EQ(connInfo.socket->GetLocalIp().GetReadableIP(), communicator.rankInfoList_[0].nicIp[0].GetReadableIP());
    EXPECT_EQ(connInfo.socket->GetRemoteIp().GetReadableIP(), remoteRank.nicIp[0].GetReadableIP());
    EXPECT_EQ(connInfo.socket->GetRemotePort(), HETEROG_CCL_PORT);

    needConnectRank.clear();
    heartbeat.netDevCtxMap_.clear();
    delete netDevCtx;
}

/**
 * @brief 断言 world-comm 初始化后的 A3 OXC 通信域已经具备超节点语义，且 Layered 查询重新可见。
 * @param comm 通信域句柄。
 * @param expectInterHccsMode 是否期望 communicator 处于 inter-HCCS 模式。
 * @param expectedSamePodCrossServerType 同 superPod、跨 server 时期望的 socket 选路。
 */
void ExpectOxcA3WorldCommReady(HcclComm comm, bool expectInterHccsMode = true,
    HcclSocketType expectedSamePodCrossServerType = HcclSocketType::SOCKET_VNIC)
{
    ASSERT_NE(comm, nullptr);
    auto &communicator = GetCommunicatorFromComm(comm);
    EXPECT_TRUE(communicator.useSuperPodMode_);
    EXPECT_EQ(communicator.isUsedInterHccsMode_, expectInterHccsMode);
    EXPECT_EQ(communicator.superPodNum_, 2U);
    ASSERT_FALSE(communicator.rankInfoList_.empty());
    ASSERT_FALSE(communicator.devBackupIpAddr_.empty());
    EXPECT_FALSE(communicator.devBackupIpAddr_[0].IsInvalid());

    HcclTopoAttr topoAttr = communicator.GetTopoAttr();
    EXPECT_TRUE(topoAttr.isOxcMode);
    EXPECT_TRUE(topoAttr.useSuperPodMode);
    EXPECT_EQ(topoAttr.superPodNum, 2U);
    ASSERT_EQ(topoAttr.rankInfoList.size(), 8U);
    EXPECT_EQ(topoAttr.rankInfoList[0].superPodId, std::string("pod0"));
    EXPECT_EQ(topoAttr.rankInfoList[4].superPodId, std::string("pod1"));

    uint32_t netPlaneId = UINT32_MAX;
    uint32_t netPlaneNum = 0;
    ASSERT_EQ(HcclGetNetPlaneId(comm, &netPlaneId), HCCL_SUCCESS);
    ASSERT_EQ(HcclGetNetPlaneNum(comm, &netPlaneNum), HCCL_SUCCESS);
    EXPECT_EQ(netPlaneNum, 2U);
    EXPECT_LT(netPlaneId, netPlaneNum);

    uint32_t *layers = nullptr;
    uint32_t layerNum = 0;
    ASSERT_EQ(HcclRankGraphGetLayers(comm, &layers, &layerNum), HCCL_SUCCESS);
    ASSERT_NE(layers, nullptr);
    std::vector<uint32_t> layerVec(layers, layers + layerNum);
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL1), layerVec.end());
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL2), layerVec.end());
    ExpectLayeredTopoInstContainsRank(comm, HCCL_NETLAYER_LAYERED_LEVEL1, communicator.userRank_);
    ExpectLayeredTopoInstContainsRank(comm, HCCL_NETLAYER_LAYERED_LEVEL2, communicator.userRank_);
    ExpectOxcA3SocketRoute(comm, expectedSamePodCrossServerType);
}
} // namespace

class HcclCommunicator91093OxcInitTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
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

class HcclCommunicator91093OxcClusterInfoConfigTest : public HcclCommunicator91093OxcInitTest {};
class HcclCommunicator91093OxcClusterInfoMemConfigTest : public HcclCommunicator91093OxcInitTest {};
class HcclCommunicator91093OxcMemConfigDeltaTest : public HcclCommunicator91093OxcInitTest {};

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_HcclCommInitClusterInfo_When_A3OxcRankTable_Expect_SuperPodInterHccsAndLayeredRankGraphRestored)
{
    WriteOxcA3InitRankTableFile(rankTableFileName);
    Ut_Device_Set(0);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &localComm), HCCL_SUCCESS);
    ExpectOxcA3WorldCommReady(localComm);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_HcclCommInitClusterInfo_When_A3OxcRetryEnvEnabled_Expect_RetryAndBackupCapabilityReady)
{
    {
        ScopedEnvVar retryEnableEnv("HCCL_OP_RETRY_ENABLE", "L1:0,L2:1");
        ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
        WriteOxcA3InitRankTableFile(rankTableFileName);
        Ut_Device_Set(0);

        HcclComm localComm = nullptr;
        ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &localComm), HCCL_SUCCESS);
        auto &communicator = GetCommunicatorFromComm(localComm);
        EXPECT_TRUE(communicator.commConfig_.GetConfigInterSuperPodRetryEnable());
        EXPECT_TRUE(communicator.GetAicpuUnfoldConfig());
        EXPECT_TRUE(communicator.retryEnable_);
        ASSERT_FALSE(communicator.devBackupIpAddr_.empty());
        EXPECT_FALSE(communicator.devBackupIpAddr_[0].IsInvalid());

        communicator.rtsSupportChangeLink_ = true;
        EXPECT_TRUE(communicator.IsEnableRoce());
        EXPECT_TRUE(communicator.IsEnableBackupLink());
        Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    }

    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_HcclCommInitClusterInfo_When_A3OxcInterHccsDisable_Expect_FallbackToParameterPlaneSocketRoute)
{
    ScopedEnvVar interHccsDisableEnv("HCCL_INTER_HCCS_DISABLE", "true");
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
    WriteOxcA3InitRankTableFile(rankTableFileName);
    Ut_Device_Set(0);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &localComm), HCCL_SUCCESS);
    ExpectOxcA3WorldCommReady(localComm, false, HcclSocketType::SOCKET_NIC);
    ExpectHeartbeatFallbackConsumesParameterPlaneIp(localComm);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcClusterInfoConfigTest,
    Ut_HcclCommInitClusterInfoConfig_When_A3OxcRankTable_Expect_SuperPodInterHccsAndLayeredRankGraphRestored)
{
    WriteOxcA3InitRankTableFile(rankTableFileName);
    Ut_Device_Set(0);

    HcclCommConfig config;
    InitA3CommConfig(config, "ut_a3_cfg_ready");

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoConfig(rankTableFileName, 0, &config, &localComm), HCCL_SUCCESS);
    ExpectOxcA3WorldCommReady(localComm);
    EXPECT_FALSE(IsOneSidedComm(localComm));
    EXPECT_FALSE(IsCommNameExistInOneSidedComms(0, config.hcclCommName));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
}

TEST_F(HcclCommunicator91093OxcClusterInfoConfigTest,
    Ut_HcclCommInitClusterInfoConfig_When_A3OxcRetryEnvEnabled_Expect_RetryAndAicpuReady)
{
    ScopedEnvVar retryEnableEnv("HCCL_OP_RETRY_ENABLE", "L1:0,L2:1");
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
    WriteOxcA3InitRankTableFile(rankTableFileName);
    Ut_Device_Set(0);

    HcclCommConfig config;
    InitA3CommConfig(config, "ut_a3_cfg_retry", true);

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoConfig(rankTableFileName, 0, &config, &localComm), HCCL_SUCCESS);
    auto &communicator = GetCommunicatorFromComm(localComm);
    ExpectOxcA3WorldCommReady(localComm);
    EXPECT_TRUE(communicator.commConfig_.GetConfigInterSuperPodRetryEnable());
    EXPECT_TRUE(communicator.GetAicpuUnfoldConfig());
    EXPECT_TRUE(communicator.retryEnable_);
    EXPECT_FALSE(IsOneSidedComm(localComm));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcClusterInfoMemConfigTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_A3OxcRankTable_Expect_SuperPodInterHccsAndLayeredRankGraphRestored)
{
    Ut_Device_Set(0);

    HcclCommConfig config;
    InitA3CommConfig(config, "ut_a3_memcfg_ready");
    const std::string rankTableString = BuildOxcA3InitRankTable().dump();

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), 0, &config, &localComm), HCCL_SUCCESS);
    ExpectOxcA3WorldCommReady(localComm);
    EXPECT_TRUE(IsOneSidedComm(localComm));
    EXPECT_TRUE(IsCommNameExistInOneSidedComms(0, config.hcclCommName));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    EXPECT_FALSE(IsCommNameExistInOneSidedComms(0, config.hcclCommName));
}

TEST_F(HcclCommunicator91093OxcClusterInfoMemConfigTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_A3OxcRetryEnvEnabled_Expect_RetryAndAicpuReady)
{
    ScopedEnvVar retryEnableEnv("HCCL_OP_RETRY_ENABLE", "L1:0,L2:1");
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
    Ut_Device_Set(0);

    HcclCommConfig config;
    InitA3CommConfig(config, "ut_a3_memcfg_retry", true);
    const std::string rankTableString = BuildOxcA3InitRankTable().dump();

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), 0, &config, &localComm), HCCL_SUCCESS);
    auto &communicator = GetCommunicatorFromComm(localComm);
    ExpectOxcA3WorldCommReady(localComm);
    EXPECT_TRUE(communicator.commConfig_.GetConfigInterSuperPodRetryEnable());
    EXPECT_TRUE(communicator.GetAicpuUnfoldConfig());
    EXPECT_TRUE(communicator.retryEnable_);
    EXPECT_TRUE(IsOneSidedComm(localComm));
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcMemConfigDeltaTest,
    Ut_HcclCommInitClusterInfoMemConfig_When_A3OxcRankTable_Expect_OneSidedDeltaBoundariesRemainControlled)
{
    Ut_Device_Set(0);

    HcclCommConfig config;
    InitA3CommConfig(config, "ut_a3_memcfg_delta");
    const std::string rankTableString = BuildOxcA3InitRankTable().dump();

    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), 0, &config, &localComm), HCCL_SUCCESS);
    auto &communicator = GetCommunicatorFromComm(localComm);
    EXPECT_TRUE(IsOneSidedComm(localComm));
    EXPECT_TRUE(IsCommNameExistInOneSidedComms(0, config.hcclCommName));
    EXPECT_TRUE(communicator.useSuperPodMode_);
    EXPECT_TRUE(communicator.isUsedInterHccsMode_);
    EXPECT_EQ(communicator.superPodNum_, 2U);

    HcclComm oneSidedComm = localComm;
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    EXPECT_FALSE(IsOneSidedComm(oneSidedComm));
    EXPECT_FALSE(IsCommNameExistInOneSidedComms(0, config.hcclCommName));
}
