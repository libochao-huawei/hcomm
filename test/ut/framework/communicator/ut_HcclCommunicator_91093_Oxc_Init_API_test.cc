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
#define private public
#define protected public
#include "coll_reduce_scatter_ring_layered_executor.h"
#include "coll_all_reduce_ring_layered_executor.h"
#include "coll_all_gather_ring_layered_executor.h"
#undef protected
#undef private

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
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
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
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
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


// -----------------------------------------------------------------------------
// OXC Ring high-fidelity proof helpers and tests
//
// First-pass scope only:
// - real-entry provenance from ClusterInfo family surfaces
// - representative diagonal matrix: RS×ClusterInfo, AR×Config, AG×MemConfig
// - residual broader shape/dtype/stride/topology expansion is intentionally deferred
// -----------------------------------------------------------------------------

namespace {
using namespace hccl;

constexpr u64 HF_RS_AR_COUNT = 8U;
constexpr u64 HF_AG_COUNT = 2U;
constexpr u64 HF_CCL_BUFFER_SIZE = 64U * 1024U * 1024U;
constexpr u32 HF_PROOF_USER_RANK = 0U;
constexpr u32 HF_PROOF_RANK_SIZE = 4U;
constexpr u32 HF_TEST_SQ_HEAD = 0U;
constexpr u32 HF_TEST_SQ_TAIL = 100U;
u8 g_hfSqAddr[HCCL_SQE_SIZE * HCCL_SQE_MAX_CNT] = {0};

enum class OxcRingHighFidelityEntryRoute {
    CLUSTER_INFO = 0,
    CONFIG,
    MEM_CONFIG,
};

enum class OxcRingHighFidelityClassification {
    REAL_ENTRY_HIGH_FIDELITY = 0,
    BRIDGE_FALLBACK,
};

struct OxcRingHighFidelitySnapshot {
    OxcRingHighFidelityEntryRoute route {OxcRingHighFidelityEntryRoute::CLUSTER_INFO};
    std::string routeName;
    std::string commName;
    HcclTopoAttr topoAttr {};
    HcclAlgoAttr algoAttr {};
    TopoType topoType {TopoType::TOPO_TYPE_RESERVED};
    u32 netPlaneId {0};
    u32 netPlaneNum {1};
    bool isOneSided {false};
    bool commNameRegistered {false};
    std::vector<u32> layers;
};

nlohmann::json BuildOxcA3HighFidelityRankTable()
{
    auto buildDevice = [](uint32_t rankId, uint32_t deviceId, uint32_t superDeviceId) {
        return nlohmann::json::object({
            {"device_id", std::to_string(deviceId)},
            {"super_device_id", std::to_string(superDeviceId)},
            {"rankid", std::to_string(rankId)},
            {"device_ip", std::string("10.10.0.") + std::to_string(10 + rankId)},
            {"backup_device_ip", std::string("10.20.0.") + std::to_string(10 + rankId)},
            {"device_port", 16666 + static_cast<int>(rankId)},
            {"backup_device_port", 17666 + static_cast<int>(rankId)}
        });
    };

    nlohmann::json serverList = nlohmann::json::array({
        nlohmann::json::object({
            {"server_id", "server0"},
            {"host_ip", "192.168.10.10"},
            {"device", nlohmann::json::array({
                buildDevice(0U, 0U, 100U),
                buildDevice(1U, 1U, 101U)
            })}
        }),
        nlohmann::json::object({
            {"server_id", "server1"},
            {"host_ip", "192.168.10.11"},
            {"device", nlohmann::json::array({
                buildDevice(2U, 0U, 200U),
                buildDevice(3U, 1U, 201U)
            })}
        })
    });

    return nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "oxc_hf_proof_ut"},
        {"server_list", serverList},
        {"super_pod_list", nlohmann::json::array({
            nlohmann::json::object({{"super_pod_id", "pod0"}, {"server_list", nlohmann::json::array({nlohmann::json::object({{"server_id", "server0"}})})}}),
            nlohmann::json::object({{"super_pod_id", "pod1"}, {"server_list", nlohmann::json::array({nlohmann::json::object({{"server_id", "server1"}})})}})
        })},
        {"oxc_group_list", nlohmann::json::array({
            nlohmann::json::object({{"group_id", "group0"}, {"rank_list", nlohmann::json::array({
                nlohmann::json::object({{"rank_id", "0"}}),
                nlohmann::json::object({{"rank_id", "1"}})
            })}}),
            nlohmann::json::object({{"group_id", "group1"}, {"rank_list", nlohmann::json::array({
                nlohmann::json::object({{"rank_id", "2"}}),
                nlohmann::json::object({{"rank_id", "3"}})
            })}})
        })}
    });
}

void WriteOxcA3HighFidelityRankTableFile(const char *fileName)
{
    Ut_Clusterinfo_File_Create(fileName, BuildOxcA3HighFidelityRankTable());
}

std::vector<u32> CaptureVisibleLayers(HcclComm comm)
{
    uint32_t *layers = nullptr;
    uint32_t layerNum = 0;
    EXPECT_EQ(HcclRankGraphGetLayers(comm, &layers, &layerNum), HCCL_SUCCESS);
    EXPECT_NE(layers, nullptr);
    if (layers == nullptr) {
        return {};
    }
    return std::vector<u32>(layers, layers + layerNum);
}

