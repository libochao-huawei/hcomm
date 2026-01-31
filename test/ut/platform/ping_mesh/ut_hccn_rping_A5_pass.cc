/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <thread>
#include <chrono>
#include <securec.h>
#include <stdio.h>

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ping_mesh.h"
#include "hccn_rping.h"
#include "hccl_ip_address.h"
#include "adapter_rts.h"
#ifdef CONFIG_CONTEXT
#include "hccp_ctx.h"
#endif
#include "local_ub_rma_buffer.h"
#include "orion_adapter_hccp.h"

using namespace hccl;

HcclResult HccnRpingGetResultStub(PingMesh *obj, u32 deviceId, u32 targetNum, RpingInput *input, RpingOutput *output)
{
    for (int i = 0; i < targetNum; i++) {
        output[i].state = 2;
        output[i].txPkt = 10;
        output[i].rxPkt = 10;
        output[i].minRTT = 5;
        output[i].maxRTT = 15;
        output[i].avgRTT = 10;
    }
    return HCCL_SUCCESS;
}

inline HcclResult hrtGetDeviceRefreshStub(s32 *deviceLogicId)
{
    *deviceLogicId = 1;
    return HCCL_SUCCESS;
}

class HccnRping_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccnRping_UT SetUP" << std::endl;
    }
 
    static void TearDownTestCase()
    {
        std::cout << "HccnRping_UT TearDown" << std::endl;
    }
 
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MOCKER_CPP(&PingMesh::HccnRpingInit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingDeinit)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingAddTarget)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingRemoveTarget)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingGetTarget)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingBatchPingStart)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingBatchPingStop)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER_CPP(&PingMesh::HccnRpingGetResult)
        .stubs()
        .with(any())
        .will(invoke(HccnRpingGetResultStub));

        MOCKER_CPP(&PingMesh::HccnRpingGetPayload)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtGetDeviceRefresh)
        .stubs()
        .with(any())
        .will(invoke(hrtGetDeviceRefreshStub));

        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "HccnRping_UT Test SetUP" << std::endl;
    }
 
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "HccnRping_UT Test TearDown" << std::endl;
    }

public:
    char deviceIp_[8][11] = {
        "192.1.1.58",
        "192.1.2.58",
        "192.1.3.58",
        "192.1.4.58",
        "192.1.1.59",
        "192.1.2.59",
        "192.1.3.59",
        "192.1.4.59"
    };
        //填对应eid
    char device_eid[8][33] = {
        "000000000000002000100000dfdf1701",
        "000000000000002000100000dfdf1702",
        "000000000000002000100000dfdf1703",
        "000000000000002000100000dfdf1704",
        "000000000000002000100000dfdf1705",
        "000000000000002000100000dfdf1706",
        "000000000000002000100000dfdf1707",
        "000000000000002000100000dfdf1708"
    };

    int ipLen_ = 32;
};

//A5_UT测试
TEST_F(HccnRping_UT, ut_HccnRpingInit)
{
    MOCKER_CPP(&PingMesh::GetDeviceLogicId)
    .stubs()
    .with(any())
    .will(returnValue(1));

    u32 devId = 1;
    u32 devserverId = 2;
    HccnRpingInitAttr *initAttr = new HccnRpingInitAttr();
    initAttr->mode = HCCN_RPING_MODE_UB;
    initAttr->port = 13886;
    initAttr->npuNum = 128;
    initAttr->bufferSize = 4096 * 10;
    initAttr->sl = 0;
    initAttr->tc = 0;
    initAttr->eid = new char[ipLen_];
    strcpy(initAttr->eid, device_eid[devId]);
    void *pingmesh = nullptr;

    // 初始化
    auto ret = HccnRpingInit(devId, initAttr, &pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);
    EXPECT_TRUE(pingmesh != nullptr);
    delete[] initAttr->eid;
    delete initAttr;


    // 反初始化
    ret = HccnRpingDeinit(pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);

}

