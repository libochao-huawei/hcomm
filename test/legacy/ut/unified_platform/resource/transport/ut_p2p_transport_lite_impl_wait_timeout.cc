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
#include <mockcpp/MockObject.h>
#define private public
#include "virtual_topo.h"
#include "dev_ub_connection.h"
#include "task.h"
#include "p2p_transport_lite_impl.h"
#include "mem_transport_lite.h"
#include "socket.h"
#include "rtsq_a5.h"
#include "ascend_hal.h"
#include "drv_api_exception.h"
#include "ub_conn_lite_mgr.h"
#include "ins_to_sqe_rule.h"
#include "stream_lite.h"
#include "mem_transport_callback.h"
#undef private

using namespace Hccl;

class P2PTransportLiteImplWaitTimeoutTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "P2PTransportLiteImplWaitTimeoutTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "P2PTransportLiteImplWaitTimeoutTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        MOCKER(HrtGetDeviceType).stubs().will(returnValue((DevType)DevType::DEV_TYPE_910A2));
        MOCKER_CPP(&RtsqBase::QuerySqBaseAddr).stubs().with(any()).will(returnValue(reinterpret_cast<u64>(&mockSq)));
        MOCKER_CPP(&RtsqBase::QuerySqStatusByType).stubs().with(any()).will(returnValue(static_cast<u32>(0)));
        MOCKER_CPP(&RtsqBase::ConfigSqStatusByType).stubs();
        std::cout << "A Test case in P2PTransportLiteImplWaitTimeoutTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in P2PTransportLiteImplWaitTimeoutTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    std::vector<char> GetNotifyUniqueId(u32 notifyId, u32 devPhyId)
    {
        BinaryStream binaryStream;
        binaryStream << notifyId;
        binaryStream << devPhyId;
        std::vector<char> result;
        binaryStream.Dump(result);
        return result;
    }

    std::vector<char> GetRmtBufferUniqueId(u64 addr, u64 size, u32 id)
    {
        BinaryStream binaryStream;
        binaryStream << addr;
        binaryStream << size;
        binaryStream << id;
        std::vector<char> result;
        binaryStream.Dump(result);
        return result;
    }

    std::vector<char> BuildP2PTransportLiteUniqueId()
    {
        auto locNotify0 = GetNotifyUniqueId(1, 1);
        auto locNotify1 = GetNotifyUniqueId(2, 2);
        auto rmtNotify0 = GetRmtBufferUniqueId(1, 1, 1);
        auto rmtNotify1 = GetRmtBufferUniqueId(2, 2, 2);

        u32 notifyNum = 2;

        auto rmtBuffer0 = GetRmtBufferUniqueId(300, 200, 3);
        auto rmtBuffer1 = GetRmtBufferUniqueId(300, 200, 4);
        u32 buffeNum = 2;

        BinaryStream binaryStream;

        u32 type = (u32)TransportType::P2P;
        binaryStream << type;
        binaryStream << notifyNum;
        binaryStream << buffeNum;

        std::vector<char> data0;
        data0.insert(data0.end(), locNotify0.begin(), locNotify0.end());
        data0.insert(data0.end(), locNotify1.begin(), locNotify1.end());
        binaryStream << data0;

        std::vector<char> data1;
        data1.insert(data1.end(), rmtNotify0.begin(), rmtNotify0.end());
        data1.insert(data1.end(), rmtNotify1.begin(), rmtNotify1.end());
        binaryStream << data1;

        std::vector<char> data2;
        data2.insert(data2.end(), rmtBuffer0.begin(), rmtBuffer0.end());
        data2.insert(data2.end(), rmtBuffer1.begin(), rmtBuffer1.end());
        binaryStream << data2;

        std::vector<char> liteData;
        binaryStream.Dump(liteData);
        return liteData;
    }

    u8  mockSq[AC_SQE_SIZE * AC_SQE_MAX_CNT]{0};
};

