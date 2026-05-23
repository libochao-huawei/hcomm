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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hcomm_c_adpt.h"
#include "hcomm_res_defs.h"
#include "endpoint_pair.h"

#define private public
#define protected public

#include "urma_mem.h"

#undef protected
#undef private

using namespace hcomm;

class UrmaRegedMemMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "UrmaRegedMemMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UrmaRegedMemMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in UrmaRegedMemMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in UrmaRegedMemMgrTest TearDown" << std::endl;
    }
};

// 父+子集注册 → 先解注册子 → 再解注册父
TEST_F(UrmaRegedMemMgrTest, ut_UbRegedMemMgr_URMA_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    UbRegedMemMgr ubRegedMemMgr{};
    HcommMem mem0;
    mem0.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem0.addr = (void*)0x100;
    mem0.size = 100;
    std::string memTag0 = "buffer0";
    void *memHandle0 = nullptr;
    HcclResult ret = ubRegedMemMgr.RegisterMemory(mem0, memTag0.c_str(), &memHandle0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcommMem mem1;
    mem1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem1.addr = (void*)0x100;
    mem1.size = 10;
    std::string memTag1 = "buffer1";
    void *memHandle1 = nullptr;
    ret = ubRegedMemMgr.RegisterMemory(mem1, memTag1.c_str(), &memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 先解注册子buffer（通过handle找父key做Del，父ref 2→1）
    ret = ubRegedMemMgr.UnregisterMemory(memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 再解注册父buffer（自己的key在tree中，ref 1→0）
    ret = ubRegedMemMgr.UnregisterMemory(memHandle0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 多次注册同一范围，各自解注册
TEST_F(UrmaRegedMemMgrTest, ut_UbRegedMemMgr_URMA_When_MultipleSameRange_Expect_AllSuccess)
{
    UbRegedMemMgr ubRegedMemMgr{};
    HcommMem mem;
    mem.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem.addr = (void*)0x3000;
    mem.size = 1024;

    void *h1 = nullptr, *h2 = nullptr, *h3 = nullptr;
    ASSERT_EQ(ubRegedMemMgr.RegisterMemory(mem, "t1", &h1), HCCL_SUCCESS);
    ASSERT_EQ(ubRegedMemMgr.RegisterMemory(mem, "t2", &h2), HCCL_SUCCESS);
    ASSERT_EQ(ubRegedMemMgr.RegisterMemory(mem, "t3", &h3), HCCL_SUCCESS);

    EXPECT_EQ(ubRegedMemMgr.UnregisterMemory(h3), HCCL_SUCCESS);
    EXPECT_EQ(ubRegedMemMgr.UnregisterMemory(h2), HCCL_SUCCESS);
    EXPECT_EQ(ubRegedMemMgr.UnregisterMemory(h1), HCCL_SUCCESS);
}