OxcRingHighFidelitySnapshot CaptureOxcRingHighFidelitySnapshot(HcclComm comm,
    OxcRingHighFidelityEntryRoute route, const std::string &commName = std::string())
{
    OxcRingHighFidelitySnapshot snapshot;
    snapshot.route = route;
    snapshot.commName = commName;
    snapshot.routeName = (route == OxcRingHighFidelityEntryRoute::CLUSTER_INFO) ? "clusterinfo" :
        ((route == OxcRingHighFidelityEntryRoute::CONFIG) ? "config" : "memconfig");

    auto &communicator = GetCommunicatorFromComm(comm);
    snapshot.topoAttr = communicator.GetTopoAttr();
    communicator.attrCollector_.GetAlgoAttr(snapshot.algoAttr);
    EXPECT_NE(communicator.implAlg_.get(), nullptr);
    if (communicator.implAlg_ != nullptr) {
        EXPECT_EQ(communicator.implAlg_->GetCommPlaneRanks(snapshot.topoAttr.commPlaneRanks), HCCL_SUCCESS);
        EXPECT_EQ(communicator.implAlg_->GetTopoType(snapshot.topoType), HCCL_SUCCESS);
    }
    snapshot.layers = CaptureVisibleLayers(comm);
    EXPECT_EQ(HcclGetNetPlaneId(comm, &snapshot.netPlaneId), HCCL_SUCCESS);
    EXPECT_EQ(HcclGetNetPlaneNum(comm, &snapshot.netPlaneNum), HCCL_SUCCESS);
    snapshot.topoAttr.netPlaneId = snapshot.netPlaneId;
    snapshot.topoAttr.netPlaneNum = snapshot.netPlaneNum;
    snapshot.isOneSided = IsOneSidedComm(comm);
    snapshot.commNameRegistered = !commName.empty() && IsCommNameExistInOneSidedComms(0, commName);
    return snapshot;
}

void ExpectOxcRingHighFidelityEntryReady(const OxcRingHighFidelitySnapshot &snapshot)
{
    EXPECT_TRUE(snapshot.topoAttr.isOxcMode);
    EXPECT_EQ(snapshot.topoAttr.userRank, HF_PROOF_USER_RANK);
    EXPECT_EQ(snapshot.topoAttr.userRankSize, HF_PROOF_RANK_SIZE);
    EXPECT_TRUE(snapshot.topoAttr.useSuperPodMode);
    EXPECT_EQ(snapshot.topoAttr.superPodNum, 2U);
    EXPECT_EQ(snapshot.topoAttr.netPlaneId, snapshot.netPlaneId);
    EXPECT_EQ(snapshot.topoAttr.netPlaneNum, snapshot.netPlaneNum);
    EXPECT_EQ(snapshot.netPlaneNum, 2U);
    EXPECT_LT(snapshot.netPlaneId, snapshot.netPlaneNum);
    EXPECT_TRUE(snapshot.algoAttr.isUsedInterHccsMode);
    EXPECT_NE(std::find(snapshot.layers.begin(), snapshot.layers.end(), HCCL_NETLAYER_LAYERED_LEVEL1), snapshot.layers.end());
    EXPECT_NE(std::find(snapshot.layers.begin(), snapshot.layers.end(), HCCL_NETLAYER_LAYERED_LEVEL2), snapshot.layers.end());
    ASSERT_GT(snapshot.topoAttr.commPlaneRanks.size(), COMM_LAYERED_LEVEL2);
    EXPECT_FALSE(snapshot.topoAttr.commPlaneRanks[COMM_LEVEL0].empty());
    EXPECT_FALSE(snapshot.topoAttr.commPlaneRanks[COMM_LAYERED_LEVEL1].empty());
    EXPECT_FALSE(snapshot.topoAttr.commPlaneRanks[COMM_LAYERED_LEVEL2].empty());
}

OxcRingHighFidelityClassification ClassifyOxcRingHighFidelitySnapshot(const OxcRingHighFidelitySnapshot &snapshot)
{
    if (!snapshot.topoAttr.isOxcMode || snapshot.netPlaneNum == 0U || snapshot.netPlaneId >= snapshot.netPlaneNum) {
        return OxcRingHighFidelityClassification::BRIDGE_FALLBACK;
    }
    if (snapshot.topoAttr.commPlaneRanks.size() <= COMM_LAYERED_LEVEL2) {
        return OxcRingHighFidelityClassification::BRIDGE_FALLBACK;
    }
    if (snapshot.topoAttr.commPlaneRanks[COMM_LEVEL0].empty() ||
        snapshot.topoAttr.commPlaneRanks[COMM_LAYERED_LEVEL1].empty() ||
        snapshot.topoAttr.commPlaneRanks[COMM_LAYERED_LEVEL2].empty()) {
        return OxcRingHighFidelityClassification::BRIDGE_FALLBACK;
    }
    if (std::find(snapshot.layers.begin(), snapshot.layers.end(), HCCL_NETLAYER_LAYERED_LEVEL1) == snapshot.layers.end() ||
        std::find(snapshot.layers.begin(), snapshot.layers.end(), HCCL_NETLAYER_LAYERED_LEVEL2) == snapshot.layers.end()) {
        return OxcRingHighFidelityClassification::BRIDGE_FALLBACK;
    }
    return OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY;
}

