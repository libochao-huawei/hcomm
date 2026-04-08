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
#include "env_config/env_config.h"
#include "base_config.h"

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

TEST_F(CcuTransportTest, ut_CcuTransport_GetUserRemoteMem_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    RdmaHandle rdmaHandle = (void*)0x100;
    auto buffer0 = std::make_shared<Buffer>(0x100, 0x100);
    auto locBuffer0 = std::make_shared<Hccl::LocalUbRmaBuffer>(buffer0, rdmaHandle);
    CommMemInfo memInfo0{};
    memInfo0.mem.addr = (void*)0x100;
    memInfo0.mem.size = (uint64_t)0x100;
    memInfo0.bufferHandle = static_cast<void*>(locBuffer0.get());

    auto buffer1 = std::make_shared<Buffer>(0x101, 0x101);
    auto locBuffer1 = make_shared<Hccl::LocalUbRmaBuffer>(buffer1, rdmaHandle);
    CommMemInfo memInfo1{};
    memInfo1.mem.addr = (void*)0x101;
    memInfo1.mem.size = (uint64_t)0x101;
    std::string tag = "buffer1";
    strncpy_s(memInfo1.memTag, sizeof(memInfo1.memTag), tag.c_str(), tag.size());
    memInfo1.mem.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    memInfo1.bufferHandle = static_cast<void*>(locBuffer1.get());

    std::vector<CommMemInfo*> memInfos{};
    memInfos.push_back(&memInfo0);
    memInfos.push_back(&memInfo1);

    void **memHandles = reinterpret_cast<void**>(memInfos.data());
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};

    HcclResult ret = hcomm::BuildBufferInfos(memHandles, 2, bufferInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    hcomm::CcuTransport::CcuConnectionInfo connInfo{};
    MOCKER_CPP(&hcomm::CcuConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuTransport::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ret = hcomm::CcuCreateTransport(fakeSocket, connInfo, bufferInfos, ccuTransport);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    BinaryStream binaryStream;
    ret = ccuTransport->BufferInfoPack(binaryStream, ccuTransport->locBufferInfos_);
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
    EXPECT_EQ(remoteMems[0].type, CommMemType::COMM_MEM_TYPE_DEVICE);
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

TEST_F(CcuTransportTest, ut_CcuTransport_UpdateMemInfo_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    RdmaHandle rdmaHandle = (void*)0x100;
    auto buffer0 = std::make_shared<Buffer>(0x100, 0x100);
    auto locBuffer0 = std::make_shared<Hccl::LocalUbRmaBuffer>(buffer0, rdmaHandle);
    CommMemInfo memInfo0{};
    memInfo0.mem.addr = (void*)0x100;
    memInfo0.mem.size = (uint64_t)0x100;
    memInfo0.bufferHandle = static_cast<void*>(locBuffer0.get());
    std::vector<CommMemInfo*> memInfos{};
    memInfos.push_back(&memInfo0);

    void **memHandles = reinterpret_cast<void**>(memInfos.data());
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    HcclResult ret = hcomm::BuildBufferInfos(memHandles, 1, bufferInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    hcomm::CcuTransport::CcuConnectionInfo connInfo{};
    MOCKER_CPP(&hcomm::CcuConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuTransport::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ret = hcomm::CcuCreateTransport(fakeSocket, connInfo, bufferInfos, ccuTransport);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    auto buffer1 = std::make_shared<Buffer>(0x101, 0x101);
    hcomm::CcuTransport::CclBufferInfo bufInfo1{};
    bufInfo1.addr = (uint64_t)0x101;
    bufInfo1.size = (uint32_t)0x101;
    std::string tag = "buffer1";
    memcpy_s(bufInfo1.memTag.data(), bufInfo1.memTag.size(), tag.c_str(), tag.size());
    bufInfo1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferVecTemp{};
    bufferVecTemp.push_back(bufInfo1);

    BinaryStream binaryStream;
    ret = ccuTransport->BufferInfoPack(binaryStream, bufferVecTemp);
    binaryStream.Dump(ccuTransport->sendData_);
    ccuTransport->recvData_ = ccuTransport->sendData_;

    MOCKER(&Hccl::EnvConfig::Parse).stubs().will(ignoreReturnValue());
    Hccl::EnvSocketConfig fakeSocketConfig{};
    MOCKER(&Hccl::EnvConfig::GetSocketConfig).stubs().will(returnValue(fakeSocketConfig));
    MOCKER(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().will(returnValue(100));
    ret = ccuTransport->UpdateMemInfo(bufferVecTemp);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::CcuTransport::CclBufferInfo &bufInfo = ccuTransport->locBufferInfos_[1];
    const char* src = bufInfo.memTag.data();
    std::string tagCopy(src, strnlen(src, HCCL_RES_TAG_MAX_LEN));
    EXPECT_EQ(tagCopy, "buffer1");
    EXPECT_EQ(bufInfo.type, CommMemType::COMM_MEM_TYPE_DEVICE);
    EXPECT_EQ(bufInfo.addr, (uint64_t)0x101);
    EXPECT_EQ(bufInfo.size, (uint32_t)0x101);
}

TEST_F(CcuTransportTest, ut_CcuTransport_UpdateMemInfo_When_Timeout_Expect_ReturnIsHCCL_E_TIMEOUT)
{
    std::unique_ptr<hcomm::CcuConnection> ccuConnection{nullptr};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    bufferInfos.emplace_back();
    hcomm::CcuTransport ccuTransport{fakeSocket, std::move(ccuConnection), bufferInfos};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferVecTemp{};
    bufferVecTemp.emplace_back();
    MOCKER(&Hccl::EnvConfig::Parse).stubs().will(ignoreReturnValue());
    Hccl::EnvSocketConfig fakeSocketConfig{};
    MOCKER(&Hccl::EnvConfig::GetSocketConfig).stubs().will(returnValue(fakeSocketConfig));
    MOCKER(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().will(returnValue(100));
    Hccl::SocketStatus fakeSocketStatus = Hccl::SocketStatus::TIMEOUT;
    MOCKER(&Hccl::Socket::GetAsyncStatus).stubs().will(returnValue(fakeSocketStatus));
    HcclResult ret = ccuTransport.UpdateMemInfo(bufferVecTemp);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(CcuTransportTest, ut_CcuTransport_UpdateMemInfo_When_bufferNumIs0_Expect_ReturnIsHCCL_SUCCESS)
{
    std::unique_ptr<hcomm::CcuConnection> ccuConnection{nullptr};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    bufferInfos.emplace_back();
    hcomm::CcuTransport ccuTransport{fakeSocket, std::move(ccuConnection), bufferInfos};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferVecTemp{};
    HcclResult ret = ccuTransport.UpdateMemInfo(bufferVecTemp);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}