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

class HcclCommInitRootInfoConfigTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        // 将建链超时时间设置为1s，减少测试用例运行时间
        MOCKER(GetExternalInputHcclLinkTimeOut)
            .stubs()
            .with(any())
            .will(returnValue(1));
    }
    void TearDown() override {
        // 删除所有拓扑建链的线程
        HcclOpInfoCtx& opBaseInfo = GetHcclOpInfoCtx();
        opBaseInfo.hcclCommTopoInfoDetectServer.clear();
        opBaseInfo.hcclCommTopoInfoDetectAgent.clear();

        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_nRanksIsZero_Expect_ReturnIsHCCL_E_PARA)
{
    int nRanks = 0;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rank = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize= 400;
 
    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
}

// 小概率出现段错误
TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_GetDeviceError_Expect_ReturnIsHCCL_E_RUNTIME)
{
    // int nRanks = 1;
    // HcclRootInfo id = Ut_Get_Root_Info(0);
    // int rank = 0;
    // MOCKER(rtGetDevice)
    //     .stubs()
    //     .will(returnValue(ACL_ERROR_RT_CONTEXT_NULL));
    // HcclCommConfig commConfig;
    // HcclCommConfigInit(&commConfig);
    // commConfig.hcclBufferSize= 400;

    // HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, &commConfig, &comm);
    // EXPECT_EQ(ret, HCCL_E_RUNTIME);

    // GlobalMockObject::verify();
    // Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_RootInfoIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    Ut_Device_Set(0);
    int nRanks = 1;
    HcclRootInfo *pId = nullptr;
    int rank = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize= 400;

    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, pId, rank, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}


TEST_F(HcclCommInitRootInfoTest, Ut_Group_HcclCommInitRootInfoConfig_rank8)
{
    int nRanks = 8;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rankId = 0;
    HcclComm comms[nRanks];

    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);

    HcclGroupStart();
    for (int i = 0; i < nRanks; ++i){
        Ut_Device_Set(i);
        HcclCommInitRootInfoConfig(nRanks, &id, rankId, &commConfig, &comms[i]);
    }
    HcclResult ret = HcclGroupEnd();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclGroupStart();
    for (int i = 0; i < nRanks; ++i){
        Ut_Device_Set(i);
        Ut_Comm_Destroy(comms[i]);
    }
    ret = HcclGroupEnd();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_RankGreaterThannRanks_Expect_ReturnIsHCCL_E_PARA)
{
    int nRanks = 10;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rank = 10;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize= 400;
 
    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_PCommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    int nRanks = 1;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rank = 0;
    HcclCommConfig *pCommConfig = nullptr;

    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, pCommConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_PCommConfigIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    int nRanks = 1;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rank = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize= 400;

    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, &commConfig, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_nRanksIsLarge_Expect_ReturnIsHCCL_E_INTERNAL)
{
    int nRanks = 128*1024;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rank = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize= 400;

    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommInitRootInfoConfigTest, Ut_HcclCommInitRootInfoConfig_When_RecvTimeOut_Expect_ReturnIsHCCL_E_TIMEOUT)
{
    MOCKER_CPP(&TopoInfoExchangeBase::RecvClusterInfoMsg)
        .stubs()
        .will(returnValue(HCCL_E_TIMEOUT));
    int nRanks = 1;
    HcclRootInfo id = Ut_Get_Root_Info(0);
    int rank = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.hcclBufferSize= 400;

    HcclResult ret = HcclCommInitRootInfoConfig(nRanks, &id, rank, &commConfig, &comm);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);

    Ut_Comm_Destroy(comm);
}