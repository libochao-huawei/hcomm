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

#include "adapter_rts.h"
#include "adapter_hal_pub.h"

#include "hcomm_primitives.h"

#include "ccu_kernel_impl/ccu_var_add_simple_demo.h"
#include "ccu_kernel_impl/ccu_loop_add_demo.h"
#include "ccu_kernel_impl/ccu_jump_demo.h"
#include "ccu_kernel_impl/ccu_reduce_scatter_mesh1d_demo.h"

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
    HcommResult hcommRet = 0;
    CcuResult ccuRest = CcuResult::CCU_E_RESERVED;

    CommAddr srcAddr{}, dstAddr{};
    srcAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    srcAddr.addr.s_addr = srcIp;
    dstAddr.type = CommAddrType::COMM_ADDR_TYPE_IP_V4;
    dstAddr.addr.s_addr = dstIp;

    const auto &srcEpDesc = MockEndpointDesc(srcAddr, srcDevPhyId);
    EndpointHandle srcEpHandle{};
    hcommRet = HcommEndpointCreate(&srcEpDesc, &srcEpHandle);
    EXPECT_EQ(hcommRet, static_cast<HcommResult>(HcclResult::HCCL_SUCCESS));

    const auto &dstEpDesc = MockEndpointDesc(dstAddr, dstDevPhyId);

    const auto &socket = MockHcclSocket(srcAddr, dstAddr);
    HcommSocket socketPtr = static_cast<HcommSocket>(socket.get());
    const auto &rmaBuffer = MockUbRmaBuffer();
    auto commMemInfo = MockCommMemInfo(rmaBuffer.get());
    void *memHandle = static_cast<void *>(&commMemInfo);
    auto channelDesc = MockHcommChannelDesc(dstEpDesc, socketPtr, memHandle);
    constexpr uint32_t channelNum = 1;
    ChannelHandle channelHandle{0};
    hcommRet = HcommChannelCreate(srcEpHandle, commEngine, &channelDesc, channelNum, &channelHandle);
    EXPECT_EQ(hcommRet, static_cast<HcommResult>(HcclResult::HCCL_SUCCESS));

    int32_t statusList[] = {0};

    constexpr uint32_t MAX_LOOP_TIME = 100;
    uint32_t leftTime = MAX_LOOP_TIME;
    while (leftTime--) {
        hcommRet = HcommChannelGetStatus(&channelHandle, channelNum, statusList);
        if (hcommRet == static_cast<HcommResult>(HcclResult::HCCL_SUCCESS)) {
            break;
        }

        if (hcommRet != static_cast<HcommResult>(HcclResult::HCCL_E_AGAIN)) {
            HCCL_ERROR("[%s] invalid ret[%d].", __func__, hcommRet);
            break;
        }
    }

    EXPECT_EQ(hcommRet, static_cast<HcommResult>(HcclResult::HCCL_SUCCESS));
    return {srcEpHandle, channelHandle};
}