void ExpectOxcRingHighFidelitySnapshotUnchanged(const OxcRingHighFidelitySnapshot &before,
    const OxcRingHighFidelitySnapshot &after)
{
    EXPECT_EQ(before.routeName, after.routeName);
    EXPECT_EQ(before.commName, after.commName);
    EXPECT_EQ(before.netPlaneId, after.netPlaneId);
    EXPECT_EQ(before.netPlaneNum, after.netPlaneNum);
    EXPECT_EQ(before.topoType, after.topoType);
    EXPECT_EQ(before.topoAttr.useSuperPodMode, after.topoAttr.useSuperPodMode);
    EXPECT_EQ(before.topoAttr.superPodNum, after.topoAttr.superPodNum);
    EXPECT_EQ(before.algoAttr.isUsedInterHccsMode, after.algoAttr.isUsedInterHccsMode);
    EXPECT_EQ(before.topoAttr.isOxcMode, after.topoAttr.isOxcMode);
    EXPECT_EQ(before.topoAttr.userRank, after.topoAttr.userRank);
    EXPECT_EQ(before.topoAttr.userRankSize, after.topoAttr.userRankSize);
    EXPECT_EQ(before.topoAttr.commPlaneRanks, after.topoAttr.commPlaneRanks);
    EXPECT_EQ(before.layers, after.layers);
}

class OxcHighFidelityDummyTransportBase final : public TransportBase {
public:
    OxcHighFidelityDummyTransportBase() : TransportBase(nullptr, DummyNotifyPool(), DummyMachinePara(), std::chrono::milliseconds(0)) {}
    HcclResult TxAsync(std::vector<TxMemoryInfo> &txMems, Stream &stream) override {(void)txMems;(void)stream;return HCCL_SUCCESS;}
    HcclResult RxAsync(std::vector<RxMemoryInfo> &rxMems, Stream &stream) override {(void)rxMems;(void)stream;return HCCL_SUCCESS;}
    HcclResult TxAck(Stream &stream) override {(void)stream;return HCCL_SUCCESS;}
    HcclResult RxAck(Stream &stream) override {(void)stream;return HCCL_SUCCESS;}
    HcclResult TxWaitDone(Stream &stream) override {(void)stream;return HCCL_SUCCESS;}
    HcclResult RxWaitDone(Stream &stream) override {(void)stream;return HCCL_SUCCESS;}
private:
    static std::unique_ptr<NotifyPool> &DummyNotifyPool(){ static std::unique_ptr<NotifyPool> pool = nullptr; return pool; }
    static MachinePara &DummyMachinePara(){ static MachinePara para {}; return para; }
};

LINK BuildOxcHighFidelityDummyLink()
{
    return std::make_shared<Transport>(new (std::nothrow) OxcHighFidelityDummyTransportBase());
}

SingleSubCommTransport BuildOxcHighFidelitySingleSubCommTransport(const std::vector<u32> &userRanks, u32 selfUserRank)
{
    SingleSubCommTransport transport;
    transport.transportRequests.resize(userRanks.size());
    transport.links.resize(userRanks.size());
    transport.status.resize(userRanks.size(), TransportStatus::READY);
    for (u32 i = 0; i < userRanks.size(); ++i) {
        transport.userRank2subCommRank[userRanks[i]] = i;
        transport.subCommRank2UserRank[i] = userRanks[i];
        transport.links[i] = BuildOxcHighFidelityDummyLink();
        transport.transportRequests[i].isValid = true;
        transport.transportRequests[i].localUserRank = selfUserRank;
        transport.transportRequests[i].remoteUserRank = userRanks[i];
    }
    return transport;
}

HcclResult HcclD2DMemcpyAsyncStub(HcclDispatcher, DeviceMem &dst, const DeviceMem &src, Stream &)
{
    CHK_PTR_NULL(dst.ptr());
    CHK_PTR_NULL(src.ptr());
    const size_t copySize = static_cast<size_t>(std::min(dst.size(), src.size()));
    const errno_t ret = memcpy_s(dst.ptr(), copySize, src.ptr(), copySize);
    return (ret == EOK) ? HCCL_SUCCESS : HCCL_E_INTERNAL;
}

AlgResourceResponse BuildOxcHighFidelityAlgResourceResponse(const OxcRingHighFidelitySnapshot &snapshot)
{
    AlgResourceResponse algRes;
    algRes.opTransportResponse.resize(static_cast<u32>(COMM_LEVEL_RESERVED));
    const std::array<CommPlane, 3> planes = {COMM_LEVEL0, COMM_LAYERED_LEVEL1, COMM_LAYERED_LEVEL2};
    for (CommPlane plane : planes) {
        const u32 planeIndex = static_cast<u32>(plane);
        for (const auto &subComm : snapshot.topoAttr.commPlaneRanks[planeIndex]) {
            if (subComm.empty()) {
                continue;
            }
            algRes.opTransportResponse[planeIndex].push_back(
                BuildOxcHighFidelitySingleSubCommTransport(subComm, snapshot.topoAttr.userRank));
        }
    }
    return algRes;
}

