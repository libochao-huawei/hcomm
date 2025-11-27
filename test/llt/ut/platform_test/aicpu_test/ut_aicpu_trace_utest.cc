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

#ifndef private
#define private public
#define protected public
#endif
#include "mmpa_api.h"
#include "utrace_api.h"

#include "dfx/mc2_trace_utils.h"
#include "common/aicpu_hccl_common.h"
#include "common/aicpu_kfc_def.h"

#include "llt_hccl_stub.h"
#include "llt_hccl_stub_mc2.h"
#undef private
#undef protected

using namespace std;

namespace {
char g_traceHandleStub;
TraceStructEntry g_traceStructEntryStub;
bool g_isError = false;

TraHandle UtraceCreateWithAttr(TracerType tracerType, const char *objName, const TraceAttr *attr)
{
    if (g_isError) {
        return -1;
    }
    return 1;
}

TraHandle UtraceGetHandle(TracerType tracerType, const char *objName)
{
    return 1;
}

TraStatus UtraceSubmit(TraHandle handle, const void *buffer, uint32_t bufSize)
{
    if (buffer == nullptr) {
        return TRACE_FAILURE;
    }
    return TRACE_SUCCESS;
}

TraStatus UtraceSave(TracerType tracerType, bool syncFlag)
{
    return TRACE_SUCCESS;
}

TraEventHandle UtraceEventCreate(const char *eventName)
{
    return 1;
}

void UtraceEventDestroy(TraEventHandle eventHandle)
{
    return;
}

void UtraceDestroy(TraHandle handle)
{
    return;
}

TraStatus UtraceEventBindTrace(TraEventHandle eventHandle, TraHandle handle)
{
    return TRACE_SUCCESS;
}

TraStatus UtraceEventReport(TraEventHandle eventHandle)
{
    return TRACE_SUCCESS;
}

TraStatus UtraceSetGlobalAttr(const TraceGlobalAttr *attr)
{
    return TRACE_SUCCESS;
}

TraceStructEntry *UtraceStructEntryCreate(const char *name)
{
    return &g_traceStructEntryStub;
}

void UtraceStructItemFieldSet(TraceStructEntry *en, const char *name, uint8_t type, uint8_t mode, uint8_t bytes,
    uint64_t length)
{
    return;
}

void UtraceStructItemArraySet(TraceStructEntry *en, const char *name, uint8_t type, uint8_t mode, uint8_t bytes,
    uint64_t length)
{
    return;
}

void UtraceStructSetAttr(TraceStructEntry *en, TraceAttr *attr)
{
    return;
}

void UtraceStructEntryDestroy(TraceStructEntry *en)
{
    return;
}

std::map<std::string, void *> Mc2TraceMap = {
    { "UtraceCreateWithAttr", (void *)UtraceCreateWithAttr },
    { "UtraceGetHandle", (void *)UtraceGetHandle },
    { "UtraceSubmit", (void *)UtraceSubmit },
    { "UtraceEventCreate", (void *)UtraceEventCreate },
    { "UtraceEventDestroy", (void *)UtraceEventDestroy },
    { "UtraceDestroy", (void *)UtraceDestroy },
    { "UtraceEventBindTrace", (void *)UtraceEventBindTrace },
    { "UtraceEventReport", (void *)UtraceEventReport },
    { "UtraceSetGlobalAttr", (void *)UtraceSetGlobalAttr },
    { "UtraceStructEntryCreate", (void *)UtraceStructEntryCreate },
    { "UtraceStructItemFieldSet", (void *)UtraceStructItemFieldSet },
    { "UtraceStructItemArraySet", (void *)UtraceStructItemArraySet },
    { "UtraceStructSetAttr", (void *)UtraceStructSetAttr },
    { "UtraceStructEntryDestroy", (void *)UtraceStructEntryDestroy },
};

void *DlopenStub(const char *filename, int flags)
{
    if (filename == nullptr) {
        return nullptr;
    }

    return (void *)&g_traceHandleStub;
}

void *DlsymStub(void *handle, const char *symbol)
{
    if (symbol == nullptr) {
        return nullptr;
    }
    std::string symbolt = symbol;
    auto it = Mc2TraceMap.find(symbolt);
    if (it != Mc2TraceMap.cend()) {
        return it->second;
    }
    return nullptr;
}

int DlcloseStub(void *handle)
{
    if (handle == nullptr) {
        return -1;
    } else if (handle == (void *)&g_traceHandleStub) {
        return 0;
    }

    return dlclose(handle);
}

void MockTraceDlopen()
{
    MOCKER(mmDlopen).stubs().will(invoke(DlopenStub));
    MOCKER(mmDlsym).stubs().will(invoke(DlsymStub));
}
}

class MC2Trace_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MC2Trace_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MC2Trace_UT TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        g_stubDevType = DevType::DEV_TYPE_910B;
        MockTraceDlopen();
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "MC2Trace_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "MC2Trace_UT Test TearDown" << std::endl;
    }
};

TEST(MC2TraceUtilsTest, ut_GetPlatform)
{
    DevType tem = g_stubDevType;
    g_stubDevType = DevType::DEV_TYPE_910_93;
    MOCKER(halGetDeviceInfo)
    .stubs()
    .with(any())
    .will(invoke(StubhalGetDeviceInfo));

    halChipInfo info = {"Ascend", "910_9381", "0"};
    MOCKER(halGetChipInfo)
    .stubs()
    .with(any(), outBoundP(&info, sizeof(halChipInfo)))
    .will(returnValue(DRV_ERROR_NONE));

    DevType devType = DevType::DEV_TYPE_COUNT;
    HcclResult ret = hrtHalGetDeviceType(0, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(devType, DevType::DEV_TYPE_910B);
    GlobalMockObject::verify();
    g_stubDevType = tem;
}

TEST(MC2TraceUtilsTest, ut_GetPlatform_310P)
{
    DevType tem = g_stubDevType;
    g_stubDevType = DevType::DEV_TYPE_310P3;
    MOCKER(halGetDeviceInfo)
    .stubs()
    .with(any())
    .will(invoke(StubhalGetDeviceInfo));

    halChipInfo info = {"Ascend", "310P3", "0"};
    MOCKER(halGetChipInfo)
    .stubs()
    .with(any(), outBoundP(&info, sizeof(halChipInfo)))
    .will(returnValue(DRV_ERROR_NONE));

    DevType devType = DevType::DEV_TYPE_COUNT;
    g_stubDevType = DevType::DEV_TYPE_310P1;
    HcclResult ret = hrtHalGetDeviceType(0, devType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(devType, DevType::DEV_TYPE_310P3);

    GlobalMockObject::verify();
    g_stubDevType = tem;
}