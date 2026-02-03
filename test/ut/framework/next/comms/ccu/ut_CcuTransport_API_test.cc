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

#include "ccu_transport.h"

#undef protected
#undef private

class CcuTransportTest : public BaseInit {
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

TEST_F(CcuTransportTest, Ut_CcuTransport)
{
    std::unique_ptr<hcomm::CcuTransport> impl{};
    hcomm::CcuTransport::CcuConnectionInfo connInfo{};
    hcomm::CcuTransport::CclBufferInfo buffInfo{};
    hcomm::CcuCreateTransport(nullptr, connInfo, buffInfo, impl);
    // std::unique_ptr<Hccl::CcuTransport> impl{};
    // Hccl::CcuTransport::CcuConnectionInfo connInfo{};
    // Hccl::CcuTransport::CclBufferInfo buffInfo{};
    // Hccl::CcuCreateTransport(nullptr, connInfo, buffInfo, impl);

    std::cout << "Hello World" << std::endl;
}
