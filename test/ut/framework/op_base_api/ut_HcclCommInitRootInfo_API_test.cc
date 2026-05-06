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

HcclResult TryOverrideRootInfoRankTableFromFile(uint32_t rank, hccl::HcclCommParams &params,
    hccl::RankTable_t &rankTable, const hccl::HcclBasicRankInfo &localRankInfo, std::string &rankTableM,
    bool &overrideApplied);

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

class ScopedEnvVar {
public:
    ScopedEnvVar(const char *name, const std::string &value) : name_(name)
    {
        const char *previous = getenv(name);
        if (previous != nullptr) {
            hadPrevious_ = true;
            previousValue_ = previous;
        }
        setenv(name, value.c_str(), 1);
    }

    explicit ScopedEnvVar(const char *name) : name_(name)
    {
        const char *previous = getenv(name);
        if (previous != nullptr) {
            hadPrevious_ = true;
            previousValue_ = previous;
        }
        unsetenv(name);
    }

    ~ScopedEnvVar()
    {
        if (hadPrevious_) {
            setenv(name_.c_str(), previousValue_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    bool hadPrevious_ { false };
    std::string previousValue_;
};

HcclResult HrtGetDeviceRootInfoStub(s32 *deviceLogicId)
{
    CHK_PTR_NULL(deviceLogicId);
    *deviceLogicId = 0;
    return HCCL_SUCCESS;
}

std::string CreateTempRankTableFile(const nlohmann::json &rankTable)
{
    char pathTemplate[] = "/tmp/rootinfo_ranktable_XXXXXX.json";
    int fd = mkstemps(pathTemplate, 5);
    EXPECT_GE(fd, 0);
    close(fd);
    std::ofstream out(pathTemplate);
    out << rankTable.dump();
    out.close();
    return std::string(pathTemplate);
}

nlohmann::json BuildSuperPodRankTableV12()
{
    return nlohmann::json::object({
        {"status", "completed"},
        {"version", SUPERPOD_CLUSTER_VERSION},
        {"server_count", "1"},
        {"server_list", nlohmann::json::array({
            nlohmann::json::object({
                {"server_id", "server0"},
                {"host_ip", "192.168.10.10"},
                {"device", nlohmann::json::array({
                    nlohmann::json::object({
                        {"rank_id", "0"},
                        {"device_id", "0"},
                        {"device_ip", "10.10.0.10"},
                        {"super_device_id", "100"}
                    })
                })}
            })
        })},
        {"super_pod_list", nlohmann::json::array({
            nlohmann::json::object({
                {"super_pod_id", "pod0"},
                {"server_list", nlohmann::json::array({
                    nlohmann::json::object({{"server_id", "server0"}})
                })}
            })
        })}
    });
}

nlohmann::json BuildOxcRankTableV14()
{
    return nlohmann::json::object({
        {"status", "completed"},
        {"version", OXC_CLUSTER_VERSION},
        {"task_id", "rootinfo_oxc_override_ut"},
        {"server_list", nlohmann::json::array({
            nlohmann::json::object({
                {"server_id", "server0"},
                {"host_ip", "192.168.10.10"},
                {"device", nlohmann::json::array({
                    nlohmann::json::object({
                        {"device_id", "0"},
                        {"super_device_id", "100"},
                        {"rankid", "0"},
                        {"device_ip", "10.10.0.10"},
                        {"backup_device_ip", "10.20.0.10"},
                        {"device_port", 16666},
                        {"backup_device_port", 17666}
                    })
                })}
            })
        })},
        {"super_pod_list", nlohmann::json::array({
            nlohmann::json::object({
                {"super_pod_id", "pod0"},
                {"server_list", nlohmann::json::array({
                    nlohmann::json::object({{"server_id", "server0"}})
                })}
            })
        })},
        {"oxc_group_list", nlohmann::json::array({
            nlohmann::json::object({
                {"group_id", "group0"},
                {"rank_list", nlohmann::json::array({
                    nlohmann::json::object({{"rank_id", "0"}})
                })}
            })
        })}
    });
}

HcclCommParams BuildDetectParams()
{
    HcclCommParams params;
    params.rank = 0;
    params.userRank = 0;
    params.logicDevId = 0;
    params.totalRanks = 1;
    params.deviceType = DevType::DEV_TYPE_910_93;
    params.serverId = "192.168.10.10";
    return params;
}

HcclBasicRankInfo BuildLocalRankInfo(u32 devicePhyId = 0, const std::string &superPodId = "pod0", u32 superDeviceId = 100)
{
    HcclBasicRankInfo info;
    info.rank = 0;
    info.rankSize = 1;
    info.deviceType = DevType::DEV_TYPE_910_93;
    info.deviceLogicID = 0;
    info.devicePhysicID = devicePhyId;
    info.hostIP.SetReadableAddress("192.168.10.10");
    info.superPodId = superPodId;
    info.superDeviceId = superDeviceId;
    return info;
}

RankTable_t BuildDetectRankTable(const std::string &serverId = "server0")
{
    RankTable_t rankTable;
    rankTable.version = "detect";
    rankTable.serverNum = 1;
    rankTable.deviceNum = 1;
    rankTable.rankNum = 1;
    RankInfo_t rankInfo;
    rankInfo.rankId = 0;
    rankInfo.serverId = serverId;
    rankInfo.deviceInfo.devicePhyId = 0;
    rankInfo.hostIp.SetReadableAddress("192.168.10.10");
    rankTable.rankList.push_back(rankInfo);
    return rankTable;
}

std::string CaptureRootInfoOverrideStdout(u32 rank, HcclCommParams &params, RankTable_t &rankTable,
    const HcclBasicRankInfo &localRankInfo, std::string &rankTableM, bool &overrideApplied, HcclResult &ret)
{
    testing::internal::CaptureStdout();
    ret = TryOverrideRootInfoRankTableFromFile(rank, params, rankTable, localRankInfo, rankTableM, overrideApplied);
    return testing::internal::GetCapturedStdout();
}

class HcclCommInitRootInfoOverrideLogTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        SetIfProfile(false);
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDevice).defaults().will(invoke(HrtGetDeviceRootInfoStub));
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

TEST_F(HcclCommInitRootInfoOverrideLogTest, Ut_TryOverrideRootInfoRankTableFromFile_When_RankTableFileIsUnset_Expect_KeepDetectMainline)
{
    ScopedEnvVar scopedEnv("RANK_TABLE_FILE");
    ScopedStubLogLevel scopedLogLevel(DLOG_INFO);
    HcclCommParams params = BuildDetectParams();
    RankTable_t rankTable = BuildDetectRankTable();
    HcclBasicRankInfo localRankInfo = BuildLocalRankInfo();
    std::string rankTableM;
    bool overrideApplied = true;
    HcclResult ret = HCCL_SUCCESS;

    std::string output = CaptureRootInfoOverrideStdout(0, params, rankTable, localRankInfo,
        rankTableM, overrideApplied, ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(overrideApplied);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] RANK_TABLE_FILE is not set, keep topo-detect rankTable."),
        std::string::npos);
    EXPECT_EQ(rankTable.version, std::string("detect"));
}

TEST_F(HcclCommInitRootInfoOverrideLogTest, Ut_TryOverrideRootInfoRankTableFromFile_When_SuperPodRankTableLoaded_Expect_VersionAndSummaryLogged)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_INFO);
    std::string rankTablePath = CreateTempRankTableFile(BuildSuperPodRankTableV12());
    ScopedEnvVar scopedEnv("RANK_TABLE_FILE", rankTablePath);
    bool useSuperPodMode = true;
    MOCKER(IsSuperPodMode).stubs().with(outBound(useSuperPodMode)).will(returnValue(HCCL_SUCCESS));

    HcclCommParams params = BuildDetectParams();
    RankTable_t rankTable = BuildDetectRankTable();
    HcclBasicRankInfo localRankInfo = BuildLocalRankInfo();
    std::string rankTableM;
    bool overrideApplied = false;
    HcclResult ret = HCCL_SUCCESS;

    std::string output = CaptureRootInfoOverrideStdout(0, params, rankTable, localRankInfo,
        rankTableM, overrideApplied, ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(overrideApplied);
    EXPECT_EQ(rankTable.version, std::string(SUPERPOD_CLUSTER_VERSION));
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] load ranktable file success"), std::string::npos);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] parser dispatch version[1.2] parser[concise]."),
        std::string::npos);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride][Summary] version[1.2] rank_num[1] server_num[1] device_num[1]."),
        std::string::npos);
    EXPECT_NE(output.find("super_pod_id[pod0]"), std::string::npos);
    unlink(rankTablePath.c_str());
}

