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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "orion_adapter_hccp.h"

#define private public
#define protected public

#include "aiv_ub_mem_transport.h"

#undef protected
#undef private

class AivUbMemTransportTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AivUbMemTransportTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AivUbMemTransportTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AivUbMemTransportTest SetUP" << std::endl;
        Hccl::IpAddress   localIp("1.0.0.0");
        Hccl::IpAddress   remoteIp("2.0.0.0");
        fakeSocket = new Hccl::Socket(nullptr, localIp, 100, remoteIp, "test", Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in AivUbMemTransportTest TearDown" << std::endl;
    }

    Hccl::Socket *fakeSocket;
    Hccl::RdmaHandle rdmaHandle = (void *)0x1000000;
};

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_GetUserRemoteMem_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    auto rmtBuffer0 = std::make_unique<Hccl::RemoteIpcRmaBuffer>();
    aivTransport->rmtBufferVec_.push_back(std::move(rmtBuffer0));
    auto rmtBuffer1 = std::make_unique<Hccl::RemoteIpcRmaBuffer>();
    rmtBuffer1->addr = (uintptr_t)0x101;
    rmtBuffer1->size = (u64)0x101;
    rmtBuffer1->memType = HcclMemType::HCCL_MEM_TYPE_HOST;
    aivTransport->rmtBufferVec_.push_back(std::move(rmtBuffer1));

    std::array<char, HCCL_RES_TAG_MAX_LEN> memTag0{};
    std::string tag0 = "cclBuffer";
    memcpy_s(memTag0.data(), memTag0.size(), tag0.c_str(), tag0.size());
    aivTransport->remoteUserMemTag_.push_back(memTag0);
    std::array<char, HCCL_RES_TAG_MAX_LEN> memTag1{};
    std::string tag1 = "buffer1";
    memcpy_s(memTag1.data(), memTag1.size(), tag1.c_str(), tag1.size());
    aivTransport->remoteUserMemTag_.push_back(memTag1);

    CommMem *remoteMems;
    char **memTags;
    u32 memNum;
    HcclResult ret = aivTransport->GetUserRemoteMem(&remoteMems, &memTags, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string memTag = memTags[0];
    EXPECT_EQ(memTag, "buffer1");
    EXPECT_EQ(remoteMems[0].type, HcclMemType::HCCL_MEM_TYPE_HOST);
    EXPECT_EQ(remoteMems[0].addr, (void *)0x101);
    EXPECT_EQ(remoteMems[0].size, (uint64_t)0x101);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_GetUserRemoteMem_When_bufferNumIs0_Expect_ReturnIsHCCL_E_PARA)
{
    HcommChannelDesc desc{};
    desc.memHandleNum = 0;
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    HcclResult ret = aivTransport->Init();
    EXPECT_EQ(ret, HCCL_E_PARA);
}