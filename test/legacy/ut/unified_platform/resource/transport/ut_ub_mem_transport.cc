/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mockcpp/MockObject.h>
#include "virtual_topo.h"
#include "dev_ub_connection.h"
#include "task.h"
#define private public
#define protected public
#include "ub_mem_transport.h"
#include "ub_local_notify.h"
#include "local_ub_rma_buffer.h"
#include "socket_exception.h"
#include "exchange_ub_conn_dto.h"
#include "dev_buffer.h"
#include "rma_buffer.h"
#include "internal_exception.h"
#undef protected
#undef private
#include "stub_communicator_impl_trans_mgr.h"
#include "mem_transport_callback.h"
#include "dlprof_func.h"
#include "rdma_handle_manager.h"

using namespace Hccl;

static int memcpy_stub(void *dest, int dest_max, const void *src, int count)
{
    memcpy(dest, src, count);
    return 0;
}

class StubUbRmaConnection : public DevUbConnection {
public:
    StubUbRmaConnection(LinkData& linkData) : link(linkData), DevUbConnection((void *)0x100, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE)
    {
        status = RmaConnStatus::READY;
    }

    unique_ptr<BaseTask> PrepareRead(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                     const SqeConfig &config) override
    {
        u32 jettyId = 100;
        u64 funcId  = 100;
        u32 piVal   = 100;
        u64 dieId   = 100;
        return make_unique<TaskUbDbSend>(jettyId, funcId, piVal, dieId);
    }

    unique_ptr<BaseTask> PrepareReadReduce(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                           DataType datatype, ReduceOp reduceOp, const SqeConfig &config) override
    {
        u8 dwqe[64]{0};
        u32 jettyId = 100;
        u64 funcId  = 100;
        u32 dwqeSize = 64;
        u64 dieId   = 100;
        return make_unique<TaskUbDirectSend>(funcId, dieId, jettyId, dwqeSize, dwqe);
    }

    unique_ptr<BaseTask> PrepareWrite(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                      const SqeConfig &config) override
    {
        u32 jettyId = 100;
        u64 funcId  = 100;
        u32 piVal   = 100;
        u64 dieId   = 100;
        return make_unique<TaskUbDbSend>(jettyId, funcId, piVal, dieId);
    }
    
    unique_ptr<BaseTask> PrepareWriteWithNotify(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                                u64 data, const MemoryBuffer &remoteNotifyMemBuf,
                                                const SqeConfig &config) override
    {
        u32 jettyId = 100;
        u64 funcId  = 100;
        u32 piVal   = 100;
        u64 dieId   = 100;
        return make_unique<TaskUbDbSend>(jettyId, funcId, piVal, dieId);
    }

    unique_ptr<BaseTask> PrepareWriteReduce(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                            DataType datatype, ReduceOp reduceOp, const SqeConfig &config) override
    {
        u32 jettyId = 100;
        u64 funcId  = 100;
        u32 piVal   = 100;
        u64 dieId   = 100;
        return make_unique<TaskUbDbSend>(jettyId, funcId, piVal, dieId);
    }

    unique_ptr<BaseTask> PrepareInlineWriteReduce(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                                  DataType datatype, ReduceOp reduceOp, const SqeConfig &config)
    {
        u32 jettyId = 100;
        u64 funcId  = 100;
        u32 piVal   = 100;
        u64 dieId   = 100;
        return make_unique<TaskUbDbSend>(jettyId, funcId, piVal, dieId);
    }

    unique_ptr<BaseTask> PrepareInlineWrite(const MemoryBuffer &remoteMemBuf, u64 data,
                                            const SqeConfig &config) override
    {
        u64 dbAddr  = 100;
        u32 piVal   = 100;
        return make_unique<TaskWriteValue>(dbAddr, piVal);
    }

    unique_ptr<BaseTask> PrepareWriteReduceWithNotify(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                                      DataType datatype, ReduceOp reduceOp, u64 data,
                                                      const MemoryBuffer &remoteNotifyMemBuf,
                                                      const SqeConfig    &config) override
    {
        return nullptr;
    }

    string Describe() const override
    {
        return "StubUbRmaConnection";
    }