TEST_F(HcclCommInitRootInfoOverrideLogTest, Ut_TryOverrideRootInfoRankTableFromFile_When_OxcRankTableLoaded_Expect_VersionAndDetailLogged)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_DEBUG);
    std::string rankTablePath = CreateTempRankTableFile(BuildOxcRankTableV14());
    ScopedEnvVar scopedEnv("RANK_TABLE_FILE", rankTablePath);

    HcclCommParams params = BuildDetectParams();
    RankTable_t rankTable = BuildDetectRankTable();
    HcclBasicRankInfo localRankInfo = BuildLocalRankInfo();
    std::string rankTableM;
    bool overrideApplied = false;
    HcclResult ret = HCCL_SUCCESS;

    std::string output = CaptureRootInfoOverrideStdout(0, params, rankTable, localRankInfo,
        rankTableM, overrideApplied, ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(overrideApplied);
    EXPECT_EQ(rankTable.version, std::string(OXC_CLUSTER_VERSION));
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] parser dispatch version[1.4] parser[oxc]."),
        std::string::npos);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride][Summary] version[1.4] rank_num[1] server_num[1] device_num[1]."),
        std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Summary]"), std::string::npos);
    EXPECT_NE(output.find("[OXC_HCOMM][Parse][Detail][Ports] rank_id[0] device_port[16666] backup_device_port[17666]."),
        std::string::npos);
    unlink(rankTablePath.c_str());
}

