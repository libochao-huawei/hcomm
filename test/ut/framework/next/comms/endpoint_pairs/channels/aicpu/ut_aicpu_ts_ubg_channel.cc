/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "aicpu/aicpu_ts_ubg_channel.h"

#define private public
using namespace hcomm;

class AicpuTsUbgChannelTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

struct FakeEndpoint {
    EndpointDesc desc;
    void* rdmaHandle{reinterpret_cast<void*>(0xDEADBEEF)};
    FakeEndpoint() {
        memset(&desc, 0, sizeof(desc));
        Hccl::IpAddress localIp("127.0.0.1");
        desc.protocol = COMM_PROTOCOL_UBG;
        desc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        desc.commAddr.addr = localIp.GetBinaryAddress().addr;
        desc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    }
    EndpointDesc GetEndpointDesc() { return desc; }
    void* GetRdmaHandle() { return rdmaHandle; }
    Hccl::IpAddress GetIpAddress() const { return Hccl::IpAddress("127.0.0.1"); }
};

class FakeSocket : public Hccl::Socket {
public:
    FakeSocket(Hccl::SocketStatus status = Hccl::SocketStatus::OK) :
        Hccl::Socket(nullptr, Hccl::IpAddress(), 0, Hccl::IpAddress(), "fake", Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE),
        status_(status) {}
    void SendAsync(const u8 *sendBuf, u32 size) { sent_.insert(sent_.end(), sendBuf, sendBuf + size); }
    void RecvAsync(u8 *recvBuf, u32 size) {
        if (recvBuf && size) {
            if (!sent_.empty()) {
                u32 copySize = static_cast<u32>(std::min<size_t>(sent_.size(), static_cast<size_t>(size)));
                memcpy(recvBuf, sent_.data(), copySize);
                if (copySize < size) std::memset(recvBuf + copySize, 0, size - copySize);
                sent_.erase(sent_.begin(), sent_.begin() + copySize);
            } else {
                std::memset(recvBuf, 0, size);
            }
        }
    }
    Hccl::SocketStatus GetAsyncStatus() { return status_; }
    Hccl::SocketRole GetRole() const { return Hccl::SocketRole::SERVER; }

    Hccl::SocketStatus status_;
    std::vector<u8> sent_;
};

namespace Hccl {
class FakeLocalUbRmaBuffer : public Hccl::LocalUbRmaBuffer {
public:
    FakeLocalUbRmaBuffer(std::shared_ptr<Hccl::Buffer> b, void* rdma): LocalUbRmaBuffer(b, rdma) {}
    string Describe() const override { return "hello"; }
};
class FakeUbLocalNotify : public Hccl::UbLocalNotify {
public:
    FakeUbLocalNotify(void* rdma, bool dev) : UbLocalNotify(rdma, dev) {}
};
}

static HcommChannelDesc MakeFakeChannelDesc(FakeSocket* sock) {
    HcommChannelDesc d{};
    d.socket = reinterpret_cast<void*>(sock);
    d.notifyNum = 0;
    d.exchangeAllMems = false;
    d.memHandles = nullptr;
    d.memHandleNum = 0;
    return d;
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetChannelKind_Returns_AICPU_TS_UBG) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);
    EXPECT_EQ(ch.GetChannelKind(), HcommChannelKind::AICPU_TS_UBG);
}

TEST_F(AicpuTsUbgChannelTest, Ut_Clean_WithoutInit_Returns_SUCCESS) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    auto ret = ch.Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetNotifyNum_Returns_Value) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    ch.notifyNum_ = 42;
    uint32_t n = 0;
    auto ret = ch.GetNotifyNum(&n);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(n, 42u);
}

TEST_F(AicpuTsUbgChannelTest, Ut_Init_MockedHelpers_Returns_SUCCESS) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket(Hccl::SocketStatus::OK);
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);
    AicpuTsUbgChannel ch(ep, desc);

    ch.Init();

    SUCCEED();

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_PackingHelpers_NoCrash) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    Hccl::BinaryStream bs;
    ch.NotifyVecPack(bs);
    std::vector<Hccl::LocalRmaBuffer*> emptyBuf;
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> tags;
    ch.BufferVecPack(bs, emptyBuf, tags);
    ch.ConnVecPack(bs);

    SUCCEED();
}