class OxcRingHighFidelityDispatcher final : public DispatcherPub {
public:
    explicit OxcRingHighFidelityDispatcher() : DispatcherPub(0) {}
    HcclResult Init() override { return HCCL_SUCCESS; }
    HcclResult MemcpyAsync(DeviceMem &dst, const DeviceMem &src, Stream &stream,
        u32 remoteUserRank = INVALID_VALUE_RANKID,
        LinkType inLinkType = LinkType::LINK_ONCHIP) override
    {
        (void)stream;
        (void)remoteUserRank;
        (void)inLinkType;
        return HcclD2DMemcpyAsyncStub(nullptr, dst, src, stream);
    }
    HcclResult InlineReduceAsync(const void *src, u64 count, const HcclDataType datatype, HcclReduceOp redOp,
        Stream &stream, void *dst, u32 remoteUserRank = INVALID_VALUE_RANKID,
        LinkType inLinkType = LinkType::LINK_ONCHIP) override
    {
        (void)remoteUserRank;
        (void)inLinkType;
        return ReduceIntoDst(src, dst, count, datatype, redOp, stream);
    }
    HcclResult ReduceAsync(const void *src, void *dst, u64 dataCount, const HcclDataType datatype,
        HcclReduceOp redOp, Stream &stream,
        HcclReduceType reduceType = HcclReduceType::HCCL_TBE_REDUCE) override
    {
        (void)reduceType;
        return ReduceIntoDst(src, dst, dataCount, datatype, redOp, stream);
    }
private:
    HcclResult ReduceIntoDst(const void *src, void *dst, u64 dataCount,
        const HcclDataType datatype, HcclReduceOp redOp, Stream &stream)
    {
        (void)stream;
        if (datatype != HCCL_DATA_TYPE_INT32 || redOp != HCCL_REDUCE_SUM) {
            return HCCL_E_NOT_SUPPORT;
        }
        auto *srcPtr = static_cast<const int *>(src);
        auto *dstPtr = static_cast<int *>(dst);
        CHK_PTR_NULL(srcPtr);
        CHK_PTR_NULL(dstPtr);
        for (u64 i = 0; i < dataCount; ++i) {
            dstPtr[i] += srcPtr[i];
        }
        return HCCL_SUCCESS;
    }
};

Stream BuildOxcHighFidelityTestStream()
{
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 128;
    streamInfo.sqBaseAddr = static_cast<void *>(g_hfSqAddr);
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, true);
    static SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(HF_TEST_SQ_HEAD, HF_TEST_SQ_TAIL, &sqeCqeCtx);
    return stream;
}

std::unique_ptr<TopoMatcher> BuildOxcHighFidelityTopoMatcher(const OxcRingHighFidelitySnapshot &snapshot)
{
    HcclTopoInfo topoInfo;
    topoInfo.userRank = snapshot.topoAttr.userRank;
    topoInfo.realUserRank = snapshot.topoAttr.realUserRank;
    topoInfo.userRankSize = snapshot.topoAttr.userRankSize;
    topoInfo.serverNum = snapshot.topoAttr.serverNum;
    topoInfo.superPodNum = snapshot.topoAttr.superPodNum;
    topoInfo.deviceNumPerAggregation = snapshot.topoAttr.deviceNumPerAggregation;
    topoInfo.deviceType = snapshot.topoAttr.deviceType;
    topoInfo.topoType = snapshot.topoType;
    topoInfo.devicePhyId = snapshot.topoAttr.devicePhyId;
    topoInfo.deviceLogicId = snapshot.topoAttr.deviceLogicId;
    topoInfo.useSuperPodMode = snapshot.topoAttr.useSuperPodMode;
    topoInfo.meshAggregationRankSize = snapshot.topoAttr.meshAggregationRankSize;
    topoInfo.moduleNum = snapshot.topoAttr.moduleNum;
    topoInfo.isSingleMeshAggregation = snapshot.topoAttr.isSingleMeshAggregation;
    topoInfo.isDiffDeviceModule = snapshot.topoAttr.isDiffDeviceModule;
    topoInfo.isDiffDeviceType = snapshot.topoAttr.isDiffDeviceType;
    topoInfo.gcdDeviceNumPerAggregation = snapshot.topoAttr.gcdDeviceNumPerAggregation;
    topoInfo.nicList = snapshot.topoAttr.nicList;
    topoInfo.pairLinkCounter = snapshot.topoAttr.pairLinkCounter;
    topoInfo.netPlaneId = snapshot.netPlaneId;
    topoInfo.netPlaneNum = snapshot.netPlaneNum;

    HcclAlgoInfo algoInfo;
    algoInfo.inlineReduceSwitchOn = snapshot.algoAttr.inlineReduceSwitchOn;
    algoInfo.identifier = snapshot.algoAttr.identifier;
    algoInfo.isUsedRdmaLevel0 = snapshot.algoAttr.isUsedRdmaLevel0;

    HcclExternalEnable externalEnable;
    externalEnable.algoConfig = snapshot.algoAttr.commAlgoConfig;

    std::vector<bool> isBridgeVector;
    std::vector<std::vector<std::vector<u32>>> serverAndSuperPodToRank;
    return std::unique_ptr<TopoMatcher>(new (std::nothrow) TopoMatcher(snapshot.topoAttr.commPlaneRanks,
        isBridgeVector, topoInfo, algoInfo, externalEnable, serverAndSuperPodToRank));
}

