/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <cstdio>
#include <cstdlib>

#include <hccl/hccl_comm.h>
#include <hccl/hccl_inner.h>
#include "externalinput_pub.h"
#include "externalinput.h"
#include "adapter_rts.h"
#include "env_config.h"

#define private public
#define protected public
#include "comm_config_pub.h"
#include "hccl_communicator.h"
#undef protected
#undef private
using namespace std;
using namespace hccl;

class CommConfigTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--CommConfigTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--CommConfigTest TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(CommConfigTest, utCommConfig_load)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");

    EXPECT_EQ(commConfig.GetConfigBufferSize(), 200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 0);

    HcclCommConfig userConfig;
    HcclCommConfigInit(&userConfig);

    userConfig.hcclBufferSize = 300;
    userConfig.hcclDeterministic = 1;
    strcpy_s(userConfig.hcclCommName, COMM_NAME_MAX_LENGTH, "Comm1");

    HcclResult ret = commConfig.Load(&userConfig);

    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(commConfig.GetConfigBufferSize(), 300 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    EXPECT_EQ(commConfig.GetConfigCommName(), "Comm1");
    GlobalMockObject::verify();
}

#if 0

TEST_F(CommConfigTest, utCommConfig_magicword_verify)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 200, 0 };

    HcclResult ret = commConfig.CheckMagicWord(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.info = { sizeof(configHandle), 0, 1, { 0 } };
    ret = commConfig.CheckMagicWord(configHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}

#endif

TEST_F(CommConfigTest, utCommConfig_version_compatibility_v0)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 0, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1 };

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 0);
    EXPECT_EQ(commConfig.GetConfigCommName(), "comm_ID");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_version_compatibility_v1)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "should_not_be_loaded", 0, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 300 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    EXPECT_EQ(commConfig.GetConfigCommName(), "comm_ID");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_default_env_config)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, HCCL_COMM_BUFFSIZE_CONFIG_NOT_SET, HCCL_COMM_DETERMINISTIC_CONFIG_NOT_SET };

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 0);
    GlobalMockObject::verify();
}

ExternalInput g_externalInput;

TEST_F(CommConfigTest, utCommConfig_op_expansion)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministic)
    .stubs()
    .will(returnValue(false));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 4, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigAivMode(), true);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    EXPECT_EQ(configHandle.info.version, 4);
    EXPECT_EQ(configHandle.opExpansionMode, 3);

    configHandle.opExpansionMode = 1;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.opExpansionMode = 2;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.opExpansionMode = 3;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    configHandle.opExpansionMode = 4;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    g_externalInput.aicpuUnfold = false;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_op_expansion_v0)
{
    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 4, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4};
    g_externalInput.hcclDeterministic == true;
    configHandle.opExpansionMode = 3;
    configHandle.deterministic = 1;
    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigAivMode(), true);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 1);
    ret = commConfig.SetConfigOpExpansionMode(configHandle);
    configHandle.opExpansionMode = 0;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    configHandle.opExpansionMode = 1;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    configHandle.opExpansionMode = 2;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    configHandle.opExpansionMode = 999;
    ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, utCommConfig_deterministic_strcit)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministicV2).stubs().will(returnValue(0));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 2, "comm_ID", "should_not_be_loaded", 0, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(commConfig.GetConfigBufferSize(), 300 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE);
    EXPECT_EQ(commConfig.GetConfigDeterministic(), 2);
    EXPECT_EQ(commConfig.GetConfigCommName(), "comm_ID");
    GlobalMockObject::verify();
}

TEST_F(CommConfigTest, Ut_GetAicpuUnfoldConfig_When_SetConfigOpExpansionMode_Aicpu_A3_ReturnIsHCCL_SUCCESS)
{
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 4, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 1, "comm_ID", "Unspecified", 3, 132, 4};
    configHandle.opExpansionMode = 2;
    HcclResult ret = commConfig.SetConfigOpExpansionMode(configHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankTable_t rankTable;
    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = 2;
    rankTable.serverNum = 2;
    aclrtSetDevice(0);

    HcclRtStream opStream;
    rtStream_t stream;
    HcclCommunicator communicator(commConfig);
    bool flag = communicator.GetAicpuUnfoldConfig();
    EXPECT_EQ(flag, true);
    GlobalMockObject::verify();
}

#if 0

TEST_F(CommConfigTest, utCommConfig_deterministic_strcit_fail)
{
    MOCKER(GetExternalInputCCLBuffSize)
    .stubs()
    .will(returnValue(static_cast<u64>(200 * HCCL_CCL_COMM_FIXED_CALC_BUFFER_SIZE)));

    MOCKER(GetExternalInputHcclDeterministicV2).stubs().will(returnValue(0));

    // 确定性计算配置为规约保序仅支持A2场景
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 300, 2, "comm_ID", "should_not_be_loaded", 0, 132, 4};

    HcclResult ret = commConfig.SetConfigByVersion(configHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}

#endif
