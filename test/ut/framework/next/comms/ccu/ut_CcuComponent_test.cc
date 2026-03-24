/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"

#include <iostream>

#define private public
#define protected public

#include "ccu_comp.h"

#undef protected
#undef private

#include "mocks/ccu_device_mock_utils.h"

class CcuComponentTest : public BaseInit {
public:
    void SetUp() override {
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

TEST_F(CcuComponentTest, Ut_CcuComponent_Init_When_Mock_Is_Fine_Expect_Return_Ok)
{
    const int32_t devLogicId = MAX_MODULE_DEVICE_NUM - 1; // 避免影响其他用例
    const hcomm::CcuVersion ccuVersion = hcomm::CcuVersion::CCU_V1;
    MockCcuNetworkDeviceDefault(devLogicId); // 先处理网络设备，再初始化ccu
    MockCcuResourcesDefault(devLogicId, ccuVersion);

    hcomm::CcuComponent ccuComponent{};
    ccuComponent.devLogicId_ = devLogicId;

    EXPECT_EQ(ccuComponent.Init(), HcclResult::HCCL_SUCCESS);
}