TEST_F(HcclCommInitRootInfoOverrideLogTest, Ut_TryOverrideRootInfoRankTableFromFile_When_FileMissing_Expect_LoadFailureLogged)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_INFO);
    ScopedEnvVar scopedEnv("RANK_TABLE_FILE", "/tmp/does_not_exist_ranktable.json");
    HcclCommParams params = BuildDetectParams();
    RankTable_t rankTable = BuildDetectRankTable();
    HcclBasicRankInfo localRankInfo = BuildLocalRankInfo();
    std::string rankTableM;
    bool overrideApplied = false;
    HcclResult ret = HCCL_SUCCESS;

    std::string output = CaptureRootInfoOverrideStdout(0, params, rankTable, localRankInfo,
        rankTableM, overrideApplied, ret);
    EXPECT_NE(ret, HCCL_SUCCESS);
    EXPECT_FALSE(overrideApplied);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] detected RANK_TABLE_FILE[/tmp/does_not_exist_ranktable.json]."),
        std::string::npos);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] load ranktable file failed"), std::string::npos);
}

TEST_F(HcclCommInitRootInfoOverrideLogTest, Ut_TryOverrideRootInfoRankTableFromFile_When_DeviceMismatch_Expect_RejectOverride)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_INFO);
    std::string rankTablePath = CreateTempRankTableFile(BuildSuperPodRankTableV12());
    ScopedEnvVar scopedEnv("RANK_TABLE_FILE", rankTablePath);
    bool useSuperPodMode = true;
    MOCKER(IsSuperPodMode).stubs().with(outBound(useSuperPodMode)).will(returnValue(HCCL_SUCCESS));

    HcclCommParams params = BuildDetectParams();
    RankTable_t rankTable = BuildDetectRankTable();
    HcclBasicRankInfo localRankInfo = BuildLocalRankInfo(7);
    std::string rankTableM;
    bool overrideApplied = false;
    HcclResult ret = HCCL_SUCCESS;

    std::string output = CaptureRootInfoOverrideStdout(0, params, rankTable, localRankInfo,
        rankTableM, overrideApplied, ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(overrideApplied);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] reject override due to devicePhyId mismatch, keep topo-detect rankTable."),
        std::string::npos);
    EXPECT_EQ(rankTable.version, std::string("detect"));
    unlink(rankTablePath.c_str());
}

TEST_F(HcclCommInitRootInfoOverrideLogTest, Ut_TryOverrideRootInfoRankTableFromFile_When_ServerIdMismatch_Expect_RejectOverride)
{
    ScopedStubLogLevel scopedLogLevel(DLOG_INFO);
    std::string rankTablePath = CreateTempRankTableFile(BuildSuperPodRankTableV12());
    ScopedEnvVar scopedEnv("RANK_TABLE_FILE", rankTablePath);
    bool useSuperPodMode = true;
    MOCKER(IsSuperPodMode).stubs().with(outBound(useSuperPodMode)).will(returnValue(HCCL_SUCCESS));

    HcclCommParams params = BuildDetectParams();
    RankTable_t rankTable = BuildDetectRankTable("detect_server");
    HcclBasicRankInfo localRankInfo = BuildLocalRankInfo();
    std::string rankTableM;
    bool overrideApplied = false;
    HcclResult ret = HCCL_SUCCESS;

    std::string output = CaptureRootInfoOverrideStdout(0, params, rankTable, localRankInfo,
        rankTableM, overrideApplied, ret);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(overrideApplied);
    EXPECT_NE(output.find("[RootInfo][RankTableOverride] reject override due to serverId mismatch, keep topo-detect rankTable."),
        std::string::npos);
    EXPECT_EQ(rankTable.version, std::string("detect"));
    unlink(rankTablePath.c_str());
}