class OxcRingHighFidelityReduceScatterExecutorProbe : public CollReduceScatterRingLayeredExecutor {
public:
    explicit OxcRingHighFidelityReduceScatterExecutorProbe(const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollReduceScatterRingLayeredExecutor(dispatcher, topoMatcher) {}

    using CollReduceScatterRingLayeredExecutor::KernelRun;
    using CollReduceScatterRingLayeredExecutor::algResResp_;
    using CollReduceScatterRingLayeredExecutor::workflowMode_;
    using CollReduceScatterRingLayeredExecutor::tag_;

    std::vector<std::string> evidence;

protected:
    HcclResult ActiveSlaveStreamsForTest(const Stream &stream) override {(void)stream; evidence.push_back("active-slave-streams"); return HCCL_SUCCESS;}
    HcclResult MultiRingReduceScatterForTest(const std::string &, DeviceMem inputMem, DeviceMem,
        u64, HcclDataType, HcclReduceOp, const std::vector<std::vector<Slice>> &, Stream, s32, u64,
        const HcomCollOpInfo *, const std::vector<std::vector<Slice>> &) override
    {
        evidence.push_back("outer-rs");
        int *inputSeed = static_cast<int *>(inputMem.ptr());
        const int values[8] = {1111,1112,1113,1114,1115,1116,1117,1118};
        for (int i = 0; i < 8; ++i) {
            inputSeed[i] = values[i];
        }
        return HCCL_SUCCESS;
    }
};

class OxcRingHighFidelityAllReduceExecutorProbe : public CollAllReduceRingLayeredExecutor {
public:
    explicit OxcRingHighFidelityAllReduceExecutorProbe(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollAllReduceRingLayeredExecutor(dispatcher, topoMatcher) {}

    using CollAllReduceRingLayeredExecutor::KernelRun;
    using CollAllReduceRingLayeredExecutor::algResResp_;
    using CollAllReduceRingLayeredExecutor::workflowMode_;
    using CollAllReduceRingLayeredExecutor::tag_;

    std::vector<std::string> evidence;

protected:
    HcclResult ActiveSlaveStreamsForTest(const Stream &stream) override {(void)stream; evidence.push_back("active-slave-streams"); return HCCL_SUCCESS;}
    HcclResult MultiRingReduceScatterForTest(const std::string &, DeviceMem, DeviceMem outputMem, u64,
        HcclDataType, HcclReduceOp, const std::vector<std::vector<Slice>> &, Stream, s32, u64,
        const HcomCollOpInfo *) override
    {
        evidence.push_back("outer-rs");
        int *buffer = static_cast<int *>(outputMem.ptr());
        const int values[8] = {101,102,201,202,301,302,401,402};
        for (int i = 0; i < 8; ++i) {
            buffer[i] = values[i];
        }
        return HCCL_SUCCESS;
    }
    HcclResult MultiRingAllGatherForTest(const std::string &, DeviceMem, DeviceMem outputMem, u64,
        HcclDataType, const std::vector<std::vector<Slice>> &, Stream, s32, u64,
        const HcomCollOpInfo *) override
    {
        evidence.push_back("outer-ag");
        int *buffer = static_cast<int *>(outputMem.ptr());
        const int values[8] = {1001,1002,1003,1004,1005,1006,1007,1008};
        for (int i = 0; i < 8; ++i) {
            buffer[i] = values[i];
        }
        return HCCL_SUCCESS;
    }
};

class OxcRingHighFidelityAllGatherExecutorProbe : public CollAllGatherRingLayeredExecutor {
public:
    explicit OxcRingHighFidelityAllGatherExecutorProbe(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher)
        : CollAllGatherRingLayeredExecutor(dispatcher, topoMatcher) {}

    using CollAllGatherRingLayeredExecutor::KernelRun;
    using CollAllGatherRingLayeredExecutor::algResResp_;
    using CollAllGatherRingLayeredExecutor::workflowMode_;
    using CollAllGatherRingLayeredExecutor::tag_;

