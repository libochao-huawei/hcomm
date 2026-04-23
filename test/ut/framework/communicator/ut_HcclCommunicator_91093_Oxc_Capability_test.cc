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
using namespace hccl;

class HcclCommunicator91093OxcCapabilityTest : public testing::Test {
public:
    void SetUp() override
    {
        DevType deviceType = DevType::DEV_TYPE_910_93;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommunicator91093OxcCapabilityTest,
    Ut_CommConfig_When_OpExpansionModeIsAicpuOn91093_Expect_AicpuUnfoldSetTrue)
{
    HcclCommConfig config;
    HcclCommConfigInit(&config);
    config.hcclOpExpansionMode = COMM_CONFIG_OPEXPANSION_AICPU;

    CommConfig commConfig("ut_a3_aicpu_mode");
    ASSERT_EQ(commConfig.Load(&config), HCCL_SUCCESS);
    EXPECT_TRUE(commConfig.GetConfigAicpuUnfold());
    EXPECT_FALSE(commConfig.GetConfigAivMode());
}

TEST_F(HcclCommunicator91093OxcCapabilityTest,
    Ut_HcclCommunicator_When_BackupLinkChecksAicpuFromCommConfig_Expect_EnableBackupLink)
{
    HcclCommConfig config;
    HcclCommConfigInit(&config);
    config.hcclOpExpansionMode = COMM_CONFIG_OPEXPANSION_AICPU;
    ASSERT_EQ(strcpy_s(config.hcclRetryEnable, sizeof(config.hcclRetryEnable), "0,0,1"), EOK);

    CommConfig commConfig("ut_a3_backup_link_gate");
    ASSERT_EQ(commConfig.Load(&config), HCCL_SUCCESS);
    commConfig.retryEnable_[HCCL_RETRY_ENABLE_LEVEL_2] = true;
    HcclCommunicator communicator(commConfig);

    communicator.deviceType_ = DevType::DEV_TYPE_910_93;
    communicator.interServer_ = true;
    communicator.retryEnable_ = true;
    communicator.rtsSupportChangeLink_ = true;
    communicator.isDiffDeviceType_ = false;
    communicator.devBackupIpAddr_.clear();
    communicator.devBackupIpAddr_.push_back(HcclIpAddress("10.20.0.10"));
    communicator.superPodNum_ = 2;
    communicator.multiSuperPodDiffServerNumMode_ = true;
    communicator.attrCollector_.deviceType_ = DevType::DEV_TYPE_910_93;
    communicator.attrCollector_.interServer_ = true;
    communicator.attrCollector_.superPodNum_ = 2;
    communicator.attrCollector_.multiSuperPodDiffServerNumMode_ = true;

    EXPECT_TRUE(communicator.GetAicpuUnfoldConfig());
    EXPECT_TRUE(communicator.commConfig_.GetConfigInterSuperPodRetryEnable());
    EXPECT_TRUE(communicator.IsEnableRoce());
    EXPECT_TRUE(communicator.IsEnableBackupLink());
}
} // namespace
