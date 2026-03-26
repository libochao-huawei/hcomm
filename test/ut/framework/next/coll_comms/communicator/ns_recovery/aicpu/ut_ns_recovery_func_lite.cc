/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <thread>
#define private public
#include "ns_recovery_func_lite.h"
#include "coll_comm_aicpu.h"
#include "kfc.h"
#include "hdc_pub.h"
#undef private

using namespace hccl;

constexpr u32 h2dBufferSize = sizeof(Hccl::KfcCommand);
constexpr u32 d2hBufferSize = sizeof(Hccl::KfcStatus);
#define HCCL_HDC_TYPE_D2H 0
#define HCCL_HDC_TYPE_H2D 1

static HcclResult HrtDrvMemCpyStub(void *dst, uint64_t destMax, const void *src, uint64_t count)
{
    memcpy(dst, src, count);
    return HCCL_SUCCESS;
}

class NsRecoveryFuncLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "NsRecoveryFuncLiteTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "NsRecoveryFuncLiteTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        memset_s(hostBufH2d, sizeof(hostBufH2d), 0, sizeof(hostBufH2d));
        memset_s(hostBufD2h, sizeof(hostBufD2h), 0, sizeof(hostBufD2h));
        memset_s(hostCacheD2h, sizeof(hostCacheD2h), 0, sizeof(hostCacheD2h));
        MOCKER(HrtMallocHost).stubs().with(any()).will(returnValue(static_cast<void *>(hostBufH2d)))
                                                .then(returnValue(static_cast<void *>(hostBufD2h)))
                                                .then(returnValue(static_cast<void *>(hostCacheD2h)));
        memset_s(devBufH2d, sizeof(devBufH2d), 0, sizeof(devBufH2d));
        memset_s(devCacheH2d, sizeof(devCacheH2d), 0, sizeof(devCacheH2d));
        memset_s(devBufD2h, sizeof(devBufD2h), 0, sizeof(devBufD2h));
        MOCKER(HrtMalloc).stubs().with(any(),any()).will(returnValue(static_cast<void *>(devBufH2d)))
                                                    .then(returnValue(static_cast<void *>(devCacheH2d)))
                                                    .then(returnValue(static_cast<void *>(devBufD2h)));
        MOCKER(HrtDrvMemCpy).stubs().with().will(invoke(HrtDrvMemCpyStub));

        std::cout << "A Test case in NsRecoveryFuncLiteTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in NsRecoveryFuncLiteTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    char hostBufH2d[4 * 1024];
    char hostBufD2h[4 * 1024];
    char hostCacheD2h[4 * 1024];
    char devBufH2d[4 * 1024];
    char devCacheH2d[4 * 1024];
    char devBufD2h[4 * 1024];
};

TEST_F(NsRecoveryFuncLiteTest, test_handle_stop_launch)
{
    // Create CollCommAicpu instance
    CollCommAicpu comm;
    comm.isSuspended = false;
    comm.isCommReady = true;

    // Test HandleStopLaunch
    NsRecoveryFuncLite::GetInstance().HandleStopLaunch(&comm);
    EXPECT_EQ(comm.isSuspended, true);
    EXPECT_EQ(comm.needClean, true);
}

TEST_F(NsRecoveryFuncLiteTest, test_handle_clean)
{
    // Create CollCommAicpu instance
    CollCommAicpu comm;
    comm.isSuspended = true;
    comm.isCommReady = true;
    comm.needClean = true;

    // Mock StreamLiteMgr::GetMaster
    u32 fakeStreamId = 0;
    u32 fakeSqId     = 0;
    u32 fakedevPhyId = 0;
    BinaryStream liteBinaryStream;
    liteBinaryStream << fakeStreamId;
    liteBinaryStream << fakeSqId;
    liteBinaryStream << fakedevPhyId;
    std::vector<char> uniqueId{};
    liteBinaryStream.Dump(uniqueId);
    StreamLite stream(uniqueId);
    MOCKER_CPP(&StreamLiteMgr::GetMaster).stubs().with().will(returnValue(&stream));

    // Mock DeviceQuery
    MOCKER_CPP(&NsRecoveryFuncLite::DeviceQuery).stubs().with(any(), any(), any()).will(returnValue(HCCL_SUCCESS));

    // Test HandleClean
    NsRecoveryFuncLite::GetInstance().HandleClean(&comm);
    EXPECT_EQ(comm.needClean, false);
    EXPECT_EQ(comm.isSuspended, true);
}

TEST_F(NsRecoveryFuncLiteTest, test_device_query)
{
    // Test DeviceQuery with error
    MOCKER(halTsdrvCtl).stubs().with(any()).will(returnValue(DRV_ERROR_NOT_SUPPORT));
    auto ret = NsRecoveryFuncLite::GetInstance().DeviceQuery(0, 0, 0);
    EXPECT_EQ(ret, HCCL_E_DRV);
    GlobalMockObject::verify();

    // Test DeviceQuery with success
    MOCKER(halTsdrvCtl).stubs().with(any()).will(returnValue(DRV_ERROR_NONE));
    ret = NsRecoveryFuncLite::GetInstance().DeviceQuery(0, 0, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(NsRecoveryFuncLiteTest, test_get_instance)
{
    // Test GetInstance singleton
    auto &instance1 = NsRecoveryFuncLite::GetInstance();
    auto &instance2 = NsRecoveryFuncLite::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(NsRecoveryFuncLiteTest, test_call_method)
{
    // Test Call method (empty test as it's a daemon function)
    // NsRecoveryFuncLite::GetInstance().Call();
}