    RmaConnStatus GetStatus() override
    {
        return RmaConnStatus::READY;
    }

private:
    LinkData link;
};

class StubSocket : public Socket {
public:
    StubSocket() : Socket(nullptr, IpAddress("1.0.0.0"), 0, IpAddress("1.0.0.0"), "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE)
    {
        MOCKER(HrtRaSocketBlockSend).stubs().will(invoke(Send));
        MOCKER(HrtRaSocketBlockRecv).stubs().will(invoke(Recv));
    }

    static bool Send(Socket *This, const u8 *sendBuf, u32 size)
    {
        buffer.resize(size);
        memcpy(buffer.data(), sendBuf, size);
        return true;
    }

    static bool Recv(Socket *This, u8 *recvBuf, u32 size)
    {
        if(buffer.size() < size) {
            return false;
        }
        memcpy(recvBuf, buffer.data(), size);
        return true;
    }

private:
    static std::vector<char> buffer;
};

std::vector<char> StubSocket::buffer;

class UbMemTransportTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "UbMemTransportTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UbMemTransportTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in UbMemTransportTest SetUP" << std::endl;
        MOCKER(HrtMemAsyncCopy).stubs().with(any());
        MOCKER(HrtReduceAsync).stubs().with(any());
        std::pair<TokenIdHandle, uint32_t> fakeTokenInfo = std::make_pair(0x12345678, 1);
        MOCKER_CPP(&RdmaHandleManager::GetTokenIdInfo).stubs().will(returnValue(fakeTokenInfo));
        MOCKER(HrtGetStreamId).stubs().with(any()).will(returnValue(0));
        MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_910A2));
        fakeLocalOutParam.handle       = fakeNotifyHandleAddr;
        memcpy_s(fakeLocalOutParam.key, HRT_UB_MEM_KEY_MAX_LEN, fakeKey, HRT_UB_MEM_KEY_MAX_LEN);
        fakeLocalOutParam.tokenId      = fakeTokenId;
        fakeLocalOutParam.targetSegVa  = fakeTargetSegVa;
        fakeLocalOutParam.keySize      = fakeKeySize;
        MOCKER(HrtRaUbLocalMemReg).stubs().with(any(), any()).will(returnValue(fakeLocalOutParam));
        fakeRemoteOutParam.handle      = fakeNotifyHandleAddr;
        fakeRemoteOutParam.targetSegVa = fakeTargetSegVa;
        MOCKER(HrtRaUbRemoteMemImport).stubs().with(any(), any(), any(), any()).will(returnValue(fakeRemoteOutParam));
        MOCKER(HrtDeviceGetBareTgid).stubs().will(returnValue(fakePid));
        MOCKER(HrtGetDevice).stubs().will(returnValue(0));
        MOCKER(HrtNotifyCreate).stubs().will(returnValue((void *)(fakeNotifyHandleAddr)));
        MOCKER(HrtGetNotifyID).stubs().will(returnValue(fakeNotifyId));
        MOCKER(HrtNotifyGetAddr).stubs().with(any()).will(returnValue(fakeAddress));
        MOCKER(HrtNotifyGetOffset).stubs().will(returnValue(fakeOffset));

        MOCKER(HrtNotifyRecord).stubs().with(any());
        MOCKER(HrtNotifyWaitWithTimeOut).stubs().with(any());
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in UbMemTransportTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    RmaBufferSlice    locSlice;
    RmtRmaBufferSlice rmtSlice;
    std::shared_ptr<DevBuffer> devBuf = DevBuffer::Create(0x100, 0x100);

    HrtRaUbLocalMemRegOutParam fakeLocalOutParam;
    HrtRaUbRemMemImportedOutParam fakeRemoteOutParam;
    u64               fakeNotifyHandleAddr = 100;
    u64               fakeTargetSegVa      = 150;
    u32               fakeNotifyId         = 1;
    u64               fakeOffset           = 200;
    u64               fakeAddress          = 300;
    u32               fakePid              = 100;
    u32               fakeTokenId          = 100;
    u8                fakeKey[HRT_UB_MEM_KEY_MAX_LEN] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    u32               fakeKeySize          = 10;
};

