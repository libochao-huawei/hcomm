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
#include "hccl_api.h"
 
// using namespace std;
// using namespace hccl;
 
constexpr const char* RANKTABLE_FILE_NAME = "./ut_independent_op_test.json";
 
class HcclCommChannelTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        UT_COMM_CREATE_DEFAULT(comm);
    }
    void TearDown() override {
        Ut_Comm_Destroy(comm);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};
 
TEST_F(HcclCommChannelTest, Ut_HcclChannelCreate_When_Comm_Nullptr_Return_EPTR)
{
    RoCEAttr roceAttr;
    ChannelDesc desc[2] = {
        {1, CommProtocol::COMM_PROTOCOL_ROCE, 2, {.roceAttr = roceAttr}}, 
        {2, CommProtocol::COMM_PROTOCOL_ROCE, 2, {.roceAttr = roceAttr}}
    };

    ChannelHandle channel;
    HcclResult ret = HcclChannelCreate(nullptr, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, desc, 2, &channel);
    EXPECT_EQ(ret, HCCL_E_PTR);
}