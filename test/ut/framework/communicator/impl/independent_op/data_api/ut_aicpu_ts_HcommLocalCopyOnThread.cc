/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../../hccl_api_base_test.h"

using namespace hccl;

class UtAicpuTsHcommLocalCopyOnThread : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
    }
    void TearDown() override
    {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(UtAicpuTsHcommLocalCopyOnThread, Ut_HcommLocalCopyOnThread_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    AicpuTsThread threadOnDevice{StreaamType::STREAM_TYPE_DEVICE, 0, NotifyLoadType::DEVICE_NOTIFY};
    ThreadHandle thread = reinterpret_cast<THreadHandle>(&threadOnDevice);
    uint64_t tempDst[6] = {0};
    uint64_t tempSrc[6] = {1, 1, 4, 5, 1, 4};
    void *dst = reinterpret_cast<void *>(&tempDst);
    void *src = reinterpret_cast<void *>(&tempSrc);
    uint64_t len = sizeof(tempDst);

    HcclResult res = HCCL_E_RESERVED;

    MOCKER_CPP(&AicpuTsThread::IsDeviceA5).stubs().will(returnValue(true));
    MOCKER_CPP(&AicpuTsThread::LocalCopy).stubs().will(returnValue(HCCL_SUCCESS));

    res = HcommLocalCopyOnThread(thread, dst, src, len);

    EXPECT_EQ(res, HCCL_SUCCESS);
}