TEST_F(UbMemTransportTest, UbMemTransport_describe)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::DEV_NET), 0, 1, 0, 1);
    void                             *rdmaHandle = (void *)0x100;
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes);
    transport.Describe();
}

TEST_F(UbMemTransportTest, UbMemTransport_get_status)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::P2P), 0, 1, 0, 1);
    void                             *rdmaHandle = (void *)0x100;
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes);

    MOCKER_CPP(&UbMemTransport::SendExchangeData).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&UbMemTransport::RecvExchangeData).stubs();
    MOCKER_CPP(&UbMemTransport::RecvDataProcess).stubs().will(returnValue(true));
    MOCKER_CPP(&UbMemTransport::SendFinish).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&UbMemTransport::RecvFinish).stubs().will(ignoreReturnValue());

    SocketStatus socketStatusInit = SocketStatus::INIT;
    SocketStatus socketStatusOK   = SocketStatus::OK;

    StubSocket stubSocket;
    transport.socket = &stubSocket;

    // 需要发送 finish
    TransportStatus transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::SOCKET_OK);

    int fakeFdStatus = SOCKET_CONNECTED;
    FdHandle fakeFdHandle = (void *)100;
    RaSocketFdHandleParam fakeParam(fakeFdHandle, fakeFdStatus);

    MOCKER(RaGetOneSocket).stubs()
        .with(any(), any())
        .will(returnValue(fakeParam));

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::SEND_DATA);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::RECV_DATA);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::PROCESS_DATA);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::CONN_OK);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::SEND_FIN);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::READY);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::RECV_FIN);

    // 复位并重新打桩
    transport.baseStatus = TransportStatus::INIT;
    transport.ubStatus = UbMemTransport::UbStatus::INIT;
    transport.socket = &fakeSocket;
    GlobalMockObject::verify();

    MOCKER_CPP(&UbMemTransport::SendExchangeData).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&UbMemTransport::RecvExchangeData).stubs();
    MOCKER_CPP(&UbMemTransport::RecvDataProcess).stubs().will(returnValue(false));
    MOCKER_CPP(&UbMemTransport::SendFinish).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&UbMemTransport::RecvFinish).stubs().will(ignoreReturnValue());
    
    MOCKER(RaGetOneSocket).stubs()
        .with(any(), any())
        .will(returnValue(fakeParam));

    // 不需要发送finish
    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::SOCKET_OK);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::SEND_DATA);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::SOCKET_OK);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::RECV_DATA);

    transStatus = transport.GetStatus();
    EXPECT_EQ(transStatus, TransportStatus::READY);
    EXPECT_EQ(transport.ubStatus, UbMemTransport::UbStatus::RECV_FIN);
}