TEST_F(AicpuTsUbgChannelTest, Ut_H2DResPack_Packs_Data) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);

    ch.channelStatus = ChannelStatus::READY;
    ch.notifyNum_ = 1;
    ch.bufferNum_ = 2;
    ch.connNum_ = 1;

    auto notifyUptr = std::make_unique<Hccl::FakeUbLocalNotify>(fe.GetRdmaHandle(), true);
    Hccl::UbLocalNotify *rawNotify = notifyUptr.get();
    ch.localNotifies_.push_back(std::move(notifyUptr));
    ch.commonRes_.notifyVec.push_back(rawNotify);

    auto rmtUbRmaBufPtr = std::make_unique<Hccl::RemoteUbRmaBuffer>(fe.GetRdmaHandle());
    ch.rmtNotifyVec_.push_back(std::move(rmtUbRmaBufPtr));

    auto buffer = std::make_shared<Hccl::Buffer>(0x100, 0x100);
    auto locUbRmaBufPtr = std::make_unique<Hccl::FakeLocalUbRmaBuffer>(buffer, fe.GetRdmaHandle());
    Hccl::FakeLocalUbRmaBuffer *locUbRmaBuf = locUbRmaBufPtr.get();
    ch.commonRes_.bufferVec.push_back(locUbRmaBuf);
    ch.commonRes_.bufferVec.push_back(nullptr);

    auto rmtBufPtr = std::make_unique<Hccl::RemoteUbRmaBuffer>(fe.GetRdmaHandle());
    ch.rmtBufferVec_.push_back(std::move(rmtBufPtr));

    Hccl::DevUbUbgConnection ubgConn(fe.GetRdmaHandle(), ipAddress, ipAddress, Hccl::OpMode::OPBASE, true,
        Hccl::HrtUbJfcMode::STARS_POLL, ipAddress, ipAddress);
    ch.commonRes_.connVec.push_back(&ubgConn);

    std::vector<char> out{};
    HcclResult ret = ch.H2DResPack(out);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_BuildConnection_Creates_UbgConn) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);
    ch.localEp_ = fe.GetEndpointDesc();
    ch.remoteEp_ = fe.GetEndpointDesc();
    ch.rdmaHandle_ = fe.GetRdmaHandle();
    Hccl::IpAddress locAddr = fe.GetIpAddress();
    Hccl::IpAddress rmtAddr = fe.GetIpAddress();
    ch.locAddr_ = locAddr;
    ch.rmtAddr_ = rmtAddr;

    MOCKER_CPP(&Hccl::TpManager::Init).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER_CPP(hrtGetDevice, s32(s32*)).stubs().will(invoke([](s32* id) { *id = 0; return HCCL_SUCCESS; }));

    auto ret = ch.BuildConnection();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ASSERT_EQ(ch.commonRes_.connVec.size(), 1u);
    auto* conn = dynamic_cast<Hccl::DevUbUbgConnection*>(ch.commonRes_.connVec[0]);
    ASSERT_NE(conn, nullptr);

    delete fakeSock;
}

class FakeRemoteUbRmaBuffer : public Hccl::RemoteUbRmaBuffer {
public:
    FakeRemoteUbRmaBuffer(void* rdmaHandle, uint64_t addr, size_t size, HcclMemType type, const std::string& tag)
        : Hccl::RemoteUbRmaBuffer(rdmaHandle) {
        this->addr = addr;
        this->size = size;
        this->memType = type;
        this->memTag = tag;
    }
};

