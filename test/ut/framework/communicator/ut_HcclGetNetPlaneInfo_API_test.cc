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

class HcclGetNetPlaneInfoTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
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
