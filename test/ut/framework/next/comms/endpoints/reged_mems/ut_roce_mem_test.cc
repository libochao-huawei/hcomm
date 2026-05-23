/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "roce_mem.h"
#include "endpoint.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"

using namespace hcomm;

class RoceRegedMemMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RoceRegedMemMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "RoceRegedMemMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in RoceRegedMemMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in RoceRegedMemMgrTest TearDown" << std::endl;
    }
};

TEST_F(RoceRegedMemMgrTest, Ut_GetParamsFromMemDesc_When_DescLenTooSmall_Expect_Return_Error)
{
    std::shared_ptr<RoceRegedMemMgr> roceRegedMemMgrPtr = std::make_shared<RoceRegedMemMgr>();
    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    
    char buffer[10];
    uint32_t descLen = 10;
    
    HcclResult ret = roceRegedMemMgrPtr->GetParamsFromMemDesc(buffer, descLen, endpointDesc, dto);
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
}

// 父+子集注册 → 先解注册子(alias) → 再解注册父
TEST_F(RoceRegedMemMgrTest, ut_RoceRegedMemMgr_When_ParentChild_Expect_UnregisterChildFirst)
{
    RoceRegedMemMgr roceRegedMemMgr{};
    HcommMem mem0;
    mem0.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem0.addr = (void*)0x1000;
    mem0.size = 4096;
    std::string memTag0 = "parent";
    void *memHandle0 = nullptr;
    HcclResult ret = roceRegedMemMgr.RegisterMemory(mem0, memTag0.c_str(), &memHandle0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 子集注册（应创建alias buffer）
    HcommMem mem1;
    mem1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem1.addr = (void*)0x1000;
    mem1.size = 1024;
    std::string memTag1 = "child";
    void *memHandle1 = nullptr;
    ret = roceRegedMemMgr.RegisterMemory(mem1, memTag1.c_str(), &memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 先解注册子buffer（IsAlias查找父key → Del ref 2→1）
    ret = roceRegedMemMgr.UnregisterMemory(memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 再解注册父buffer（自己的key在tree中 → Del ref 1→0）
    ret = roceRegedMemMgr.UnregisterMemory(memHandle0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 多次注册同一范围，各自解注册
TEST_F(RoceRegedMemMgrTest, ut_RoceRegedMemMgr_When_MultipleSameRange_Expect_AllSuccess)
{
    RoceRegedMemMgr roceRegedMemMgr{};
    HcommMem mem;
    mem.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem.addr = (void*)0x5000;
    mem.size = 2048;

    void *h1 = nullptr, *h2 = nullptr, *h3 = nullptr;
    EXPECT_EQ(roceRegedMemMgr.RegisterMemory(mem, "t1", &h1), HCCL_SUCCESS);
    EXPECT_EQ(roceRegedMemMgr.RegisterMemory(mem, "t2", &h2), HCCL_SUCCESS);
    EXPECT_EQ(roceRegedMemMgr.RegisterMemory(mem, "t3", &h3), HCCL_SUCCESS);

    EXPECT_EQ(roceRegedMemMgr.UnregisterMemory(h3), HCCL_SUCCESS);
    EXPECT_EQ(roceRegedMemMgr.UnregisterMemory(h2), HCCL_SUCCESS);
    EXPECT_EQ(roceRegedMemMgr.UnregisterMemory(h1), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrTest, Ut_GetParamsFromMemDesc_When_DescLenEqualSize_Expect_Return_Success)
{
    std::shared_ptr<RoceRegedMemMgr> roceRegedMemMgrPtr = std::make_shared<RoceRegedMemMgr>();
    EndpointDesc endpointDesc;
    Hccl::ExchangeRdmaBufferDto dto;
    
    char buffer[sizeof(EndpointDesc)];
    uint32_t descLen = sizeof(EndpointDesc);
    
    MOCKER_CPP_VIRTUAL(dto,&Hccl::ExchangeRdmaBufferDto::Deserialize)
        .stubs();
    
    HcclResult ret = roceRegedMemMgrPtr->GetParamsFromMemDesc(buffer, descLen, endpointDesc, dto);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}
