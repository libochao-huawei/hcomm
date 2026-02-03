/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "../communicator/hccl_api_base_test.h"
#include "hccl_comm_pub.h"
class TestCollComm : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestCollComm, Ut_TestInitCollComm_When_Expect_Success)
{
    void* commV2 = (void*)0x2000;
    void* rankGraph = (void*)0x2000;
    u32 rank = 1;
    HcclMem cclBuffer;
    cclBuffer.size = 1;
    cclBuffer.type = HcclMemType::HCCL_MEM_TYPE_HOST;
    cclBuffer.addr = (void*)0x1000;;
    char commName[ROOTINFO_INDENTIFIER_MAX_LENGTH] = {};
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = make_shared<hccl::hcclComm>(1, 1, commName);
    HcclCommConfig config;
    config.hcclOpExpansionMode = 1; // 非CCU模式，避免拉起CCU平台层
    HcclResult ret = hcclCommPtr->InitCollComm(commV2, rankGraph, rank, cclBuffer, commName, &config);
    EXPECT_EQ(ret, 0);
    CollComm* collComm = hcclCommPtr->GetCollComm();
    EXPECT_NE(collComm, nullptr);
}