TEST_F(P2PTransportLiteImplWaitTimeoutTest, Wait_WithSpecificTimeout_Expect_Success)
{
    std::vector<char> liteData = BuildP2PTransportLiteUniqueId();

    RmaConnLite rmaConnLite;
    RmaConnLite *connLite = &rmaConnLite;
    MOCKER_CPP(&UbConnLiteMgr::Get).stubs().will(returnValue(connLite));
    MOCKER_CPP(&MirrorTaskManager::AddTaskInfo).stubs().with(any());
    LinkData linkData(BasePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB), 0, 1, 0, 1);
    MirrorTaskManager mirrorTaskMgr(0, &GlobalMirrorTasks::Instance(), true);
    auto transportCallback = MemTransportCallback(linkData, mirrorTaskMgr);
    MemTransportLite transportLite(liteData, transportCallback);
    auto *p2pTransportLite = dynamic_cast<P2PTransportLiteImpl *>(transportLite.impl.get());

    u32 fakeStreamId = 1;
    u32 fakeSqId = 1;
    u32 fakeDevPhyId = 1;
    BinaryStream liteBinaryStream;
    liteBinaryStream << fakeStreamId;
    liteBinaryStream << fakeSqId;
    liteBinaryStream << fakeDevPhyId;
    std::vector<char> streamUniqueId{};
    liteBinaryStream.Dump(streamUniqueId);

    StreamLite stream(streamUniqueId);
    RtsqA5 rtsq(fakeDevPhyId, fakeStreamId, fakeSqId);
    stream.rtsq = std::make_unique<RtsqA5>(rtsq);

    MOCKER_CPP_VIRTUAL(rtsq, &RtsqA5::SdmaCopy).stubs().with(any(), any(), any(), any());
    MOCKER_CPP(&P2PTransportLiteImpl::BuildNotifyWaitTask).stubs();

    u32 timeout = 1800;
    EXPECT_NO_THROW(p2pTransportLite->Wait(0, stream, timeout));
}

TEST_F(P2PTransportLiteImplWaitTimeoutTest, Wait_WithZeroTimeout_Expect_Success)
{
    std::vector<char> liteData = BuildP2PTransportLiteUniqueId();

    RmaConnLite rmaConnLite;
    RmaConnLite *connLite = &rmaConnLite;
    MOCKER_CPP(&UbConnLiteMgr::Get).stubs().will(returnValue(connLite));
    MOCKER_CPP(&MirrorTaskManager::AddTaskInfo).stubs().with(any());
    LinkData linkData(BasePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB), 0, 1, 0, 1);
    MirrorTaskManager mirrorTaskMgr(0, &GlobalMirrorTasks::Instance(), true);
    auto transportCallback = MemTransportCallback(linkData, mirrorTaskMgr);
    MemTransportLite transportLite(liteData, transportCallback);
    auto *p2pTransportLite = dynamic_cast<P2PTransportLiteImpl *>(transportLite.impl.get());

    u32 fakeStreamId = 1;
    u32 fakeSqId = 1;
    u32 fakeDevPhyId = 1;
    BinaryStream liteBinaryStream;
    liteBinaryStream << fakeStreamId;
    liteBinaryStream << fakeSqId;
    liteBinaryStream << fakeDevPhyId;
    std::vector<char> streamUniqueId{};
    liteBinaryStream.Dump(streamUniqueId);

    StreamLite stream(streamUniqueId);
    RtsqA5 rtsq(fakeDevPhyId, fakeStreamId, fakeSqId);
    stream.rtsq = std::make_unique<RtsqA5>(rtsq);

    MOCKER_CPP_VIRTUAL(rtsq, &RtsqA5::SdmaCopy).stubs().with(any(), any(), any(), any());
    MOCKER_CPP(&P2PTransportLiteImpl::BuildNotifyWaitTask).stubs();

    u32 timeout = 0;
    EXPECT_NO_THROW(p2pTransportLite->Wait(0, stream, timeout));
}
