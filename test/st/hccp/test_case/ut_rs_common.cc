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

TEST_F(HccpSystemTest, MultiHostTlsTest)
{
    const std::vector<int> test_phyids = {1,5};

    RunTests(test_phyids, [](int phyid)->int {
        std::cout << "[HOST-" << phyid << "] Running RaInit...\n";
        RaInitConfig config;
        config.phyId = phyid;
        config.nicPosition = 1;
        config.hdcType = 18;
        config.enableHdcAsync = false;
        int ret = RaInit(&config);
        if (ret != 0) {
            std::cerr << "[HOST-" << phyid << "] RaInit failed ret=" << ret << "\n";
            return ret;
        }
        bool tls_enable = false;
        RaInfo info{.mode = 1, .phyId = phyid};

        std::cout << "[HOST-" << phyid << "] Running RaGetTlsEnable...\n";
        ret = RaGetTlsEnable(&info, &tls_enable);
        if (ret != 0) {
            std::cerr << "[HOST-" << phyid << "] RaGetTlsEnable failed ret=" << ret << "\n";
            RaDeinit(&config);
        }
        ret = RaDeinit(&config);
        if (ret != 0) {
            std::cerr << "[HOST-" << phyid << "] RaDeinit failed ret=" << ret << "\n";
            return ret;
        }

        std::cout << "[HOST-" << phyid << "] TLS enable result: " << tls_enable << "\n";
        return ret;
    });
}