    std::vector<std::string> evidence;

protected:
    HcclResult ActiveSlaveStreamsForTest(const Stream &stream) override {(void)stream; evidence.push_back("active-slave-streams"); return HCCL_SUCCESS;}
    HcclResult MultiRingAllGatherForTest(const std::string &, DeviceMem, DeviceMem outputMem, u64,
        HcclDataType, const std::vector<std::vector<Slice>> &, Stream, s32) override
    {
        evidence.push_back("outer-ag");
        int *buffer = static_cast<int *>(outputMem.ptr());
        const int values[32] = {
            21,22,61,62,101,102,141,142,
            41,42,81,82,121,122,161,162,
            31,32,71,72,111,112,151,152,
            51,52,91,92,131,132,171,172
        };
        for (int i = 0; i < 32; ++i) {
            buffer[i] = values[i];
        }
        return HCCL_SUCCESS;
    }
};

} // namespace

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_OxcRingHighFidelityHarnessContract_When_ClusterInfoFamilyParified_Expect_RealEntryStatePreserved)
{
    WriteOxcA3HighFidelityRankTableFile(rankTableFileName);
    Ut_Device_Set(0);

    HcclComm fileComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &fileComm), HCCL_SUCCESS);
    const OxcRingHighFidelitySnapshot fileSnapshot =
        CaptureOxcRingHighFidelitySnapshot(fileComm, OxcRingHighFidelityEntryRoute::CLUSTER_INFO);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(fileComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);

    HcclCommConfig cfg;
    InitA3CommConfig(cfg, "ut_hf_cfg");
    HcclComm configComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoConfig(rankTableFileName, 0, &cfg, &configComm), HCCL_SUCCESS);
    const OxcRingHighFidelitySnapshot configSnapshot =
        CaptureOxcRingHighFidelitySnapshot(configComm, OxcRingHighFidelityEntryRoute::CONFIG, "ut_hf_cfg");
    Ut_Comm_Destroy(reinterpret_cast<void *&>(configComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);

    HcclCommConfig memCfg;
    InitA3CommConfig(memCfg, "ut_hf_memcfg");
    const std::string rankTableString = BuildOxcA3HighFidelityRankTable().dump();
    HcclComm memComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), 0, &memCfg, &memComm), HCCL_SUCCESS);
    const OxcRingHighFidelitySnapshot memSnapshot =
        CaptureOxcRingHighFidelitySnapshot(memComm, OxcRingHighFidelityEntryRoute::MEM_CONFIG, "ut_hf_memcfg");

    ExpectOxcRingHighFidelityEntryReady(fileSnapshot);
    ExpectOxcRingHighFidelityEntryReady(configSnapshot);
    ExpectOxcRingHighFidelityEntryReady(memSnapshot);

    EXPECT_EQ(ClassifyOxcRingHighFidelitySnapshot(fileSnapshot), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);
    EXPECT_EQ(ClassifyOxcRingHighFidelitySnapshot(configSnapshot), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);
    EXPECT_EQ(ClassifyOxcRingHighFidelitySnapshot(memSnapshot), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);

    EXPECT_EQ(fileSnapshot.netPlaneId, configSnapshot.netPlaneId);
    EXPECT_EQ(fileSnapshot.netPlaneId, memSnapshot.netPlaneId);
    EXPECT_EQ(fileSnapshot.netPlaneNum, configSnapshot.netPlaneNum);
    EXPECT_EQ(fileSnapshot.netPlaneNum, memSnapshot.netPlaneNum);
    EXPECT_EQ(fileSnapshot.topoAttr.useSuperPodMode, configSnapshot.topoAttr.useSuperPodMode);
    EXPECT_EQ(fileSnapshot.topoAttr.useSuperPodMode, memSnapshot.topoAttr.useSuperPodMode);
    EXPECT_EQ(fileSnapshot.topoAttr.superPodNum, configSnapshot.topoAttr.superPodNum);
    EXPECT_EQ(fileSnapshot.topoAttr.superPodNum, memSnapshot.topoAttr.superPodNum);
    EXPECT_EQ(fileSnapshot.algoAttr.isUsedInterHccsMode, configSnapshot.algoAttr.isUsedInterHccsMode);
    EXPECT_EQ(fileSnapshot.algoAttr.isUsedInterHccsMode, memSnapshot.algoAttr.isUsedInterHccsMode);
    EXPECT_EQ(fileSnapshot.topoAttr.commPlaneRanks, configSnapshot.topoAttr.commPlaneRanks);
    EXPECT_EQ(fileSnapshot.topoAttr.commPlaneRanks, memSnapshot.topoAttr.commPlaneRanks);
    EXPECT_FALSE(fileSnapshot.isOneSided);
    EXPECT_FALSE(configSnapshot.isOneSided);
    EXPECT_TRUE(memSnapshot.isOneSided);
    EXPECT_TRUE(memSnapshot.commNameRegistered);

    HcclComm oneSidedComm = memComm;
    Ut_Comm_Destroy(reinterpret_cast<void *&>(memComm));
    EXPECT_FALSE(IsOneSidedComm(oneSidedComm));
    EXPECT_FALSE(IsCommNameExistInOneSidedComms(0, memCfg.hcclCommName));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_OxcRingHighFidelityHarnessContract_When_EntryDerivedFieldsMutated_Expect_BridgeFallback)
{
    WriteOxcA3HighFidelityRankTableFile(rankTableFileName);
    Ut_Device_Set(0);
    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &localComm), HCCL_SUCCESS);
    OxcRingHighFidelitySnapshot mutated = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::CLUSTER_INFO);
    ExpectOxcRingHighFidelityEntryReady(mutated);
    ASSERT_EQ(ClassifyOxcRingHighFidelitySnapshot(mutated), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);

    mutated.netPlaneNum = 0U;
    EXPECT_EQ(ClassifyOxcRingHighFidelitySnapshot(mutated), OxcRingHighFidelityClassification::BRIDGE_FALLBACK);

    mutated = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::CLUSTER_INFO);
    ASSERT_GT(mutated.topoAttr.commPlaneRanks.size(), COMM_LAYERED_LEVEL2);
    mutated.topoAttr.commPlaneRanks[COMM_LAYERED_LEVEL1].clear();
    EXPECT_EQ(ClassifyOxcRingHighFidelitySnapshot(mutated), OxcRingHighFidelityClassification::BRIDGE_FALLBACK);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_OxcRingHighFidelityReduceScatter_When_ClusterInfoEntry_Expect_CommInitToLayeredOutputCorrect)
{
    MOCKER(HcclD2DMemcpyAsync).stubs().with(any(), any(), any(), any()).will(invoke(HcclD2DMemcpyAsyncStub));
    WriteOxcA3HighFidelityRankTableFile(rankTableFileName);
    Ut_Device_Set(0);
    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfo(rankTableFileName, 0, &localComm), HCCL_SUCCESS);
    const OxcRingHighFidelitySnapshot before = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::CLUSTER_INFO);
    ExpectOxcRingHighFidelityEntryReady(before);
    ASSERT_EQ(ClassifyOxcRingHighFidelitySnapshot(before), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);

    std::unique_ptr<TopoMatcher> topoMatcher = BuildOxcHighFidelityTopoMatcher(before);
    ASSERT_NE(topoMatcher, nullptr);
    OxcRingHighFidelityDispatcher dispatcher;
    ASSERT_EQ(dispatcher.Init(), HCCL_SUCCESS);
    OxcRingHighFidelityReduceScatterExecutorProbe executor(reinterpret_cast<HcclDispatcher>(&dispatcher), topoMatcher);
    ASSERT_EQ(executor.SetAlgType(AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING,
        AlgTypeLevel1::ALG_LEVEL1_RING, AlgTypeLevel2::ALG_LEVEL2_RING)), HCCL_SUCCESS);
    executor.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    executor.tag_ = "oxc_hf_rs";
    AlgResourceResponse algRes = BuildOxcHighFidelityAlgResourceResponse(before);
    executor.algResResp_ = &algRes;

    int input[HF_RS_AR_COUNT * 2] = {0};
    for (int i = 0; i < static_cast<int>(HF_RS_AR_COUNT); ++i) {
        input[i] = 100 + i;
    }
    int output[HF_RS_AR_COUNT] = {0};
    int scratch[HF_RS_AR_COUNT * 2] = {0};
    int userOutput[HF_RS_AR_COUNT] = {0};
    ExecMem execMem;
    execMem.count = HF_RS_AR_COUNT;
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.inputPtr = input;
    execMem.outputPtr = userOutput;

    OpParam param;
    param.tag = "oxc_hf_rs";
    param.opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    param.DataDes.count = HF_RS_AR_COUNT;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
    param.reduceType = HCCL_REDUCE_SUM;
    param.stream = BuildOxcHighFidelityTestStream();

    ASSERT_EQ(executor.KernelRun(param, execMem), HCCL_SUCCESS);
    EXPECT_EQ(executor.evidence, (std::vector<std::string>{"active-slave-streams", "outer-rs"}));
    const int expected[8] = {1111,1112,1113,1114,1115,1116,1117,1118};
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(userOutput[i], expected[i]);
    }
    const OxcRingHighFidelitySnapshot after = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::CLUSTER_INFO);
    ExpectOxcRingHighFidelitySnapshotUnchanged(before, after);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_OxcRingHighFidelityAllReduce_When_ClusterInfoConfigEntry_Expect_CommInitToLayeredOutputCorrect)
{
    MOCKER(HcclD2DMemcpyAsync).stubs().with(any(), any(), any(), any()).will(invoke(HcclD2DMemcpyAsyncStub));
    WriteOxcA3HighFidelityRankTableFile(rankTableFileName);
    Ut_Device_Set(0);
    HcclCommConfig config;
    InitA3CommConfig(config, "ut_oxc_hf_ar");
    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoConfig(rankTableFileName, 0, &config, &localComm), HCCL_SUCCESS);
    const OxcRingHighFidelitySnapshot before = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::CONFIG, "ut_oxc_hf_ar");
    ExpectOxcRingHighFidelityEntryReady(before);
    ASSERT_EQ(ClassifyOxcRingHighFidelitySnapshot(before), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);

    std::unique_ptr<TopoMatcher> topoMatcher = BuildOxcHighFidelityTopoMatcher(before);
    ASSERT_NE(topoMatcher, nullptr);
    OxcRingHighFidelityDispatcher dispatcher;
    ASSERT_EQ(dispatcher.Init(), HCCL_SUCCESS);
    OxcRingHighFidelityAllReduceExecutorProbe executor(reinterpret_cast<HcclDispatcher>(&dispatcher), topoMatcher);
    ASSERT_EQ(executor.SetAlgType(AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING,
        AlgTypeLevel1::ALG_LEVEL1_RING, AlgTypeLevel2::ALG_LEVEL2_RING)), HCCL_SUCCESS);
    executor.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    executor.tag_ = "oxc_hf_ar";
    AlgResourceResponse algRes = BuildOxcHighFidelityAlgResourceResponse(before);
    executor.algResResp_ = &algRes;

    int input[HF_RS_AR_COUNT] = {1,2,3,4,5,6,7,8};
    int output[HF_RS_AR_COUNT] = {0};
    int scratch[HF_RS_AR_COUNT] = {0};
    int userOutput[HF_RS_AR_COUNT] = {0};
    ExecMem execMem;
    execMem.count = HF_RS_AR_COUNT;
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.inputPtr = input;
    execMem.outputPtr = userOutput;

    OpParam param;
    param.tag = "oxc_hf_ar";
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = HF_RS_AR_COUNT;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
    param.reduceType = HCCL_REDUCE_SUM;
    param.stream = BuildOxcHighFidelityTestStream();

    ASSERT_EQ(executor.KernelRun(param, execMem), HCCL_SUCCESS);
    EXPECT_EQ(executor.evidence, (std::vector<std::string>{"active-slave-streams", "outer-rs", "outer-ag"}));
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(output[i], 1001 + i);
    }
    const OxcRingHighFidelitySnapshot after = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::CONFIG, "ut_oxc_hf_ar");
    ExpectOxcRingHighFidelitySnapshotUnchanged(before, after);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}