TEST_F(AicpuTsUbgChannelTest, Ut_GetRemoteMem_NullParams_ReturnsError) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    HcclMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    char** memTags = nullptr;

    EXPECT_EQ(ch.GetRemoteMem(nullptr, &memNum, memTags), HCCL_E_PARA);
    EXPECT_EQ(ch.GetRemoteMem(&remoteMem, nullptr, memTags), HCCL_E_PARA);
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetRemoteMem_NoBuffers_ReturnsSuccessWithZero) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    ch.rmtBufferVec_.clear();

    HcclMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    char* memTags[2] = {nullptr, nullptr};

    HcclResult ret = ch.GetRemoteMem(&remoteMem, &memNum, memTags);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(remoteMem, nullptr);
    EXPECT_EQ(memNum, 0U);
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetRemoteMem_WithBuffers_ReturnsCorrectData) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    void* fakeRdma = reinterpret_cast<void*>(0x1234);
    auto buf1 = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x1000, 4096, HCCL_MEM_TYPE_DEVICE, "ccl_buffer");
    auto buf2 = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x2000, 8192, HCCL_MEM_TYPE_HOST, "user_buffer");
    ch.rmtBufferVec_.push_back(std::move(buf1));
    ch.rmtBufferVec_.push_back(std::move(buf2));

    HcclMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    char* memTags[2];
    HcclResult ret = ch.GetRemoteMem(&remoteMem, &memNum, memTags);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memNum, 2U);
    ASSERT_NE(remoteMem, nullptr);

    EXPECT_EQ(remoteMem[0].type, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(remoteMem[0].addr, reinterpret_cast<void*>(0x1000));
    EXPECT_EQ(remoteMem[0].size, 4096U);
    EXPECT_STREQ(memTags[0], "ccl_buffer");

    EXPECT_EQ(remoteMem[1].type, HCCL_MEM_TYPE_HOST);
    EXPECT_EQ(remoteMem[1].addr, reinterpret_cast<void*>(0x2000));
    EXPECT_EQ(remoteMem[1].size, 8192U);
    EXPECT_STREQ(memTags[1], "user_buffer");
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetStatus_WhenSocketTimeout_Returns_SOCKET_OK) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket(Hccl::SocketStatus::TIMEOUT);
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);
    AicpuTsUbgChannel ch(ep, desc);

    ch.channelDesc_.socket = reinterpret_cast<void*>(fakeSock);
    ch.socket_ = reinterpret_cast<Hccl::Socket*>(fakeSock);

    EXPECT_EQ(ch.channelStatus, ChannelStatus::INIT);

    auto status = ch.GetStatus();
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_BuildConnection_WhenInvalidProtocol_ReturnsError) {
    FakeEndpoint fe;
    fe.desc.protocol = COMM_PROTOCOL_ROCE;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);
    ch.localEp_ = fe.GetEndpointDesc();
    ch.remoteEp_ = fe.GetEndpointDesc();
    ch.rdmaHandle_ = fe.GetRdmaHandle();
    Hccl::IpAddress locAddr = fe.GetIpAddress();
    Hccl::IpAddress rmtAddr = fe.GetIpAddress();
    ch.locAddr_ = locAddr;
    ch.rmtAddr_ = rmtAddr;

    auto ret = ch.BuildConnection();
    EXPECT_NE(ret, HCCL_SUCCESS);

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_BuildConnection_WhenInvalidCommAddr_ReturnsError) {
    FakeEndpoint fe;
    fe.desc.commAddr.type = COMM_ADDR_TYPE_RESERVED;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);
    ch.localEp_ = fe.GetEndpointDesc();
    ch.remoteEp_ = fe.GetEndpointDesc();
    ch.rdmaHandle_ = fe.GetRdmaHandle();
    Hccl::IpAddress locAddr = fe.GetIpAddress();
    Hccl::IpAddress rmtAddr = fe.GetIpAddress();
    ch.locAddr_ = locAddr;
    ch.rmtAddr_ = rmtAddr;

    auto ret = ch.BuildConnection();
    EXPECT_NE(ret, HCCL_SUCCESS);

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_BuildConnection_WhenHrtGetDeviceFails_ReturnsError) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);
    ch.localEp_ = fe.GetEndpointDesc();
    ch.remoteEp_ = fe.GetEndpointDesc();
    ch.rdmaHandle_ = fe.GetRdmaHandle();
    Hccl::IpAddress locAddr = fe.GetIpAddress();
    Hccl::IpAddress rmtAddr = fe.GetIpAddress();
    ch.locAddr_ = locAddr;
    ch.rmtAddr_ = rmtAddr;

    MOCKER_CPP(hrtGetDevice, s32(s32*)).stubs().will(returnValue(HCCL_E_INTERNAL));

    auto ret = ch.BuildConnection();
    EXPECT_NE(ret, HCCL_SUCCESS);

    delete fakeSock;
}

static void stub_Socket_SendAsync(Hccl::Socket *self, const u8 *sendBuf, u32 size)
{
    if (!self || !sendBuf || size == 0) return;
    auto *fs = dynamic_cast<FakeSocket *>(self);
    if (fs) {
        fs->sent_.insert(fs->sent_.end(), sendBuf, sendBuf + size);
    }
}

