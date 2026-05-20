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
#include "ccu_device_res.h"
#include "ccu_launch.h"

#include "mocks/ccu_device_mock_utils.h"
#include "mocks/ccu_channel_mock_utils.h"

#include "adapter_rts.h"
#include "adapter_hal_pub.h"

#include "hcomm_primitives.h"

#include "ccu_kernel_impl/ccu_var_add_simple_demo.h"
#include "ccu_kernel_impl/ccu_loop_add_demo.h"
#include "ccu_kernel_impl/ccu_func_call_demo.h"
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

#define CCU_FUNC_KERNEL_TEST(testName, demoFunc, expectRegisterSuccess)                                      \
TEST_F(HcommCcuControlApiTest, testName)                                                                    \
{                                                                                                           \
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;                                                           \
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 2;                                               \
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));                                     \
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);                                            \
    MOCKER(hrtGetDevice).stubs()                                                                            \
        .with(outBoundP(&fakeDeviceLogicId))                                                                \
        .will(returnValue(HcclResult::HCCL_SUCCESS));                                                       \
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()                                                               \
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())                             \
        .will(returnValue(HcclResult::HCCL_SUCCESS));                                                       \
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;                                 \
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);                                                         \
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);        \
    MockCcuChannelGetRes();                                                                                 \
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));                                  \
                                                                                                            \
    CcuResDesc resDesc{};                                                                                   \
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;                                                              \
    resDesc.insType = CcuInstanceType::CCU_MS;                                                              \
    constexpr uint32_t descNum = 1;                                                                         \
    CcuInsHandle insHandle{0};                                                                              \
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);                         \
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);                                                              \
                                                                                                            \
    ccuRet = HcommCcuKernelRegisterStart(insHandle);                                                        \
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);                                                              \
                                                                                                            \
    int32_t dummyArg = 0;                                                                                   \
    CcuKernelArg kernelArg = static_cast<CcuKernelArg>(&dummyArg);                                          \
    CcuKernelHandle kernelHandle{0};                                                                        \
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);                                                   \
    ccuRet = HcommCcuKernelRegister(insHandle, const_cast<char *>(#demoFunc), kernelFunc, kernelArg,        \
        &kernelHandle);                                                                                     \
    if (expectRegisterSuccess) {                                                                            \
        EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);                                                          \
        ccuRet = HcommCcuKernelRegisterEnd(insHandle);                                                      \
        EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);                                                          \
    } else {                                                                                                \
        EXPECT_NE(ccuRet, CcuResult::CCU_SUCCESS);                                                          \
    }                                                                                                       \
                                                                                                            \
    ccuRet = HcommCcuInsDestroy(insHandle);                                                                 \
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);                                                              \
}

CCU_FUNC_KERNEL_TEST(Ut_FuncCallBasic_Expect_Success, CcuFuncCallBasicDemoKernel, true)
CCU_FUNC_KERNEL_TEST(Ut_FuncCallReuse_Expect_Success, CcuFuncCallReuseDemoKernel, true)
CCU_FUNC_KERNEL_TEST(Ut_FuncCallMultiArg_Expect_Success, CcuFuncCallMultiArgDemoKernel, true)
CCU_FUNC_KERNEL_TEST(Ut_FuncCallInLoop_Expect_Fail, CcuFuncCallInLoopInvalidDemoKernel, false)
CCU_FUNC_KERNEL_TEST(Ut_FuncCallNested_Expect_Fail, CcuFuncCallNestedInvalidDemoKernel, false)