TEST_F(UbMemTransportTest, UbMemTransport_send_recv_exchange_data)
{
    std::pair<TokenIdHandle, uint32_t> retPair = {1, 1};
    MOCKER_CPP(&RdmaHandleManager::GetTokenIdInfo).stubs().will(returnValue(retPair));

    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::P2P), 0, 1, 0, 1);
    void                             *rdmaHandle = (void *)0x100;
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    StubUbRmaConnection  stubRmaConnection(link);
    RmaConnection       *rmaConnection    = &stubRmaConnection;
    locRes.connVec.push_back(rmaConnection);
    UbLocalNotify        ubLocalNotify(rdmaHandle);
    BaseLocalNotify     *validLocalNotify = &ubLocalNotify;
    locRes.notifyVec.push_back(validLocalNotify);
    LocalUbRmaBuffer     ubLocalRmaBuffer(devBuf, rdmaHandle);
    LocalRmaBuffer      *validLocalRmaBuffer = &ubLocalRmaBuffer;
    locRes.bufferVec.push_back(validLocalRmaBuffer);

    RtsCntNotify   rtsCntNotify;
    LocalCntNotify localCntNotify(rdmaHandle, &rtsCntNotify);
    locCntRes.vec.push_back(&localCntNotify);
    locCntRes.desc.push_back('0');
    locCntRes.desc.push_back(0);

    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes);
    EXPECT_THROW(transport.GetUniqueId(), InternalException);
    MOCKER(memcpy_s).stubs().with().will(invoke(memcpy_stub));
    MOCKER(HrtDeviceGetBareTgid).stubs().will(returnValue(100));
    std::string fakeKeyDesc = "fakeKeyDesc";
    MOCKER(HrtRaGetKeyDescribe).stubs().will(returnValue(fakeKeyDesc));

    StubSocket stubSocket;
    transport.socket = &stubSocket;

    int fakeFdStatus = SOCKET_CONNECTED;
    FdHandle fakeFdHandle = (void *)100;
    RaSocketFdHandleParam fakeParam(fakeFdHandle, fakeFdStatus);

    MOCKER(RaGetOneSocket).stubs()
        .with(any(), any())
        .will(returnValue(fakeParam));

    int max_times = 10;
    while (!transport.IsSocketReady()) {
        if (max_times-- <= 0) {
            std::cout << "while loop retry max times." << std::endl;
            break;
        }
    }
    EXPECT_NO_THROW(transport.SendExchangeData());

    max_times = 10;
    while (!transport.IsSocketReady()) {
        if (max_times-- <= 0) {
            std::cout << "while loop retry max times." << std::endl;
            break;
        }
    }
    EXPECT_NO_THROW(transport.RecvExchangeData());
    max_times = 10;
    while (!transport.IsSocketReady()) {
        if (max_times-- <= 0) {
            std::cout << "while loop retry max times." << std::endl;
            break;
        }
    }
}

TEST_F(UbMemTransportTest, UbMemTransport_send_recv_finish)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::P2P), 0, 1, 0, 1);
    void                             *rdmaHandle = (void *)0x100;
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes);

    MOCKER(memcpy_s).stubs().will(returnValue(-1));
    EXPECT_THROW(transport.SendFinish(), SocketException);

    GlobalMockObject::verify();
    MOCKER(memcpy_s).stubs().with().will(invoke(memcpy_stub));
    MOCKER(HrtDeviceGetBareTgid).stubs().will(returnValue(100));

    StubSocket stubSocket;
    transport.socket = &stubSocket;

    int fakeFdStatus = SOCKET_CONNECTED;
    FdHandle fakeFdHandle = (void *)100;
    RaSocketFdHandleParam fakeParam(fakeFdHandle, fakeFdStatus);

    MOCKER(RaGetOneSocket).stubs()
        .with(any(), any())
        .will(returnValue(fakeParam));

    int max_times = 10;
    while (!transport.IsSocketReady()) {
        if (max_times-- <= 0) {
            std::cout << "while loop retry max times." << std::endl;
            break;
        }
    }

    EXPECT_NO_THROW(transport.SendFinish());

    max_times = 10;
    while (!transport.IsSocketReady()) {
        if (max_times-- <= 0) {
            std::cout << "while loop retry max times." << std::endl;
            break;
        }
    }

    EXPECT_NO_THROW(transport.RecvFinish());
}

TEST_F(UbMemTransportTest, UbMemTransport_read_write_read_reduce_write_reduce)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::DEV_NET), 0, 1, 0, 1);
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    StubUbRmaConnection stubRmaConnection(link);
    RmaConnection      *rmaConnection = &stubRmaConnection;
    locRes.connVec.push_back(rmaConnection);

    Stream stream;
    
    void *rdmaHandle = (void *)0x100;
    LocalUbRmaBuffer  localRmaBuffer(devBuf, rdmaHandle);
    RemoteUbRmaBuffer remoteRmaBuffer(rdmaHandle);;
    locSlice.buf = &localRmaBuffer;
    rmtSlice.buf = &remoteRmaBuffer;

    StubCommunicatorImplTransMgr comm;
    MemTransportCallback callback(link, comm.GetMirrorTaskManager());
    MOCKER_CPP(&DlProfFunc::isStubMode).stubs().will(returnValue(true));
 
    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes, callback);

	EXPECT_THROW(transport.Read(locSlice, rmtSlice, stream), NotSupportException);
	EXPECT_THROW(transport.Write(locSlice, rmtSlice, stream), NotSupportException);

    ReduceIn reduceIn(DataType::INT8, ReduceOp::MAX);
	EXPECT_THROW(transport.ReadReduce(locSlice, rmtSlice, reduceIn, stream), NotSupportException);
	EXPECT_THROW(transport.WriteReduce(locSlice, rmtSlice, reduceIn, stream), NotSupportException);
}