TEST_F(HccnRping_UT, ut_HccnRpingInit_error)
{
    MOCKER_CPP(&PingMesh::GetDeviceLogicId)
    .stubs()
    .with(any())
    .will(returnValue(1));

    u32 devId = 1;
    u32 devserverId = 2;
    HccnRpingInitAttr *initAttr = nullptr;
    void *pingmesh = nullptr;
    // 空initAttr初始化错误检查
    auto ret = HccnRpingInit(devId, initAttr, &pingmesh);
    EXPECT_EQ(ret, HCCN_E_PARA);

    initAttr = new HccnRpingInitAttr();
    initAttr->mode = HCCN_RPING_MODE_UB;
    initAttr->port = 13886;
    initAttr->npuNum = 128;
    initAttr->bufferSize = 4096 * 10;
    initAttr->sl = 0;
    initAttr->tc = 0;
    initAttr->eid = new char[ipLen_];
    strcpy(initAttr->eid, device_eid[devId]);
    
    
    ret = HccnRpingInit(devId, initAttr, &pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);
    EXPECT_TRUE(pingmesh != nullptr);
    delete[] initAttr->eid;
    delete initAttr;

}


TEST_F(HccnRping_UT, ut_HccnRping)
{
    MOCKER_CPP(&PingMesh::GetDeviceLogicId)
    .stubs()
    .with(any())
    .will(returnValue(1));

    u32 devId = 1;
    u32 devserverId = 2;
    HccnRpingInitAttr *initAttr = new HccnRpingInitAttr();
    initAttr->mode = HCCN_RPING_MODE_UB;
    initAttr->port = 13886;
    initAttr->npuNum = 128;
    initAttr->bufferSize = 4096 * 10;
    initAttr->sl = 0;
    initAttr->tc = 0;
    initAttr->eid = new char[ipLen_];
    strcpy(initAttr->eid, device_eid[devId]);
    void *pingmesh = nullptr;

    // 初始化
    auto ret = HccnRpingInit(devId, initAttr, &pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);
    EXPECT_TRUE(pingmesh != nullptr);
    delete[] initAttr->eid;
    delete initAttr;

    // 添加节点
    int targetNum = 1;
    HccnRpingTargetInfo *target = new HccnRpingTargetInfo[1];
    target[0].srcPort = 0;
    target[0].sl = 0;
    target[0].tc = 0;
    target[0].port = 13886;
    target[0].payloadLen = 12;
    target[0].addrType = 1;
    target[0].srcEid = new char[ipLen_];
    target[0].dstEid = new char[ipLen_];
    char payload[12] = "hellotarget";
    strcpy(target[0].payload, payload);
    strcpy(target[0].srcEid, device_eid[devserverId]);
    strcpy(target[0].dstEid, device_eid[devId]);
    ret = HccnRpingAddTarget(pingmesh, targetNum, target);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 获取节点
    HccnRpingAddTargetState targetState[1] {HCCN_RPING_ADDTARGET_STATE_DONE};
    ret = HccnRpingGetTarget(pingmesh, targetNum, target, targetState);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 开始批量测试
    uint32_t pktNum = 10;
    uint32_t interval = 1;
    uint32_t timeout = 1000;
    ret = HccnRpingBatchPingStart(pingmesh, pktNum, interval, timeout);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 停止批量测试
    ret = HccnRpingBatchPingStop(pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 获取结果
    HccnRpingResultInfo result[1] {0};
    ret = HccnRpingGetResult(pingmesh, targetNum, target, result);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 获取payload
    void *payloadOutput = nullptr;
    u32 payloadLenOutput = 0;
    ret = HccnRpingGetPayload(pingmesh, &payloadOutput, &payloadLenOutput);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 删除节点
    ret = HccnRpingRemoveTarget(pingmesh, targetNum, target);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 反初始化
    ret = HccnRpingDeinit(pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    delete[] target[0].srcEid;
    delete[] target[0].dstEid;
    delete[] target;
}

TEST_F(HccnRping_UT, ut_HccnRping_RpingAddTargetV2)
{
    MOCKER_CPP(&PingMesh::GetDeviceLogicId)
    .stubs()
    .with(any())
    .will(returnValue(1));

    u32 devId = 1;
    u32 devserverId = 2;
    HccnRpingInitAttr *initAttr = new HccnRpingInitAttr();
    initAttr->mode = HCCN_RPING_MODE_UB;
    initAttr->port = 13886;
    initAttr->npuNum = 128;
    initAttr->bufferSize = 4096 * 10;
    initAttr->sl = 0;
    initAttr->tc = 0;
    initAttr->eid = new char[ipLen_];
    strcpy(initAttr->eid, device_eid[devId]);
    void *pingmesh = nullptr;

    // 初始化
    auto ret = HccnRpingInit(devId, initAttr, &pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);
    EXPECT_TRUE(pingmesh != nullptr);
    delete[] initAttr->eid;
    delete initAttr;

    // 添加节点
    int targetNum = 1;
    HccnRpingTargetInfo *target = new HccnRpingTargetInfo[1];
    target[0].srcPort = 0;
    target[0].sl = 0;
    target[0].tc = 0;
    target[0].port = 13886;
    target[0].payloadLen = 12;
    target[0].addrType = 1;
    target[0].srcEid = new char[ipLen_];
    target[0].dstEid = new char[ipLen_];
    char payload[12] = "hellotarget";
    strcpy(target[0].payload, payload);
    strcpy(target[0].srcEid, device_eid[devserverId]);
    strcpy(target[0].dstEid, device_eid[devId]);
    
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 1000;
    ret = HccnRpingAddTargetV2(pingmesh, targetNum, target, &config);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 删除节点
    ret = HccnRpingRemoveTarget(pingmesh, targetNum, target);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    // 反初始化
    ret = HccnRpingDeinit(pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);

    delete[] target[0].srcEid;
    delete[] target[0].dstEid;
    delete[] target;
}

TEST_F(HccnRping_UT, ut_HccnRping_RpingAddTargetV2_err)
{
    HccnRpingAddTargetConfig config;
    config.connectTimeout = 0;
    auto ret = HccnRpingAddTargetV2(nullptr, 1, nullptr, &config);
    EXPECT_EQ(ret, HCCN_E_PARA);
}

TEST_F(HccnRping_UT, ut_HccnRpingHandleCheck)
{
 
    u32 devId = 3;
    u32 devserverId = 2;
    HccnRpingInitAttr *initAttr = new HccnRpingInitAttr();
    initAttr->mode = HCCN_RPING_MODE_UB;
    initAttr->port = 13886;
    initAttr->npuNum = 128;
    initAttr->bufferSize = 4096 * 10;
    initAttr->sl = 0;
    initAttr->tc = 0;   
    initAttr->eid = new char[ipLen_];
    strcpy(initAttr->eid, device_eid[devId]);
    void *pingmesh = nullptr;

    // 初始化
    auto ret = HccnRpingInit(1, initAttr, &pingmesh);
    EXPECT_EQ(ret, HCCN_SUCCESS);
    EXPECT_TRUE(pingmesh != nullptr);
    delete[] initAttr->eid;
    delete initAttr;

    // 添加节点
    int targetNum = 1;
    HccnRpingTargetInfo *target = new HccnRpingTargetInfo[1];
    target[0].srcPort = 0;
    target[0].sl = 0;
    target[0].tc = 0;
    target[0].port = 13886;
    target[0].payloadLen = 12;
    target[0].addrType = 1;
    target[0].srcEid = new char[ipLen_];
    target[0].dstEid = new char[ipLen_];
    char payload[12] = "hellotarget";
    strcpy(target[0].payload, payload);
    strcpy(target[0].srcEid, device_eid[devserverId]);
    strcpy(target[0].dstEid, device_eid[devId]);
    ret = HccnRpingAddTarget(pingmesh, targetNum, target);
    EXPECT_EQ(ret, HCCN_E_PARA);

    // 获取节点
    HccnRpingAddTargetState targetState[1] {HCCN_RPING_ADDTARGET_STATE_DONE};
    ret = HccnRpingGetTarget(pingmesh, targetNum, target, targetState);
    EXPECT_EQ(ret, HCCN_E_PARA);

    // 开始批量测试
    uint32_t pktNum = 10;
    uint32_t interval = 1;
    uint32_t timeout = 1000;
    ret = HccnRpingBatchPingStart(pingmesh, pktNum, interval, timeout);
    EXPECT_EQ(ret, HCCN_E_PARA);

    // 停止批量测试
    ret = HccnRpingBatchPingStop(pingmesh);
    EXPECT_EQ(ret, HCCN_E_PARA);

    // 获取结果
    HccnRpingResultInfo result[1] {0};
    ret = HccnRpingGetResult(pingmesh, targetNum, target, result);
    EXPECT_EQ(ret, HCCN_E_PARA);

    // 删除节点
    ret = HccnRpingRemoveTarget(pingmesh, targetNum, target);
    EXPECT_EQ(ret, HCCN_E_PARA);

    // 反初始化
    ret = HccnRpingDeinit(pingmesh);
    EXPECT_EQ(ret, HCCN_E_PARA);

    delete[] target[0].srcEid;
    delete[] target[0].dstEid;
    delete[] target;
    delete static_cast<PingMesh*>(pingmesh);
}
 