static void stub_Socket_RecvAsync(Hccl::Socket *self, u8 *recvBuf, u32 size)
{
    if (!self || !recvBuf || size == 0) return;
    auto *fs = dynamic_cast<FakeSocket *>(self);
    if (fs) {
        if (!fs->sent_.empty()) {
            u32 copySize = static_cast<u32>(std::min<size_t>(fs->sent_.size(), static_cast<size_t>(size)));
            memcpy(recvBuf, fs->sent_.data(), copySize);
            if (copySize < size) std::memset(recvBuf + copySize, 0, size - copySize);
            fs->sent_.erase(fs->sent_.begin(), fs->sent_.begin() + copySize);
            return;
        }
    }
    std::memset(recvBuf, 0, size);
}

TEST_F(AicpuTsUbgChannelTest, Ut_ProcessUbgState_AllStates_Transitions) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();

    MOCKER_CPP(&Hccl::Socket::SendAsync, void(Hccl::Socket::*)(const u8 *, u32))
        .stubs()
        .with(any(), any())
        .will(invoke(stub_Socket_SendAsync));
    MOCKER_CPP(&Hccl::Socket::RecvAsync, void(Hccl::Socket::*)(u8 *, u32))
        .stubs()
        .with(any(), any())
        .will(invoke(stub_Socket_RecvAsync));
    MOCKER_CPP(&AicpuTsUbgChannel::BuildConnection).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);

    FakeEndpoint localEp;
    FakeEndpoint remoteEp;
    ch.localEp_ = localEp.GetEndpointDesc();
    ch.remoteEp_ = remoteEp.GetEndpointDesc();

    ch.channelDesc_.socket = reinterpret_cast<void*>(fakeSock);
    ch.socket_ = reinterpret_cast<Hccl::Socket*>(fakeSock);

    ch.channelDesc_.exchangeAllMems = false;

    ch.commonRes_.connVec = {};
    ch.commonRes_.bufferVec = {};
    ch.connNum_ = 0;
    ch.bufferNum_ = 0;

    ch.channelStatus = ChannelStatus::SOCKET_OK;
    ch.ubgStatus = AicpuTsUbgChannel::UbgStatus::INIT;

    int iter = 0;
    while (ch.channelStatus != ChannelStatus::READY && iter < 200) {
        ch.GetStatus();
        iter++;
    }

    EXPECT_EQ(ch.channelStatus, ChannelStatus::READY);

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetUserRemoteMem_NullParams_ReturnsError) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    void* fakeRdma = reinterpret_cast<void*>(0x1234);
    auto cclBuf = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x1000, 4096, HCCL_MEM_TYPE_DEVICE, "ccl_buffer");
    ch.rmtBufferVec_.push_back(std::move(cclBuf));

    CommMem* remoteMem = nullptr;
    char** memTag = nullptr;
    uint32_t memNum = 0;

    EXPECT_EQ(ch.GetUserRemoteMem(nullptr, &memTag, &memNum), HCCL_E_PARA);
    EXPECT_EQ(ch.GetUserRemoteMem(&remoteMem, nullptr, &memNum), HCCL_E_PARA);
    EXPECT_EQ(ch.GetUserRemoteMem(&remoteMem, &memTag, nullptr), HCCL_E_PARA);
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetUserRemoteMem_OnlyCclBuffer_ReturnsSuccessWithZero) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    void* fakeRdma = reinterpret_cast<void*>(0x1234);
    auto cclBuf = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x1000, 4096, HCCL_MEM_TYPE_DEVICE, "ccl_buffer");
    ch.rmtBufferVec_.push_back(std::move(cclBuf));

    ch.cacheValid_ = false;

    CommMem* remoteMem = nullptr;
    char** memTag = nullptr;
    uint32_t memNum = 0;

    HcclResult ret = ch.GetUserRemoteMem(&remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memNum, 0U);
    EXPECT_EQ(remoteMem, nullptr);
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetUserRemoteMem_WithUserBuffers_ReturnsCorrectData) {
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);

    void* fakeRdma = reinterpret_cast<void*>(0x1234);
    auto cclBuf = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x1000, 4096, HCCL_MEM_TYPE_DEVICE, "ccl");
    auto userBuf1 = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x2000, 8192, HCCL_MEM_TYPE_DEVICE, "user1");
    auto userBuf2 = std::make_unique<FakeRemoteUbRmaBuffer>(fakeRdma, 0x3000, 16384, HCCL_MEM_TYPE_DEVICE, "user2");
    ch.rmtBufferVec_.push_back(std::move(cclBuf));
    ch.rmtBufferVec_.push_back(std::move(userBuf1));
    ch.rmtBufferVec_.push_back(std::move(userBuf2));
    ch.remoteUserMemTag_.clear();
    std::array<char, HCCL_RES_TAG_MAX_LEN> tag0 = {};
    std::array<char, HCCL_RES_TAG_MAX_LEN> tag1 = {};
    std::array<char, HCCL_RES_TAG_MAX_LEN> tag2 = {};
    strcpy_s(tag0.data(), tag0.size(), "ccl");
    strcpy_s(tag1.data(), tag1.size(), "user1");
    strcpy_s(tag2.data(), tag2.size(), "user2");
    ch.remoteUserMemTag_.push_back(tag0);
    ch.remoteUserMemTag_.push_back(tag1);
    ch.remoteUserMemTag_.push_back(tag2);

    ch.cacheValid_ = false;

    CommMem* remoteMem = nullptr;
    char** memTag = nullptr;
    uint32_t memNum = 0;

    HcclResult ret = ch.GetUserRemoteMem(&remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memNum, 2U);
    ASSERT_NE(remoteMem, nullptr);
    ASSERT_NE(memTag, nullptr);

    EXPECT_EQ(remoteMem[0].type, COMM_MEM_TYPE_DEVICE);
    EXPECT_EQ(remoteMem[0].addr, reinterpret_cast<void*>(0x2000));
    EXPECT_EQ(remoteMem[0].size, 8192U);
    EXPECT_STREQ(memTag[0], "user1");

    EXPECT_EQ(remoteMem[1].type, COMM_MEM_TYPE_DEVICE);
    EXPECT_EQ(remoteMem[1].addr, reinterpret_cast<void*>(0x3000));
    EXPECT_EQ(remoteMem[1].size, 16384U);
    EXPECT_STREQ(memTag[1], "user2");
}

