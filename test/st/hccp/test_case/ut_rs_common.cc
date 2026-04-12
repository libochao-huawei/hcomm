/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_system_test_base.h"
#include "hccp.h"
class HccpSystemTest : public HccpSystemTestBase {

};

TEST_F(HccpSystemTest, HelloWord)
{
    const std::vector<int> test_phyids = {60};

    RunTests(test_phyids, [](int phyid)->int {
        TEST_LOG_INFO(phyid, "hello world!");
        return 0;
    });
}

TEST_F(HccpSystemTest, MultiHostTlsTest)
{
    const std::vector<int> test_phyids = {1,5};

    RunTests(test_phyids, [](int phyid)->int {
        TEST_LOG_INFO(phyid, "Running RaInit...");
        RaInitConfig config;
        config.phyId = phyid;
        config.nicPosition = 1;
        config.hdcType = 18;
        config.enableHdcAsync = false;
        int ret = RaInit(&config);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaInit failed ret=%d", ret);
            return ret;
        }
        bool tls_enable = false;
        RaInfo info{.mode = 1, .phyId = phyid};

        TEST_LOG_INFO(phyid, "Running RaGetTlsEnable...");
        ret = RaGetTlsEnable(&info, &tls_enable);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaGetTlsEnable failed ret=%d", ret);
            RaDeinit(&config);
        }
        ret = RaDeinit(&config);
        if (ret != 0) {
            TEST_LOG_ERROR(phyid, "RaDeinit failed ret=%d", ret);
            return ret;
        }

        TEST_LOG_INFO(phyid, "TLS enable result: %d", tls_enable);
        return ret;
    });
}