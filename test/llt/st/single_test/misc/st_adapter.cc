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

#include "network/hccp.h"
#include "dlra_function.h"

#define private public
#define protected public
#include "network_manager_pub.h"
#undef private

using namespace std;
using namespace hccl;

class HccpTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccpTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HccpTest TearDown" << std::endl;
    }
};

TEST_F(HccpTest, ut_hrtRdmaInitWithBackupAttr_notSupport)
{
    struct rdev_init_info redvInitInfo;
    redvInitInfo.mode = 1;
    redvInitInfo.notify_type = 0;
    redvInitInfo.enabled_910a_lite = false;
    redvInitInfo.disabled_lite_thread = false;
    redvInitInfo.enabled_2mb_lite = false;

    struct rdev rdevInfo;
    rdevInfo.phy_id = 0;
    struct rdev backupRdevInfo;
    backupRdevInfo.phy_id = 1;
    RdmaHandle rdmaHandle;

    MOCKER(hrtRaGetInterfaceVersion).expects(atMost(1)).will(returnValue(HCCL_E_NOT_SUPPORT));
    auto ret = HrtRdmaInitWithBackupAttr(redvInitInfo, rdevInfo, backupRdevInfo, rdmaHandle);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}

TEST_F(HccpTest, ut_CheckAutoListenVersion)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = NetworkManager::GetInstance(0).CheckAutoListenVersion(true);
    EXPECT_EQ(ret, (ret, HCCL_E_NOT_SUPPORT));
    GlobalMockObject::verify();
}