TEST_F(AicpuTsUbgChannelTest, Ut_GetStatusReady_SocketNotNullptr) {
    MOCKER_CPP(&SocketMgr::GetSocket).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&SocketMgr::PutSocket).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuTsUbgChannel::IsSocketReady).stubs().will(returnValue(true));
    HcommChannelDesc desc{};
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AicpuTsUbgChannel ch(ep, desc);
    ch.ubgStatus = AicpuTsUbgChannel::UbgStatus::SET_READY;
    ch.GetStatus();
}

TEST_F(AicpuTsUbgChannelTest, Ut_H2DResPack_WithEmptyConnVec_Returns_SUCCESS) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);

    ch.channelStatus = ChannelStatus::READY;
    ch.notifyNum_ = 1;
    ch.bufferNum_ = 0;
    ch.connNum_ = 0;

    auto notifyUptr = std::make_unique<Hccl::FakeUbLocalNotify>(fe.GetRdmaHandle(), true);
    Hccl::UbLocalNotify *rawNotify = notifyUptr.get();
    ch.localNotifies_.push_back(std::move(notifyUptr));
    ch.commonRes_.notifyVec.push_back(rawNotify);

    auto rmtUbRmaBufPtr = std::make_unique<Hccl::RemoteUbRmaBuffer>(fe.GetRdmaHandle());
    ch.rmtNotifyVec_.push_back(std::move(rmtUbRmaBufPtr));

    std::vector<char> out{};
    HcclResult ret = ch.H2DResPack(out);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete fakeSock;
}

TEST_F(AicpuTsUbgChannelTest, Ut_H2DResPack_WithNullBuffer_Returns_SUCCESS) {
    FakeEndpoint fe;
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(&fe);
    auto fakeSock = new FakeSocket();
    HcommChannelDesc desc = MakeFakeChannelDesc(fakeSock);

    AicpuTsUbgChannel ch(ep, desc);

    ch.channelStatus = ChannelStatus::READY;
    ch.notifyNum_ = 0;
    ch.bufferNum_ = 1;
    ch.connNum_ = 0;

    ch.commonRes_.bufferVec.push_back(nullptr);

    auto rmtBufPtr = std::make_unique<Hccl::RemoteUbRmaBuffer>(fe.GetRdmaHandle());
    ch.rmtBufferVec_.push_back(std::move(rmtBufPtr));

    std::vector<char> out{};
    HcclResult ret = ch.H2DResPack(out);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete fakeSock;
}