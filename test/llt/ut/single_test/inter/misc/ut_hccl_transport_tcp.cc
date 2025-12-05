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
#include <stdio.h>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#include "sal.h"

#define private public
#include "dispatcher_pub.h"
#include "transport_tcp_pub.h"
#undef private

#include "llt_hccl_stub_pub.h"
#include "profiler_manager.h"



using namespace std;
using namespace hccl;

class LinkTcpTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--CommBaseTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--CommBaseTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;

};
HcclDispatcher LinkTcpTest::dispatcherPtr = nullptr;
DispatcherPub *LinkTcpTest::dispatcher = nullptr;

class LinkTcpExpTmp : public TransportTcp
{
public:
    explicit LinkTcpExpTmp(HcclDispatcher dispatcher,
                        MachinePara& machine_para, std::chrono::milliseconds timeout);
    virtual ~LinkTcpExpTmp();
};

LinkTcpExpTmp::LinkTcpExpTmp(HcclDispatcher dispatcher,
                        MachinePara& machine_para, std::chrono::milliseconds timeout)
    : TransportTcp(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machine_para, timeout)
{

}

LinkTcpExpTmp::~LinkTcpExpTmp()
{

}

#if 1
TEST_F(LinkTcpTest, ut_link_base_test)
{
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);
    MachinePara machinePara;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<Transport> link_base(new Transport(new (std::nothrow) TransportTcp(
        dispatcher, nullptr, machinePara, timeout)));

    link_base->TxAck(stream);
    link_base->RxAck(stream);
    link_base->TxDataSignal(stream);
    link_base->RxDataSignal(stream);
}
TEST_F(LinkTcpTest, ut_link_base_helper_tcp_test)
{
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);
    MachinePara machinePara;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<TransportTcp> link_base(new TransportTcp(dispatcher, nullptr, machinePara, timeout));

    link_base->nicDeploy_ = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    MOCKER_CPP(&DispatcherPub::HostNicTcpSend)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherPub::HostNicTcpRecv)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    link_base->TxAsync(UserMemType::INPUT_MEM, 0, &(link_base->nicDeploy_), 100, stream);
    link_base->RxAsync(UserMemType::OUTPUT_MEM, 0, &(link_base->nicDeploy_), 100, stream);
    GlobalMockObject::verify();
}

TEST_F(LinkTcpTest, ut_linktcp_for_batchsendrecv)
{
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);
    MachinePara machinePara;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<TransportTcp> link_base(new TransportTcp(dispatcher, nullptr, machinePara, timeout));

    link_base->nicDeploy_ = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    MOCKER_CPP(&DispatcherPub::HostNicTcpSend)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherPub::HostNicTcpRecv)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    link_base->TxPrepare(stream);
    link_base->RxPrepare(stream);
    link_base->TxData(UserMemType::INPUT_MEM, 0, &(link_base->nicDeploy_), 100, stream);
    link_base->RxData(UserMemType::OUTPUT_MEM, 0, &(link_base->nicDeploy_), 100, stream);
    link_base->TxDone(stream);
    link_base->RxDone(stream);
    GlobalMockObject::verify();
}
#endif

#if 1
TEST_F(LinkTcpTest, ut_link_base_helper_tcp_0_test)
{
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    s32 mem_size = 256;
    DeviceMem mem = DeviceMem::alloc(mem_size);
    MachinePara machinePara;
    HcclIpAddress remoteIp{};
    HcclIpAddress localIp{};
    std::shared_ptr<HcclSocket> newSocket(new (std::nothrow)HcclSocket("test", 
        nullptr, remoteIp, 0, HcclSocketRole::SOCKET_ROLE_SERVER));
    machinePara.sockets.push_back(newSocket);

    std::chrono::milliseconds timeout;
    const std::string tag;

    std::shared_ptr<TransportTcp> link_base(new TransportTcp(dispatcher, nullptr, machinePara, timeout));

    link_base->nicDeploy_ = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    MOCKER_CPP(&DispatcherPub::HostNicTcpSend)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherPub::HostNicTcpRecv)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    link_base->TxAsync(UserMemType::INPUT_MEM, 0, &(link_base->nicDeploy_), 0, stream);
    link_base->RxAsync(UserMemType::OUTPUT_MEM, 0, &(link_base->nicDeploy_), 0, stream);
    GlobalMockObject::verify();
}
#endif