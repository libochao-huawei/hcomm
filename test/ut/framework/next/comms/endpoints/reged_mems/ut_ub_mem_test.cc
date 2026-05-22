/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hcomm_c_adpt.h"
#include "hcomm_res_defs.h"

#define private public
#define protected public
#include "ub_mem.h"
#undef protected
#undef private

using namespace hcomm;

class UbMemRegedMemMgrTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

// 参数校验
TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_MemHandleNull_Returns_E_PTR)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x1000;
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", nullptr), HCCL_E_PTR);
}

TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_SizeZero_Returns_E_PARA)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x100;
    mem.size = 0;
    mem.type = COMM_MEM_TYPE_DEVICE;
    void *h = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", &h), HCCL_E_PARA);
}

TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_InvalidMemType_Returns_E_PARA)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x200;
    mem.size = 512;
    mem.type = COMM_MEM_TYPE_INVALID;
    void *h = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", &h), HCCL_E_PARA);
}

// 首次注册成功
TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_FirstRegistration_Returns_ValidHandle)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x2000;
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;
    void *h = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "firstTag", &h), HCCL_SUCCESS);
    EXPECT_NE(h, nullptr);
}

// 精确命中：不同handle
TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_ExactMatch_Returns_DifferentHandle)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x3000;
    mem.size = 2048;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *h1 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "tagA", &h1), HCCL_SUCCESS);

    void *h2 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "tagB", &h2), HCCL_SUCCESS);
    EXPECT_NE(h1, h2);
}

// 子集
TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_Subset_Returns_ValidHandle)
{
    UbMemRegedMemMgr mgr{};
    HcommMem parent{};
    parent.addr = (void*)0x5000;
    parent.size = 8192;
    parent.type = COMM_MEM_TYPE_DEVICE;

    HcommMem subset{};
    subset.addr = (void*)0x5200;
    subset.size = 512;
    subset.type = COMM_MEM_TYPE_DEVICE;

    void *hParent = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(parent, "parent", &hParent), HCCL_SUCCESS);

    void *hSub = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(subset, "subset", &hSub), HCCL_SUCCESS);
    EXPECT_NE(hSub, hParent);
}

// 多次命中
TEST_F(UbMemRegedMemMgrTest, Ut_RegisterMemory_When_MultipleHits_Returns_DistinctHandles)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x6000;
    mem.size = 256;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handles[3] = {};
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(mgr.RegisterMemory(mem, "t", &handles[i]), HCCL_SUCCESS);
    }
    EXPECT_NE(handles[0], handles[1]);
    EXPECT_NE(handles[0], handles[2]);
    EXPECT_NE(handles[1], handles[2]);
}

// UnregisterMemory
TEST_F(UbMemRegedMemMgrTest, Ut_UnregisterMemory_When_HandleNull_Returns_E_PTR)
{
    UbMemRegedMemMgr mgr{};
    EXPECT_EQ(mgr.UnregisterMemory(nullptr), HCCL_E_PTR);
}

TEST_F(UbMemRegedMemMgrTest, Ut_UnregisterMemory_Basic_Flow)
{
    UbMemRegedMemMgr mgr{};
    HcommMem mem{};
    mem.addr = (void*)0x7000;
    mem.size = 1024;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *h = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "t", &h), HCCL_SUCCESS);
    EXPECT_EQ(mgr.UnregisterMemory(h), HCCL_SUCCESS);
}