TEST_F(HcclCommunicator91093OxcInitTest,
    Ut_OxcRingHighFidelityAllGather_When_MemConfigEntry_Expect_CommInitToLayeredOutputCorrect)
{
    MOCKER(HcclD2DMemcpyAsync).stubs().with(any(), any(), any(), any()).will(invoke(HcclD2DMemcpyAsyncStub));
    Ut_Device_Set(0);
    HcclCommConfig config;
    InitA3CommConfig(config, "ut_oxc_hf_ag");
    const std::string rankTableString = BuildOxcA3HighFidelityRankTable().dump();
    HcclComm localComm = nullptr;
    ASSERT_EQ(HcclCommInitClusterInfoMemConfig(rankTableString.c_str(), 0, &config, &localComm), HCCL_SUCCESS);
    const OxcRingHighFidelitySnapshot before = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::MEM_CONFIG, "ut_oxc_hf_ag");
    ExpectOxcRingHighFidelityEntryReady(before);
    ASSERT_TRUE(before.isOneSided);
    ASSERT_TRUE(before.commNameRegistered);
    ASSERT_EQ(ClassifyOxcRingHighFidelitySnapshot(before), OxcRingHighFidelityClassification::REAL_ENTRY_HIGH_FIDELITY);

    std::unique_ptr<TopoMatcher> topoMatcher = BuildOxcHighFidelityTopoMatcher(before);
    ASSERT_NE(topoMatcher, nullptr);
    OxcRingHighFidelityDispatcher dispatcher;
    ASSERT_EQ(dispatcher.Init(), HCCL_SUCCESS);
    OxcRingHighFidelityAllGatherExecutorProbe executor(reinterpret_cast<HcclDispatcher>(&dispatcher), topoMatcher);
    ASSERT_EQ(executor.SetAlgType(AlgType(AlgTypeLevel0::ALG_LEVEL0_NP_SINGLE_RING,
        AlgTypeLevel1::ALG_LEVEL1_RING, AlgTypeLevel2::ALG_LEVEL2_RING)), HCCL_SUCCESS);
    executor.workflowMode_ = HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    executor.tag_ = "oxc_hf_ag";
    AlgResourceResponse algRes = BuildOxcHighFidelityAlgResourceResponse(before);
    executor.algResResp_ = &algRes;

    int input[HF_AG_COUNT] = {21,22};
    int output[32] = {0};
    int scratch[HF_AG_COUNT] = {0};
    int userOutput[32] = {0};
    ExecMem execMem;
    execMem.count = HF_AG_COUNT;
    execMem.inputMem = DeviceMem::create(input, sizeof(input));
    execMem.outputMem = DeviceMem::create(output, sizeof(output));
    execMem.scratchMem = DeviceMem::create(scratch, sizeof(scratch));
    execMem.inputPtr = input;
    execMem.outputPtr = userOutput;

    OpParam param;
    param.tag = "oxc_hf_ag";
    param.opType = HcclCMDType::HCCL_CMD_ALLGATHER;
    param.DataDes.count = HF_AG_COUNT;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT32;
    param.DataDes.strideCount = 0U;
    param.supportZeroCopy = false;
    param.aicpuUnfoldMode = false;
    param.stream = BuildOxcHighFidelityTestStream();

    ASSERT_EQ(executor.KernelRun(param, execMem), HCCL_SUCCESS);
    EXPECT_EQ(executor.evidence, (std::vector<std::string>{"active-slave-streams", "outer-ag"}));
    const int expected[32] = {
        21,22,61,62,101,102,141,142,
        41,42,81,82,121,122,161,162,
        31,32,71,72,111,112,151,152,
        51,52,91,92,131,132,171,172
    };
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(output[i], expected[i]);
    }
    const OxcRingHighFidelitySnapshot after = CaptureOxcRingHighFidelitySnapshot(localComm, OxcRingHighFidelityEntryRoute::MEM_CONFIG, "ut_oxc_hf_ag");
    ExpectOxcRingHighFidelitySnapshotUnchanged(before, after);
    Ut_Comm_Destroy(reinterpret_cast<void *&>(localComm));
    ASSERT_EQ(ResetInitState(), HCCL_SUCCESS);
}
