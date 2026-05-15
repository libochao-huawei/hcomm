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

class CcuTransportTest : public BaseInit {
protected:
    void SetUp() override {
        BaseInit::SetUp();
        Hccl::IpAddress localIp("1.0.0.0");
        Hccl::IpAddress remoteIp("2.0.0.0");
        fakeSocket_ = new Hccl::Socket(nullptr, localIp, 100, remoteIp, "test", Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
        setenv("HCCL_DFS_CONFIG", "task_exception:on", 1);
        MOCKER(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().will(returnValue(100));
    }

    void TearDown() override {
        delete fakeSocket_;
        unsetenv("HCCL_DFS_CONFIG");
        GlobalMockObject::verify();
        BaseInit::TearDown();
    }

    HcclResult CreateCcuTransportWithBuffer(std::vector<hcomm::CcuTransport::CclBufferInfo> &bufferInfos,
        std::unique_ptr<hcomm::CcuTransport> &ccuTransport)
    {
        hcomm::CcuTransport::CcuConnectionInfo connInfo{};
        MOCKER_CPP(&hcomm::CcuConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&hcomm::CcuTransport::Init).stubs().will(returnValue(HCCL_SUCCESS));
        return hcomm::CcuCreateTransport(fakeSocket_, connInfo, bufferInfos, ccuTransport);
    }

    std::unique_ptr<hcomm::CcuTransport> CreateCcuTransportWithEmptyBuffer()
    {
        std::unique_ptr<hcomm::CcuConnection> ccuConnection{nullptr};
        std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{hcomm::CcuTransport::CclBufferInfo{}};
        return std::make_unique<hcomm::CcuTransport>(fakeSocket_, std::move(ccuConnection), bufferInfos);
    }

    void PrepareBinaryStreamWithBufferInfos(Hccl::BinaryStream &binaryStream, u32 bufferNum)
    {
        binaryStream << bufferNum;
        for (u32 i = 0; i < bufferNum; ++i) {
            hcomm::CcuTransport::CclBufferInfo bufferInfo;
            bufferInfo.addr = 0x10000000 + i;
            bufferInfo.size = 4096 + i;
            bufferInfo.tokenId = i;
            bufferInfo.tokenValue = i + 1;
            bufferInfo.type = CommMemType::COMM_MEM_TYPE_DEVICE;
            bufferInfo.Pack(binaryStream);
        }
    }

    CommMemInfo BuildMemInfo(u64 addr, u64 size, const std::string &tag = "",
        CommMemType type = CommMemType{})
    {
        auto buffer = std::make_shared<Hccl::Buffer>(addr, size);
        auto locBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(buffer, rdmaHandle_);
        locBuffers_.push_back(locBuffer);
        CommMemInfo memInfo{};
        memInfo.mem.addr = (void*)addr;
        memInfo.mem.size = (uint64_t)size;
        memInfo.bufferHandle = static_cast<void*>(locBuffer.get());
        if (!tag.empty()) {
            strncpy_s(memInfo.memTag, sizeof(memInfo.memTag), tag.c_str(), tag.size());
        }
        memInfo.mem.type = type;
        return memInfo;
    }

    Hccl::Socket *fakeSocket_;
    RdmaHandle rdmaHandle_ = (void*)0x100;
    std::vector<std::shared_ptr<Hccl::LocalUbRmaBuffer>> locBuffers_;
};

TEST_F(CcuTransportTest, ut_CcuTransport_GetUserRemoteMem_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    auto memInfo0 = BuildMemInfo(0x100, 0x100);
    auto memInfo1 = BuildMemInfo(0x101, 0x101, "buffer1", CommMemType::COMM_MEM_TYPE_DEVICE);
    std::vector<CommMemInfo*> memInfos{&memInfo0, &memInfo1};
    void **memHandles = reinterpret_cast<void**>(memInfos.data());
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    HcclResult ret = hcomm::BuildBufferInfos(memHandles, 2, bufferInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    ret = CreateCcuTransportWithBuffer(bufferInfos, ccuTransport);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Hccl::BinaryStream binaryStream;
    ret = ccuTransport->BufferInfoPack(binaryStream, ccuTransport->locBufferInfos_);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = ccuTransport->BufferInfoUnpack(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    CommMem *remoteMems;
    u32 memNum;
    ret = ccuTransport->GetUserRemoteMem(&remoteMems, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remoteMems[0].type, CommMemType::COMM_MEM_TYPE_DEVICE);
    EXPECT_EQ(remoteMems[0].addr, (void *)0x101);
    EXPECT_EQ(remoteMems[0].size, (uint64_t)0x101);
}

TEST_F(CcuTransportTest, ut_CcuTransport_GetUserRemoteMem_When_bufferNumIs0_Expect_ReturnIsHCCL_E_PARA)
{
    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    HcclResult ret = CreateCcuTransportWithBuffer(bufferInfos, ccuTransport);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CcuTransportTest, ut_CcuTransport_UpdateMemInfo_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    auto memInfo0 = BuildMemInfo(0x100, 0x100);
    std::vector<CommMemInfo*> memInfos{&memInfo0};
    void **memHandles = reinterpret_cast<void**>(memInfos.data());
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{};
    HcclResult ret = hcomm::BuildBufferInfos(memHandles, 1, bufferInfos);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::unique_ptr<hcomm::CcuTransport> ccuTransport{};
    ret = CreateCcuTransportWithBuffer(bufferInfos, ccuTransport);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcomm::CcuTransport::CclBufferInfo bufInfo1{};
    bufInfo1.addr = (uint64_t)0x101;
    bufInfo1.size = (uint32_t)0x101;
    bufInfo1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferVecTemp{bufInfo1};

    Hccl::BinaryStream binaryStream;
    ret = ccuTransport->BufferInfoPack(binaryStream, bufferVecTemp);
    binaryStream.Dump(ccuTransport->sendData_);
    ccuTransport->recvData_ = ccuTransport->sendData_;
    ccuTransport->exchangeDataSize_ = ccuTransport->recvData_.size();

    ret = ccuTransport->UpdateMemInfo(bufferVecTemp);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::CcuTransport::CclBufferInfo &bufInfo = ccuTransport->locBufferInfos_[1];
    EXPECT_EQ(bufInfo.type, CommMemType::COMM_MEM_TYPE_DEVICE);
    EXPECT_EQ(bufInfo.addr, (uint64_t)0x101);
    EXPECT_EQ(bufInfo.size, (uint32_t)0x101);
}

TEST_F(CcuTransportTest, ut_CcuTransport_UpdateMemInfo_When_Timeout_Expect_ReturnIsHCCL_E_TIMEOUT)
{
    std::unique_ptr<hcomm::CcuConnection> ccuConnection{nullptr};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{hcomm::CcuTransport::CclBufferInfo{}};
    hcomm::CcuTransport ccuTransport{fakeSocket_, std::move(ccuConnection), bufferInfos};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferVecTemp{hcomm::CcuTransport::CclBufferInfo{}};
    Hccl::SocketStatus fakeSocketStatus = Hccl::SocketStatus::TIMEOUT;
    MOCKER(&Hccl::Socket::GetAsyncStatus).stubs().will(returnValue(fakeSocketStatus));
    HcclResult ret = ccuTransport.UpdateMemInfo(bufferVecTemp);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(CcuTransportTest, ut_CcuTransport_UpdateMemInfo_When_bufferNumIs0_Expect_ReturnIsHCCL_SUCCESS)
{
    std::unique_ptr<hcomm::CcuConnection> ccuConnection{nullptr};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferInfos{hcomm::CcuTransport::CclBufferInfo{}};
    hcomm::CcuTransport ccuTransport{fakeSocket_, std::move(ccuConnection), bufferInfos};
    std::vector<hcomm::CcuTransport::CclBufferInfo> bufferVecTemp{};
    HcclResult ret = ccuTransport.UpdateMemInfo(bufferVecTemp);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CcuTransportTest, ut_CcuTransport_BufferInfoUnpack_When_rmtBufferNumIsZero_Expect_ReturnHCCL_E_PARA)
{
    auto ccuTransport = CreateCcuTransportWithEmptyBuffer();
    Hccl::BinaryStream binaryStream;
    u32 rmtBufferNumZero = 0;
    binaryStream << rmtBufferNumZero;

    HcclResult ret = ccuTransport->BufferInfoUnpack(binaryStream);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CcuTransportTest, ut_CcuTransport_BufferInfoUnpack_When_rmtBufferNumExceedsLimit_Expect_ReturnHCCL_E_PARA)
{
    auto ccuTransport = CreateCcuTransportWithEmptyBuffer();
    Hccl::BinaryStream binaryStream;
    u32 rmtBufferNumExceeds = 30001;
    binaryStream << rmtBufferNumExceeds;

    HcclResult ret = ccuTransport->BufferInfoUnpack(binaryStream);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(CcuTransportTest, ut_CcuTransport_BufferInfoUnpack_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    auto ccuTransport = CreateCcuTransportWithEmptyBuffer();
    Hccl::BinaryStream binaryStream;
    u32 rmtBufferNum = 2;
    PrepareBinaryStreamWithBufferInfos(binaryStream, rmtBufferNum);

    HcclResult ret = ccuTransport->BufferInfoUnpack(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(ccuTransport->rmtBufferInfos_.size(), rmtBufferNum);
    EXPECT_EQ(ccuTransport->remoteUserMemTag_.size(), rmtBufferNum);
}