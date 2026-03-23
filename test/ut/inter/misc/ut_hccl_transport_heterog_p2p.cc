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

#include "dispatcher_pub.h"
#include "transport_heterog_p2p_pub.h"

#include "llt_hccl_stub_pub.h"
#include "profiler_manager.h"



using namespace std;
using namespace hccl;

class TransportHeterogP2PTest : public testing::Test
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
HcclDispatcher TransportHeterogP2PTest::dispatcherPtr = nullptr;
DispatcherPub *TransportHeterogP2PTest::dispatcher = nullptr;


TEST_F(TransportHeterogP2PTest, ut_function_for_batchsendrecv_HeterogP2P)
{
    std::string collectiveId = "test_collective";

    s32 device_id = 0;

    MachinePara machine_para;
    machine_para.deviceLogicId = device_id;
    machine_para.supportDataReceivedAck = true;

    std::shared_ptr<TransportBase> linktmp = nullptr;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(10);
    linktmp.reset(new TransportHeterogP2P(dispatcher, nullptr, machine_para, timeout));

    Stream streamObj(StreamType::STREAM_TYPE_OFFLINE);
    HcclResult ret = linktmp->TxPrepare(streamObj);

    ret = linktmp->RxPrepare(streamObj);

    ret = linktmp->TxData(UserMemType::INPUT_MEM, 0, nullptr, 0, streamObj);

    ret = linktmp->RxData(UserMemType::INPUT_MEM, 0, nullptr, 0, streamObj);

    ret = linktmp->TxDone(streamObj);

    ret = linktmp->RxDone(streamObj);

    ret = linktmp->PostFinAck(streamObj);

    ret = linktmp->WaitFinAck(streamObj);

    GlobalMockObject::verify();
}