TEST_F(UbMemTransportTest, UbMemTransport_post_wait)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::DEV_NET), 0, 1, 0, 1);
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    StubUbRmaConnection stubRmaConnection(link);
    RmaConnection      *rmaConnection = &stubRmaConnection;
    locRes.connVec.push_back(rmaConnection);

    void *rdmaHandle = (void *)0x100;
    UbLocalNotify ubLocalNotify(rdmaHandle);
    BaseLocalNotify *validLocalNotify = &ubLocalNotify;
    locRes.notifyVec.push_back(validLocalNotify);

    Stream stream;

    StubCommunicatorImplTransMgr comm;
    MemTransportCallback callback(link, comm.GetMirrorTaskManager());
    MOCKER_CPP(&DlProfFunc::isStubMode).stubs().will(returnValue(true));
 
    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes, callback);

    std::unique_ptr<RemoteUbRmaBuffer> remoteUbRmaBuffer = std::make_unique<RemoteUbRmaBuffer>(rdmaHandle);
    transport.rmtNotifyVec.push_back(std::move(remoteUbRmaBuffer));
	EXPECT_THROW(transport.Post(0, stream), NotSupportException);
	transport.Wait(0, stream, 0);
}

TEST_F(UbMemTransportTest, UbMemTransport_write_with_notify_write_reduce_with_notify)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::DEV_NET), 0, 1, 0, 1);
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    StubUbRmaConnection stubRmaConnection(link);
    RmaConnection      *rmaConnection = &stubRmaConnection;
    locRes.connVec.push_back(rmaConnection);

    Stream stream;

    void             *rdmaHandle = (void *)0x100;
    LocalUbRmaBuffer  localRmaBuffer(devBuf, rdmaHandle);
    RemoteUbRmaBuffer remoteRmaBuffer(rdmaHandle);;
    locSlice.buf  = &localRmaBuffer;
    rmtSlice.buf  = &remoteRmaBuffer;
    locSlice.size = devBuf->GetSize();
    rmtSlice.size = devBuf->GetSize();

    RmaBufferSlice emptyLocSlice;
    emptyLocSlice.size = 0;

    StubCommunicatorImplTransMgr comm;
    MemTransportCallback callback(link, comm.GetMirrorTaskManager());
    MOCKER_CPP(&DlProfFunc::isStubMode).stubs().will(returnValue(true));
 
    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes, callback);

    std::unique_ptr<RemoteUbRmaBuffer> remoteUbRmaBuffer0 = std::make_unique<RemoteUbRmaBuffer>(rdmaHandle);
    std::unique_ptr<RemoteUbRmaBuffer> remoteUbRmaBuffer1 = std::make_unique<RemoteUbRmaBuffer>(rdmaHandle);
    transport.rmtNotifyVec.push_back(std::move(remoteUbRmaBuffer0));
    transport.rmtNotifyVec.push_back(std::move(remoteUbRmaBuffer1));

    constexpr uint32_t NOTIFY_INDEX_FIN = 1;
    WithNotifyIn withNotify(TransportNotifyType::INVALID, NOTIFY_INDEX_FIN);
    
    // normal notify
    withNotify.notifyType_ = TransportNotifyType::NORMAL;
	EXPECT_THROW(transport.WriteWithNotify(locSlice, rmtSlice, withNotify, stream), NotSupportException);
	// write empty case
	EXPECT_THROW(transport.WriteWithNotify(emptyLocSlice, rmtSlice, withNotify, stream), NotSupportException);

    ReduceIn reduceIn(DataType::INT8, ReduceOp::MAX);

    std::unique_ptr<RemoteUbRmaBuffer> remoteUbRmaBuffer2 = std::make_unique<RemoteUbRmaBuffer>(rdmaHandle);
    std::unique_ptr<RemoteUbRmaBuffer> remoteUbRmaBuffer3 = std::make_unique<RemoteUbRmaBuffer>(rdmaHandle);
    transport.rmtCntNotifyVec.push_back(std::move(remoteUbRmaBuffer2));
    transport.rmtCntNotifyVec.push_back(std::move(remoteUbRmaBuffer3));

	transport.WriteReduceWithNotify(locSlice, rmtSlice, reduceIn, withNotify, stream);
	// write empty case
	EXPECT_THROW(transport.WriteReduceWithNotify(emptyLocSlice, rmtSlice, reduceIn, withNotify, stream),
				 NotSupportException);

    // count notify
    withNotify.notifyType_ = TransportNotifyType::COUNT;
	EXPECT_THROW(transport.WriteWithNotify(locSlice, rmtSlice, withNotify, stream), NotSupportException);
	EXPECT_THROW(transport.WriteWithNotify(emptyLocSlice, rmtSlice, withNotify, stream), NotSupportException);

	transport.WriteReduceWithNotify(locSlice, rmtSlice, reduceIn, withNotify, stream);
	// write empty case
	EXPECT_THROW(transport.WriteReduceWithNotify(emptyLocSlice, rmtSlice, reduceIn, withNotify, stream),
				 NotSupportException);

    // invalid notify type
    withNotify.notifyType_ = TransportNotifyType::INVALID;
    EXPECT_THROW(transport.WriteWithNotify(locSlice, rmtSlice, withNotify, stream), InternalException);

    EXPECT_THROW(transport.WriteReduceWithNotify(locSlice, rmtSlice, reduceIn, withNotify, stream), InternalException);
}