TEST_F(HcommCcuControlApiTest, Ut_LoopObjectApi_Expect_Success)
{
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
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = CcuInstanceType::CCU_MS;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    CcuLoopAddKernelArg demoArg{};
    demoArg.numA = 3;
    demoArg.numB = 4;
    CcuKernelArg kernelArg = static_cast<CcuKernelArg>(&demoArg);
    CcuKernelHandle kernelHandle{0};
    auto kernelFunc = reinterpret_cast<void *>(CcuLoopAddDemoKernel);
    ccuRet = HcommCcuKernelRegister(insHandle, const_cast<char *>("CcuLoopAddDemoKernel"),
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRegister_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    // 整体打桩，处理ccu资源
    HcommResult hcclRet = 0;
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

    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // 建链流程打桩
    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383; // 需要与RaGetDevEidInfoList接口打桩一致
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    // 构造CcuKernel实现
    CcuKernelFunc demoFunc = CcuLoadStoreDemoKernel;
    CcuVarAddKernelArg demoArg{};
    demoArg.numA = 1;
    demoArg.numB = 2;
    demoArg.channelHandle = handlePair.second;

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
    // 需要与样例需要的load args对应
    std::vector<uint64_t> taskArgs{};
    void *fakeTaskArgs = static_cast<void *>(taskArgs.data());
    uint32_t fakeArgSize = taskArgs.size();
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

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelDoWhile_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 3;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuDoWhileWhileDemoKernel;
    CcuDoWhileWhileDemoKernelArg demoArg{};
    demoArg.loopCount = 5;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_do_while_while_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelNestedIfOuterElse_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 4;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuNestedIfOuterElseDemoKernel;
    CcuNestedIfOuterElseDemoKernelArg demoArg{};
    demoArg.outerVal = 1;
    demoArg.outerExpected = 1;
    demoArg.innerVal = 2;
    demoArg.innerExpected = 2;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_nested_if_outer_else_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelNestedIfInnerElse_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 5;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuNestedIfInnerElseDemoKernel;
    CcuNestedIfInnerElseDemoKernelArg demoArg{};
    demoArg.outerVal = 1;
    demoArg.outerExpected = 1;
    demoArg.innerVal = 2;
    demoArg.innerExpected = 2;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_nested_if_inner_else_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelDoWhileUnified_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 6;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuDoWhileUnifiedDemoKernel;
    CcuDoWhileUnifiedDemoKernelArg demoArg{};
    demoArg.loopCount = 5;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_do_while_unified_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

// =================================================================================
// 以下为针对 PR #2120 增量覆盖率（ccu_kernel.cc / ccu_primitives_impl.cc）补充的用例
// 模板与上方 Ut_HcommCcuKernelDoWhile_* 完全一致：mock 资源 → 建实例 → 建链 →
// RegisterStart → InjectXn → Register → RegisterEnd → 销毁。
// 每条用例采用唯一的 fakeDevId（MAX_MODULE_DEVICE_NUM - N）以避免 CCU 资源串扰。
// =================================================================================

// ----- P0: CcuLoopAddDemoKernel：覆盖 ccu_kernel.cc 中 Loop / LoopGroup / Executor
//        全部接口（L1456-L1736），单条用例预计可拉升整体覆盖率 ~15%
TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelLoopAdd_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 7;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuLoopAddDemoKernel;
    CcuLoopAddKernelArg demoArg{};
    demoArg.numA = 7;
    demoArg.numB = 11;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_loop_add_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

// ----- P0: CcuRemoteReadKernel：覆盖 ReadMemToMem / ReadMemToBuffer / ReadMemToMemReduce
TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRemoteRead_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 8;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuRemoteReadKernel;
    CcuVarAddKernelArg demoArg{};
    demoArg.channelHandle = handlePair.second;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_remote_read_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

// ----- P0: CcuRemoteWriteKernel：覆盖 WriteMemToMem / WriteBufferToMem / WriteMemToMemReduce
TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelRemoteWrite_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 9;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuRemoteWriteKernel;
    CcuVarAddKernelArg demoArg{};
    demoArg.channelHandle = handlePair.second;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_remote_write_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

// ----- P0: CcuAllocDemoKernel：覆盖 VariableAlloc / AddressAlloc / EventAlloc /
//        BlockEventAlloc / VariableCreateByChannel + 地址加法 + 事件 record/wait
TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelAlloc_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    HcommResult hcclRet = 0;
    CcuResult ccuRet = CcuResult::CCU_E_RESERVED;
    constexpr uint32_t fakeDevId = MAX_MODULE_DEVICE_NUM - 10;
    MOCKER(HcclGetThreadDeviceId).stubs().will(returnValue(fakeDevId));
    int32_t fakeDeviceLogicId = static_cast<int32_t>(fakeDevId);
    MOCKER(hrtGetDevice).stubs()
        .with(outBoundP(&fakeDeviceLogicId))
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER(hrtGetDevicePhyIdByIndex).stubs()
        .with(any(), outBound(static_cast<uint32_t>(fakeDeviceLogicId)), any())
        .will(returnValue(HcclResult::HCCL_SUCCESS));
    constexpr hcomm::CcuVersion fakeCcuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(fakeDeviceLogicId);
    EXPECT_EQ(MockCcuResourcesDefault(fakeDeviceLogicId, fakeCcuVersion), HcclResult::HCCL_SUCCESS);
    MockCcuChannelGetRes();
    MOCKER(hrtMemcpy).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));

    constexpr auto MS_INS_TPYE = CcuInstanceType::CCU_MS;
    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383;
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    CcuKernelFunc demoFunc = CcuAllocDemoKernel;
    CcuVarAddKernelArg demoArg{};
    demoArg.channelHandle = handlePair.second;
    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    char *kernelFuncName = "ccu_alloc_demo";
    CcuKernelHandle kernelHandle{0};
    ccuRet = HcommCcuKernelRegister(insHandle, kernelFuncName,
        kernelFunc, kernelArg, &kernelHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    ccuRet = HcommCcuKernelRegisterEnd(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    MockChannelDestory(handlePair);
    ccuRet = HcommCcuInsDestroy(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);
}

TEST_F(HcommCcuControlApiTest, Ut_HcommCcuKernelReduceScatterMesh1d_When_AllFine_Expect_ReturnCcuSUCCESS)
{
    // 整体打桩，处理ccu资源
    HcommResult hcclRet = 0;
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

    CcuResDesc resDesc{};
    resDesc.dieId = hcomm::CCU_MAX_IODIE_NUM;
    resDesc.insType = MS_INS_TPYE;
    constexpr uint32_t descNum = 1;
    CcuInsHandle insHandle{0};
    ccuRet = HcommCcuInsCreate(static_cast<void *>(&resDesc), descNum, &insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // 建链流程打桩
    constexpr auto commEngine = CommEngine::COMM_ENGINE_CCU;
    constexpr uint32_t srcDevPhyId = fakeDevId;
    constexpr uint32_t dstDevPhyId = 1;
    constexpr uint32_t srcIp = 167772383; // 需要与RaGetDevEidInfoList接口打桩一致
    constexpr uint32_t dstIp = 0x87654321;
    const auto &handlePair = MockCcuChannelConnect(srcDevPhyId, dstDevPhyId, srcIp, dstIp, commEngine);

    // 构造CcuKernel实现
    CcuKernelFunc demoFunc = CcuReduceScatterMesh1dKernel;
    ReduceScatterKernelArg demoArg{};
    demoArg.rankSize       = 2;
    demoArg.rankId         = 0;
    demoArg.channelCount   = 1;
    demoArg.channels[0]    = handlePair.second;
    demoArg.dataType       = HCCL_DATA_TYPE_FP16;
    demoArg.outputDataType = HCCL_DATA_TYPE_FP16;
    demoArg.reduceOp       = HCCL_REDUCE_SUM;

    auto kernelFunc = reinterpret_cast<void *>(demoFunc);
    auto kernelArg = static_cast<CcuKernelArg>(&demoArg);

    // 重置CCU资源
    ccuRet = HcommCcuKernelRegisterStart(insHandle);
    EXPECT_EQ(ccuRet, CcuResult::CCU_SUCCESS);

    // kernel注册
    char *kernelFuncName = "ccu_reduce_scatter_mesh1d_demo";
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
    // 需要与样例需要的load args对应（CcuReduceScatterMesh1dKernel::LoadArgs 共 15 个）
    std::vector<uint64_t> taskArgs(15, 0);
    void *fakeTaskArgs = static_cast<void *>(taskArgs.data());
    uint32_t fakeArgSize = taskArgs.size();
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
