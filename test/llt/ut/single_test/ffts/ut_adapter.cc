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

#include <driver/ascend_hal.h>
#include <runtime/rt.h>
#include "config.h"
#include <hccl/base.h>
#include <hccl/hccl_types.h>

#include "adapter/adapter_rts.h"
#include "externalinput_pub.h"
#include "externalinput.h"
#include "llt_hccl_stub_pub.h"
using namespace std;

class AdapterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "AdapterTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AdapterTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(AdapterTest, ut_hrtNotifyGetAddr)
{
    MOCKER(rtGetNotifyAddress)
    .stubs()
    .with(any(), any())
    .will(returnValue(RT_ERROR_NONE));

    HcclRtNotify notify;
    u64 *notifyAddr = new u64(0);

    auto ret = hrtNotifyGetAddr(notify, notifyAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    delete notifyAddr;
}

TEST_F(AdapterTest, ut_hrtGetNotifyID)
{
    MOCKER(aclrtGetNotifyId)
    .stubs()
    .with(any(), any())
    .will(returnValue(ACL_SUCCESS));

    HcclRtNotify signal = new u32(0);
    u32 notifyID;

    auto ret = hrtGetNotifyID(signal, &notifyID);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    delete signal;
}

TEST_F(AdapterTest, ut_hrtGetDeviceInfo)
{
    MOCKER(rtGetDeviceInfo)
    .stubs()
    .with()
    .will(returnValue(RT_ERROR_NONE));

    u32 deviceId = 0;
    HcclRtDeviceModuleType moduleType = HcclRtDeviceModuleType::HCCL_RT_MODULE_TYPE_SYSTEM;
    HcclRtDeviceInfoType infoType = HcclRtDeviceInfoType::HCCL_INFO_TYPE_CUST_OP_ENHANCE;
    s64 val = 1;

    auto ret = hrtGetDeviceInfo(deviceId, moduleType, infoType, val);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(AdapterTest, ut_hrtGetRdmaDoorbellAddr_DEV_TYPE_910_93)
{
    MOCKER(hrtGetDevice)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDeviceInfo)
    .stubs()
    .with(any(), any(), any(), any())
    .will(returnValue(HCCL_SUCCESS));

    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    u32 dbIndex = 0;
    u64 dbAddr = 0;

    auto ret = hrtGetRdmaDoorbellAddr(dbIndex, dbAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    GlobalMockObject::verify();
}

TEST_F(AdapterTest, ut_printMemoryAttr)
{
    MOCKER(HcclCheckLogLevel)
    .stubs()
    .will(returnValue(false));

    const void *memAddr = nullptr;
    auto ret = PrintMemoryAttr(memAddr);
    EXPECT_EQ(HCCL_SUCCESS, ret);
}

TEST_F(AdapterTest, ut_GetMsTimeFromExecTimeout)
{
    // 记录用户设置数值，用于用例执行结束后配置恢复
    s32 execTimeoutActual = 0;
    s32 execTimeoutTmp = GetExternalInputHcclExecTimeOut();
    std::string setTimeOutValue;
    HcclResult ret;
    s32 INPUT_MIN;
    s32 INPUT_MAX;
    s32 EXPECT_MIN;
    s32 EXPECT_MAX;

    DevType deviceType;
    ret = hrtGetDeviceTypeStub(deviceType); // 80和71要分开
    if (deviceType == DevType::DEV_TYPE_910_93 || deviceType == DevType::DEV_TYPE_910B) { // 81/71 execTimeout范围[0, 2147483647]s
        INPUT_MIN = 0;
        INPUT_MAX = HCCL_EXEC_TIME_OUT_S_910_93;
        EXPECT_MIN = 5000;
        EXPECT_MAX = HCCL_EXEC_TIME_OUT_S_910_93;
    } else { // 非81/71 execTimeout范围[1, 17340]s
        INPUT_MIN = 1;
        INPUT_MAX = HCCL_EXEC_TIME_OUT_S;
        EXPECT_MIN = 73000;  // execTimeout最小赋值为68s
        EXPECT_MAX = 17345000;
    }
    // case1: execTimeOut = INPUTMIN
    setTimeOutValue = to_string(INPUT_MIN);
    ret = SetHccLExecTimeOut(setTimeOutValue.c_str(), HcclExecTimeoutSet::HCCL_EXEC_TIMEOUT_SET_BY_ENV);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    execTimeoutActual = GetMsTimeFromExecTimeout();
    EXPECT_EQ(EXPECT_MIN, execTimeoutActual);

    // case2: execTimeOut = INPUTMAX
    setTimeOutValue = to_string(INPUT_MAX);
    ret = SetHccLExecTimeOut(setTimeOutValue.c_str(), HcclExecTimeoutSet::HCCL_EXEC_TIMEOUT_SET_BY_ENV);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    execTimeoutActual = GetMsTimeFromExecTimeout();
    EXPECT_EQ(EXPECT_MAX, execTimeoutActual);

    // case3: execTimeOut = [INPUT_MIN, INPUT_MAX)
    srand(time(0));
    s32 execTimeout = rand()%INPUT_MAX;
    execTimeout = (execTimeout >= INPUT_MIN) ? execTimeout : INPUT_MIN;
    setTimeOutValue = to_string(execTimeout);
    ret = SetHccLExecTimeOut(setTimeOutValue.c_str(), HcclExecTimeoutSet::HCCL_EXEC_TIMEOUT_SET_BY_ENV);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    execTimeoutActual = GetMsTimeFromExecTimeout();
    bool flag = false;
    if ((execTimeoutActual > 0) && (execTimeoutActual < 0x7FFFFFFF)) {
        flag = true;
    }
    EXPECT_EQ(true, flag);

    // 恢复用户设置数值
    setTimeOutValue = to_string(execTimeoutTmp);
    SetHccLExecTimeOut(setTimeOutValue.c_str(), HcclExecTimeoutSet::HCCL_EXEC_TIMEOUT_SET_BY_ENV);
}

TEST_F(AdapterTest, ut_hrtGetDeviceTypeBySocVersion)
{
    log_level_set_stub(3);
    DevType deviceType;
    std::string str{"Ascend310B1"};
    HcclResult cc = hrtGetDeviceTypeBySocVersion(str, deviceType);
    EXPECT_EQ(HCCL_SUCCESS, cc);
    EXPECT_EQ(deviceType, DevType::DEV_TYPE_310P3);
    log_level_set_stub(3);
}

s32 fake_rtGetSocVersionV91095(char *chipVer, const u32 maxLen)
{
    sal_memcpy(chipVer, sizeof("Ascend910_95"), "Ascend910_95", sizeof("Ascend910_95"));
    return DRV_ERROR_NONE;
}

TEST_F(AdapterTest, ut_hrtGetDeviceType_910_95_return_ok)
{
    CallBackInitRts();
    MOCKER(rtGetSocVersion)
    .stubs()
    .will(invoke(fake_rtGetSocVersionV91095));

    DevType deviceType;
    HcclResult cc = hrtGetDeviceType(deviceType);
    EXPECT_EQ(cc, HCCL_SUCCESS);
    EXPECT_EQ(deviceType, DevType::DEV_TYPE_910_95);
}

TEST_F(AdapterTest, ut_hrtGetDeviceTypeBySocVersion_910_95_return_ok)
{
    log_level_set_stub(3);
    DevType deviceType;
    std::string str{"Ascend910_9591"};
    HcclResult cc = hrtGetDeviceTypeBySocVersion(str, deviceType);
    EXPECT_EQ(cc, HCCL_SUCCESS);
    EXPECT_EQ(deviceType, DevType::DEV_TYPE_910_95);
    log_level_set_stub(3);
}