TEST_F(UbMemTransportTest, UbMemTransport_wait)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::P2P), 0, 1, 0, 1);
    void                             *rdmaHandle = (void *)0x100;
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    StubUbRmaConnection  stubRmaConnection(link);
    RmaConnection       *rmaConnection    = &stubRmaConnection;
    locRes.connVec.push_back(rmaConnection);
    UbLocalNotify        ubLocalNotify(rdmaHandle);
    BaseLocalNotify     *validLocalNotify = &ubLocalNotify;
    locRes.notifyVec.push_back(validLocalNotify);
    LocalUbRmaBuffer     ubLocalRmaBuffer(devBuf, rdmaHandle);
    LocalRmaBuffer      *validLocalRmaBuffer = &ubLocalRmaBuffer;
    locRes.bufferVec.push_back(validLocalRmaBuffer);

    RtsCntNotify   rtsCntNotify;
    LocalCntNotify localCntNotify(rdmaHandle, &rtsCntNotify);
    locCntRes.vec.push_back(&localCntNotify);
    locCntRes.desc.push_back('0');
    locCntRes.desc.push_back(0);

    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes);
    transport.IsResReady();
}

TEST_F(UbMemTransportTest, UbMemTransport_ConnVecUnpackProc)
{
    BaseMemTransport::CommonLocRes    locRes;
    BaseMemTransport::Attribution     attr;
    BaseMemTransport::LocCntNotifyRes locCntRes;
    LinkData                          link(BasePortType(PortDeploymentType::DEV_NET), 0, 1, 0, 1);
    void                             *rdmaHandle = (void *)0x100;
    IpAddress                         ipAddress("1.0.0.0");
    Socket                            fakeSocket(nullptr, ipAddress, 100, ipAddress, "tag", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);
    StubUbRmaConnection  stubRmaConnection(link);
    RmaConnection       *rmaConnection    = &stubRmaConnection;
    locRes.connVec.push_back(rmaConnection);

    UbMemTransport transport(locRes, attr, link, fakeSocket, rdmaHandle, locCntRes);
    u32 connNum = 1;
    transport.connNum = connNum;

    BinaryStream binaryStream;
    u32 rmtConnNum = connNum;
    binaryStream << rmtConnNum;
    u32 pos = 1;
    binaryStream << pos;

    ExchangeUbConnDto dto;
    dto.Serialize(binaryStream);
    EXPECT_NO_THROW(transport.ConnVecUnpackProc(binaryStream));
}