static void MockChannelDestory(const std::pair<EndpointHandle, ChannelHandle> &handles)
{
    HcommResult hcommRet = 0;
    constexpr uint32_t channelNum = 1;
    hcommRet = HcommChannelDestroy(&(handles.second), channelNum);
    EXPECT_EQ(hcommRet, static_cast<HcommResult>(HcclResult::HCCL_SUCCESS));

    hcommRet = HcommEndpointDestroy(handles.first);
    EXPECT_EQ(hcommRet, static_cast<HcommResult>(HcclResult::HCCL_SUCCESS));
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

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRegister_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     // 整体打桩，处理ccu资源
//     HcommResult hcclRet = 0;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 2;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs()
//         .with(outBoundP(&fakeDeviceLogicId))
//         .will(returnValue(HcclResult::HCCL_SUCCESS));
//     MOCKER(hrtGetDevicePhyIdByIndex).stubs()
//         .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
//         .will(returnValue(HcclResult::HCCL_SUCCESS));
//     constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
//     MockCcuNetworkDeviceDefault(fakeDeviceLogicId); // 先处理网络设备，再初始化ccu
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     // ccuInstance构建（正常在通信域创建中，本用例仅测试hcomm接口）
//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     // 建链流程打桩
//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383; // 需要与RaGetDevEidInfoList接口打桩一致
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     // 构造CcuKernel实现
//     CcuKernelFunc demoFunc = CcuAllocDemoKernel;
//     CcuVarAddKernelArg demoArg{};
//     demoArg.numA = 1;
//     demoArg.numB = 2;
//     demoArg.channelHandle = handlePair.second;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     // 重置CCU资源
//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     // kernel注册
//     char *kernelFuncName = "ccu_var_add_simple_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     // kernel翻译
//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
    
//     // 申请流，假定已经获取了threadHandle
//     auto fakeThreadHandle = MockThreadAllocWithStream(commEngine);

//     // kernel下发
//     // 需要与样例需要的load args对应
//     std::vector<uint64_t> taskArgs{1, 2};
//     void *fakeTaskArgs = static_cast<void *>(taskArgs.data());
//     uint32_t fakeArgSize = taskArgs.size();
//     EXPECT_EQ(HcommCcuKernelLaunch(fakeThreadHandle, kernelHandle,
//         fakeTaskArgs, fakeArgSize), CcuResult::CCU_SUCCESS);

//     // 清理各种资源，析构有时序要求
//     // 主线功能有bug，暂时不能主动释放stream
//     // constexpr uint32_t fakeThreadNum = 1;
//     // EXPECT_EQ(HcommThreadFree(&fakeThreadHandle, fakeThreadNum), HcclResult::HCCL_SUCCESS);
//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRegister_LoopAdd_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRegister_LoopAdd_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);
//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuLoopAddDemoKernel;
//     CcuLoopAddKernelArg demoArg{};
//     demoArg.numA = 3;
//     demoArg.numB = 4;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);
//     CcuKernelFunc demoFunc = CcuLoopAddDemoKernel;
//     CcuLoopAddKernelArg demoArg{};
//     demoArg.numA = 3;
//     demoArg.numB = 4;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_loop_add_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     char *kernelFuncName = "ccu_loop_add_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }
//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelIf_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelIf_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);
//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuIfDemoKernel;
//     CcuIfDemoKernelArg demoArg{};
//     demoArg.value = 42;
//     demoArg.expected = 42;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);
//     CcuKernelFunc demoFunc = CcuIfDemoKernel;
//     CcuIfDemoKernelArg demoArg{};
//     demoArg.value = 42;
//     demoArg.expected = 42;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_if_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     char *kernelFuncName = "ccu_if_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }
//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelIfNoElse_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);
//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuIfNoElseDemoKernel;
//     CcuIfNoElseDemoKernelArg demoArg{};
//     demoArg.value = 42;
//     demoArg.threshold = 42;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_if_no_else_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelNestedIfIf_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuNestedIfIfDemoKernel;
//     CcuNestedIfIfDemoKernelArg demoArg{};
//     demoArg.outerVal = 1;
//     demoArg.outerExpected = 1;
//     demoArg.innerVal = 2;
//     demoArg.innerExpected = 2;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_nested_if_if_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }
//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelWhile_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelWhile_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);
//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuWhileDemoKernel;
//     CcuWhileDemoKernelArg demoArg{};
//     demoArg.loopCount = 5;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);
//     CcuKernelFunc demoFunc = CcuWhileDemoKernel;
//     CcuWhileDemoKernelArg demoArg{};
//     demoArg.loopCount = 5;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_while_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     char *kernelFuncName = "ccu_while_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }
//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelDoWhile_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuDoWhileWhileDemoKernel;
    CcuDoWhileWhileDemoKernelArg demoArg{};
    demoArg.loopCount = 5;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_do_while_while_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelNestedIfOuterElse_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuNestedIfOuterElseDemoKernel;
//     CcuNestedIfOuterElseDemoKernelArg demoArg{};
//     demoArg.outerVal = 1;
//     demoArg.outerExpected = 1;
//     demoArg.innerVal = 2;
//     demoArg.innerExpected = 2;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_nested_if_outer_else_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelNestedIfInnerElse_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuNestedIfInnerElseDemoKernel;
//     CcuNestedIfInnerElseDemoKernelArg demoArg{};
//     demoArg.outerVal = 1;
//     demoArg.outerExpected = 1;
//     demoArg.innerVal = 2;
//     demoArg.innerExpected = 2;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_nested_if_inner_else_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelDoWhileUnified_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     CcuKernelFunc demoFunc = CcuDoWhileUnifiedDemoKernel;
//     CcuDoWhileUnifiedDemoKernelArg demoArg{};
//     demoArg.loopCount = 5;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_do_while_unified_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }

// TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelReduceScatterMesh1d_When_AllFine_Expect_ReturnCcuSUCCESS)
// {
//     HcclResult hcclRet = HcclResult::HCCL_E_RESERVED;
//     CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
//     constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM;
//     MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
//     int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
//     MOCKER(hrtGetDevice).stubs().with(outBound(&fakeDeviceLogicId)).will(returnValue(HcclResult::HCCL_SUCCESS));
//     EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, hcomm::CcuVersion::CCU_V1), HcclResult::HCCL_SUCCESS);
//     MockCcuChannelGetRes();
//     MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

//     constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
//     auto insType = MS_INS_TPYE;
//     void *ccuResDesc = static_cast<void *>(&insType);
//     CcuInsHandle insHandle{0};
//     ccuRet = HcommCcuInsCreate(ccuResDesc, &insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     // reduce-scatter 需要 rankSize-1 个对等通道，本用例采用最小 2-rank 配置，建单条通道
//     constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
//     constexpr uint32_t srcDevPhyId = fakeDevId;
//     constexpr uint32_t dstDevPhyId = 1;
//     constexpr uint32_t srcIp = 167772383;
//     constexpr uint32_t dstIp = 0x87654321;
//     const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

//     // 构造 reduce-scatter kernel 参数：2 个 rank，本 rank 为 0，1 条通道连接 peer rank 1
//     CcuKernelFunc demoFunc = CcuReduceScatterMesh1dKernel;
//     ReduceScatterKernelArg demoArg{};
//     demoArg.rankSize       = 2;
//     demoArg.rankId         = 0;
//     demoArg.channelCount   = 1;
//     demoArg.channels[0]    = handlePair.second;
//     demoArg.dataType       = HCCL_DATA_TYPE_FP16;
//     demoArg.outputDataType = HCCL_DATA_TYPE_FP16;
//     demoArg.reduceOp       = HCCL_REDUCE_SUM;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);
//     // 构造 reduce-scatter kernel 参数：2 个 rank，本 rank 为 0，1 条通道连接 peer rank 1
//     CcuKernelFunc demoFunc = CcuReduceScatterMesh1dKernel;
//     ReduceScatterKernelArg demoArg{};
//     demoArg.rankSize       = 2;
//     demoArg.rankId         = 0;
//     demoArg.channelCount   = 1;
//     demoArg.channels[0]    = handlePair.second;
//     demoArg.dataType       = HCCL_DATA_TYPE_FP16;
//     demoArg.outputDataType = HCCL_DATA_TYPE_FP16;
//     demoArg.reduceOp       = HCCL_REDUCE_SUM;
//     auto kernelFunc = reinterpret_cast<void *>(demoFunc);
//     auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterStart(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     char *kernelFuncName = "ccu_reduce_scatter_mesh1d_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     char *kernelFuncName = "ccu_reduce_scatter_mesh1d_demo";
//     CcuKernelHandle kernelHandle{0};
//     ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
//         kernelFunc, kernelArg, &kernelHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
//     ccuRet = HcommCcuKernelRegisterEnd(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

//     MockChannelDestory(handlePair);
//     ccuRet = HcommCcuInsDestroy(insHandle);
//     EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
// }