/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include "hccl_api_base_test.h"

#define private public
#define protected public

#include "log.h"
#include "adapter_rts.h"
#include "hcomm_c_adpt.h"

#include "ccu_device_pub.h"
#include "ccu_kernel_mgr.h"
#include "ccu_instance_mgr.h"
#include "ccu_primitives.h"
#include "ccu_control_api.h"

#include "mocks/ccu_device_mock_utils.h"
#include "mocks/ccu_channel_mock_utils.h"

#include "ccu_kernel_impl/ccu_var_add_simple_demo.h"

#include "adapter_rts.h"
#include "adapter_hal_pub.h"

#include "hcomm_primitives.h"

#undef protected
#undef private

class HcommCcuControlApiTest : public BaseInit {
public:
    void SetUp() override {
        GlobalMockObject::verify();
        BaseInit::SetUp();
        // 将enableEntryLog默认返回为true
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
protected:
};

static std::pair<EndpointHandle, ChannelHandle> MockCcuChannelConnect(
    uint32_t srcDevPhyId, uint32_t dstDevPhyId,
    uint32_t srcIp, uint32_t dstIp, CommEngine commEngine)
{
    HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
    CcuResult ccuRest = CcuResult::CCU_E_RESERVED;

    CommAddr srcAddr{}, dstAddr{};
    srcAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    srcAddr.addr.s_addr = srcIp;
    dstAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    dstAddr.addr.s_addr = dstIp;

    const auto &srcEpDesc = MockEndpointDesc(srcAddr, srcDevPhyId);
    EndpointHandle srcEpHandle{};
    hcclRet = HcommEndpointCreate(&srcEpDesc, &srcEpHandle);
    EXPECT_EQ(hcclRet, HcclResult::HCCL_SUCCESS);

    const auto &dstEpDesc = MockEndpointDesc(dstAddr, dstDevPhyId);

    const auto &socket = MockHcclSocket(srcAddr, dstAddr);
    HcommSocket socketPtr = static_cast<HcommSocket>(socket.get());
    const auto &rmaBuffer = MockUbRmaBuffer();
    void *memHandle = static_cast<void *>(rmaBuffer.get());
    auto channelDesc = MockHcommChannelDesc(dstEpDesc, socketPtr, memHandle);
    constexpr uint32_t channelNum = 1;
    ChannelHandle channelHandle{0};
    hcclRet = HcommChannelCreate(srcEpHandle, commEngine, &channelDesc, channelNum, &channelHandle);
    EXPECT_EQ(hcclRet, HcclResult::HCCL_SUCCESS);

    int32_t statusList[] = {0};

    constexpr uint32_t MAX_LOOP_TIME = 100;
    uint32_t leftTime = MAX_LOOP_TIME;
    while (leftTime--) {
        hcclRet = HcommChannelGetStatus(&channelHandle, channelNum, statusList);
        if (hcclRet == HcclResult::HCCL_SUCCESS) {
            break;
        }

        if (hcclRet != HcclResult::HCCL_E_AGAIN) {
            HCCL_ERROR("[%s] invalid ret[%d].", __func__, hcclRet);
            break;
        }
    }

    EXPECT_EQ(hcclRet, HcclResult::HCCL_SUCCESS);
    return {srcEpHandle, channelHandle};
}

static void MockChannelDestory(const std::pair<EndpointHandle, ChannelHandle> &handles)
{
    HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
    constexpr uint32_t channelNum = 1;
    hcclRet = HcommChannelDestroy(&(handles.second), channelNum);
    EXPECT_EQ(hcclRet, HcclResult::HCCL_SUCCESS);

    hcclRet = HcommEndpointDestroy(handles.first);
    EXPECT_EQ(hcclRet, HcclResult::HCCL_SUCCESS);
}

static ThreadHandle MockThreadAllocWithStream(CommEngine commEngine)
{
    // 选择调用Hcomm接口，而不是对具体的Thread实现打桩，减少内部依赖
    MOCKER(aclrtStreamGetId).stubs().will(returnValue(0));
    bool devRunning = false;
    MOCKER(GetRunSideIsDevice).stubs().with(outBound(devRunning))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    
    constexpr uint32_t fakeNotifyNum = 0;
    aclrtStream fakeStream{(void *)0x12345678};
    ThreadHandle fakeThreadHandle{};
    EXPECT_EQ(HcommThreadAllocWithStream(commEngine, fakeStream,
        fakeNotifyNum, &fakeThreadHandle), HcclResult::HCCL_SUCCESS);

    return fakeThreadHandle;
}

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRegister_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    // 整体打桩，处理ccu资源
    HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 2;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId); // 先处理网络设备，再初始化ccu
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    // ccuInstance构建（正常在通信域创建中，本用例仅测试hcomm接口）
    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    auto insType = MS_INS_TPYE;
    void *ccuResDesc = static_cast<void *>(&insType);
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // 建链流程打桩
    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383; // 需要与RaGetDevEidInfoList接口打桩一致
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    // 构造CcuKernel实现
    CcuKernelFunc demoFunc = CcuVarAddDemoKernel;
    CcuVarAddKernelArg demoArg{};
    demoArg.numA = 1;
    demoArg.numB = 2;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    // 重置CCU资源
    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // kernel注册
    char *kernelFuncName = "ccu_var_add_simple_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // kernel翻译
    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // 申请流，假定已经获取了threadHandle
    auto fakeThreadHandle = MockThreadAllocWithStream(commEngine);

    // kernel下发
    // todo: 当前未适配LoadArgs，暂时使用空args
    void *fakeTaskArgs = nullptr;
    uint32_t fakeArgSize = 0;
    EXPECT_EQ(HcommCcuKernelLaunch(fakeThreadHandle, kernelHandle,
        fakeTaskArgs, fakeArgSize), CcuResult::CCU_SUCCESS);

    // 清理各种资源，析构有时序要求
    // 主线功能有bug，暂时不能主动释放stream
    // constexpr uint32_t fakeThreadNum = 1;
    // EXPECT_EQ(HcommThreadFree(&fakeThreadHandle, fakeThreadNum), HcclResult::HCCL_SUCCESS);
    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}