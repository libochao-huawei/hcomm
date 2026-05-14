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
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

#ifndef private
#define private public
#define protected public
#endif
#include "hccl_comm_pub.h"
#undef private
#undef protected

#include "llt_hccl_stub_sal_pub.h"
#include "calc_crc.h"
#include "hccl_res_expt.h"

using namespace std;
using namespace hccl;

class ExchangeInfoTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "ExchangeInfoTest SetUp" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "ExchangeInfoTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        hcclCommPtr = std::make_shared<hccl::hcclComm>();
        std::cout << "A Test SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        hcclCommPtr.reset();
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }

    std::shared_ptr<hccl::hcclComm> hcclCommPtr;
};

TEST_F(ExchangeInfoTest, Ut_CApiAddExchangeInfo_When_ParamValid_Expect_Success)
{
    std::vector<u8> data = {0x01, 0x02, 0x03};
    HcclComm comm = static_cast<HcclComm>(hcclCommPtr.get());
    HcclResult ret = HcclCommAddExchangeInfo(comm, data.data(), data.size());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ExchangeInfoTest, Ut_CApiGetExchangeInfo_When_ParamValid_Expect_Success)
{
    std::vector<u8> remoteData = {0xAA, 0xBB};
    size_t size = remoteData.size();
    hccl::CollComm* collComm = hcclCommPtr->GetCollComm();
    CHK_PTR_NULL(collComm);
    hccl::MyRank* myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);
    CollCommConfigConsistency &collCommConfigConsistency = myRank->GetCollCommConfigConsistency();
    collCommConfigConsistency.StoreRemoteExchangeInfo(0, remoteData);

    HcclComm comm = static_cast<HcclComm>(hcclCommPtr.get());
    std::vector<u8> recvBuf(size, 0);
    uint32_t recvBufSize = recvBuf.size();
    uint32_t actualLen = 0;
    HcclResult ret = HcclCommGetExchangeInfo(comm, 0, recvBufSize, recvBuf.data(), &actualLen);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ExchangeInfoTest, Ut_CApiResetExchangeInfo_When_ParamValid_Expect_Success)
{
    HcclComm comm = static_cast<HcclComm>(hcclCommPtr.get());
    HcclResult ret = HcclCommResetExchangeInfo(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 端到端流程测试：AddExchangeInfo → StoreRemote → GetExchangeInfo
TEST_F(ExchangeInfoTest, Ut_EndToEnd_When_AddStoreGet_Expect_Consistent)
{
    // 1. 本端添加交换信息
    CollCommConfigConsistency consistency;
    std::vector<u8> localData = {0xDE, 0xAD, 0xBE, 0xEF};
    hccl::CollComm* collComm = hcclCommPtr->GetCollComm();
    CHK_PTR_NULL(collComm);
    hccl::MyRank* myRank = collComm->GetMyRank();
    CHK_PTR_NULL(myRank);
    CollCommConfigConsistency &collCommConfigConsistency = myRank->GetCollCommConfigConsistency();
    HcclResult ret = collCommConfigConsistency.AddExchangeInfo(localData.data(), localData.size());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 2. 模拟建链后存储对端信息
    std::vector<u8> remoteData = {0xCA, 0xFE, 0xBA, 0xBE};
    size_t size = remoteData.size();
    ret = collCommConfigConsistency.StoreRemoteExchangeInfo(1, remoteData);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 3. 清空本端交换信息状态（模拟HcclChannelAcquire建链后清空）
    ret = collCommConfigConsistency.ResetExchangeInfo();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 4. 获取对端交换信息
    std::vector<u8> recvBuf(size, 0);
    uint32_t recvBufSize = recvBuf.size();
    uint32_t actualLen = 0;
    ret = collCommConfigConsistency.GetExchangeInfo(1, recvBufSize, recvBuf.data(), &actualLen);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

