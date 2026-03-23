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
#include "llt_stub_CcuTransport.h"

#include <iostream>

#define private public
#define protected public

#include "ccu_urma_channel.h"
#include "ccu_transport_.h"
#include "local_ub_rma_buffer.h"

#undef protected
#undef private

using namespace Hccl;

class CcuTransportTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        // 将enableEntryLog默认返回为true
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
        IpAddress localIp("1.0.0.0");
        IpAddress remoteIp("2.0.0.0");
        fakeSocket = new Hccl::Socket(nullptr, localIp, 100, remoteIp, "test", Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
        delete fakeSocket;
    }
    Hccl::Socket *fakeSocket;
protected:
};

TEST_F(CcuTransportTest, Ut_CcuTransport)
{
    std::unique_ptr<hcomm::CcuTransport> impl{};
    hcomm::CcuTransport::CcuConnectionInfo connInfo{};
    hcomm::CcuTransport::CclBufferInfo buffInfo{};
    hcomm::CcuCreateTransport(nullptr, connInfo, buffInfo, impl);
    // std::unique_ptr<Hccl::CcuTransport> impl{};
    // Hccl::CcuTransport::CcuConnectionInfo connInfo{};
    // Hccl::CcuTransport::CclBufferInfo buffInfo{};
    // Hccl::CcuCreateTransport(nullptr, connInfo, buffInfo, impl);

    std::cout << "Hello World" << std::endl;
}

TEST_F(CcuTransportTest, ut_CcuTransport_GetUserRemoteMem_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    auto buffer0 = std::make_shared<Buffer>(0x100, 0x100);
    RdmaHandle rdmaHandle = (void*)0x100;
    auto locBuffer0 = make_shared<Hccl::LocalUbRmaBuffer>(buffer0, rdmaHandle);
    std::vector<Hccl::LocalUbRmaBuffer*> mems{};
    mems.push_back(locBuffer0.get());

    auto buffer1 = std::make_shared<Buffer>(0x101, 0x101);
    strcpy(buffer1->mem_Tag_, "buffer1");
    buffer1->memType_ = HcclMemType::HCCL_MEM_TYPE_HOST;
    auto locBuffer1 = make_shared<Hccl::LocalUbRmaBuffer>(buffer1, rdmaHandle);
    mems.push_back(locBuffer1.get());

    void **memHandles = reinterpret_cast<void**>(mems.data());
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};

    HcclResult ret = hcomm::BuildBufferInfos(memHandles, 2, bufferInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    hcomm::CcuTransport::CcuConnectionInfo connInfo{};
    MOCKER_CPP(&hcomm::CcuConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuTransport::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ret = hcomm::CcuCreateTransport(fakeSocket, connInfo, bufferInfos, ccuTransport);

    BinaryStream binaryStream;
    ret = ccuTransport->BufferInfoPack(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = ccuTransport->BufferInfoUnpack(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    CommMem *remoteMems;
    char **memTags;
    u32 memNum;
    ret = ccuTransport->GetUserRemoteMem(&remoteMems, &memTags, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string memTag = memTags[0];
    EXPECT_EQ(memTag, "buffer1");
    EXPECT_EQ(remoteMems[0].type, HcclMemType::HCCL_MEM_TYPE_HOST);
    EXPECT_EQ(remoteMems[0].addr, (void *)0x101);
    EXPECT_EQ(remoteMems[0].size, (uint64_t)0x101);
}

TEST_F(CcuTransportTest, ut_CcuTransport_GetUserRemoteMem_When_bufferNumIs0_Expect_ReturnIsHCCL_E_PARA)
{
    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    hcomm::CcuTransport::CcuConnectionInfo connInfo{};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    MOCKER_CPP(&hcomm::CcuConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuTransport::Init).stubs().will(returnValue(HCCL_SUCCESS));
    HcclResult ret = hcomm::CcuCreateTransport(fakeSocket, connInfo, bufferInfos, ccuTransport);
    EXPECT_EQ(ret, HCCL_E_PARA);
}