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
#include "ns_recovery_lite.h"
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

class NsRecoveryLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "NsRecoveryLiteTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "NsRecoveryLiteTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        memset_s(hostBufH2d, sizeof(hostBufH2d), 0, sizeof(hostBufH2d));
        memset_s(hostBufD2h, sizeof(hostBufD2h), 0, sizeof(hostBufD2h));
        MOCKER(HrtMallocHost).stubs().with(any()).will(returnValue(static_cast<void *>(hostBufH2d)))
                                                .then(returnValue(static_cast<void *>(hostBufD2h)));
        memset_s(devBufH2d, sizeof(devBufH2d), 0, sizeof(devBufH2d));
        memset_s(devBufD2h, sizeof(devBufD2h), 0, sizeof(devBufD2h));
        MOCKER(HrtMalloc).stubs().with(any(),any()).will(returnValue(static_cast<void *>(devBufH2d)))
                                                    .then(returnValue(static_cast<void *>(devBufD2h)));
        MOCKER(HrtDrvMemCpy).stubs().with().will(invoke(HrtDrvMemCpyStub));

        std::cout << "A Test case in NsRecoveryLiteTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        std::cout << "A Test case in NsRecoveryLiteTest TearDown" << std::endl;
        GlobalMockObject::verify();
    }

    char hostBufH2d[4 * 1024];
    char hostBufD2h[4 * 1024];
    char devBufH2d[4 * 1024];
    char devBufD2h[4 * 1024];
};

TEST_F(NsRecoveryLiteTest, test_init_and_basic_functions)
{
    // Create HDCommunicate instances
    auto kfcControlTransferH2D = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_H2D, h2dBufferSize);
    auto kfcStatusTransferD2H = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_D2H, d2hBufferSize);
    kfcControlTransferH2D->Init();
    kfcStatusTransferD2H->Init();

    // Test NsRecoveryLite initialization
    NsRecoveryLite nsRecovery;
    nsRecovery.Init(kfcControlTransferH2D, kfcStatusTransferD2H);

    // Test SetNeedClean and IsNeedClean
    nsRecovery.SetNeedClean(true);
    EXPECT_EQ(nsRecovery.IsNeedClean(), true);
    nsRecovery.SetNeedClean(false);
    EXPECT_EQ(nsRecovery.IsNeedClean(), false);

    // Test ResetErrorReported
    nsRecovery.ResetErrorReported();

    // Test BackGroundGetCmd and BackGroundSetStatus
    auto cmd = nsRecovery.BackGroundGetCmd();
    EXPECT_EQ(cmd, Hccl::KfcCommand::NONE);

    nsRecovery.BackGroundSetStatus(Hccl::KfcStatus::STOP_LAUNCH_DONE);
    nsRecovery.BackGroundSetStatus(Hccl::KfcStatus::CLEAN_DONE, Hccl::KfcErrType::NONE);
}

TEST_F(NsRecoveryLiteTest, test_ns_recovery_func_lite)
{
    // Test GetInstance
    auto &instance = NsRecoveryFuncLite::GetInstance();

    // Test Call method (empty test as it's a daemon function)
    // instance.Call();

    // Test that instance is singleton
    auto &instance2 = NsRecoveryFuncLite::GetInstance();
    EXPECT_EQ(&instance, &instance2);
}

TEST_F(NsRecoveryLiteTest, test_handle_stop_launch)
{
    // Create CollCommAicpu mock
    CollCommAicpu comm;
    comm.isSuspended = false;
    comm.isCommReady = true;

    // Test HandleStopLaunch
    NsRecoveryFuncLite::GetInstance().HandleStopLaunch(&comm);
    EXPECT_EQ(comm.isSuspended, true);
    EXPECT_EQ(comm.needClean, true);
}

TEST_F(NsRecoveryLiteTest, test_handle_clean)
{
    // Create CollCommAicpu mock
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

TEST_F(NsRecoveryLiteTest, test_device_query)
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
