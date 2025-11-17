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


class HcclCommRegMemTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        UT_COMM_CREATE_DEFAULT(comm);
    }
    void TearDown() override {
        Ut_Comm_Destroy(comm);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

static inline HcclMem MakeMem(void* addr, uint64_t size, HcclMemType type)
{
    HcclMem m{}; m.addr = addr; m.size = size; m.type = type; return m;
}
static inline HcclRegMemAttr BindShare(bool allowShare)
{
    HcclRegMemAttr a{}; a.value = 0; a.requireShare = allowShare ? 1 : 0; return a;
}

/*============================
 * 1) 参数校验：全部非法分支
 *============================*/
TEST_F(HcclCommRegMemTest, RegMem_ParamCheck)
{
    void* handle = nullptr;
    HcclRegMemAttr attr = BindShare(true);

    HcclMem bad1 = MakeMem(nullptr, 1024, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(nullptr, "opA", &bad1, attr, &handle)); // comm nullptr

    HcclMem good = MakeMem((void*)0x1000, 1024, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, nullptr, &good, attr, &handle)); // memTag nullptr
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "", &good, attr, &handle));      // memTag empty
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", nullptr, attr, &handle)); // mem nullptr

    HcclMem bad2 = MakeMem((void*)0x1000, 0, HCCL_MEM_TYPE_DEVICE);                    // size = 0
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", &bad2, attr, &handle));

    HcclMem bad3 = MakeMem((void*)0x1000, 128, HCCL_MEM_TYPE_NUM);                     // bad type
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", &bad3, attr, &handle));

    void** badHandle = nullptr;                                                    // memHandle nullptr
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", &good, attr, badHandle));
}

/*=====================================================
 * 2) 同 tag 正常注册 + 幂等：相同 key 返回相同 raw
 *=====================================================*/
TEST_F(HcclCommRegMemTest, RegMem_SameTag_Normal_And_Idempotent)
{
    HcclRegMemAttr attr = BindShare(true);

    void* h1 = nullptr;
    HcclMem m = MakeMem((void*)0x2000, 4096, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opX", &m, attr, &h1));
    EXPECT_NE(h1, nullptr);

    // 同一 tag、相同 key 再次注册：应复用同一 raw
    void* h2 = nullptr;
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opX", &m, attr, &h2));
    EXPECT_EQ(h1, h2);

    // 解绑一次：移除该 tag 下这条绑定
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "opX", h1));
    // 再解绑一次：幂等（可能已经没有绑定），不报错
    (void)HcclCommUnregMem(comm, "opX", h2);
}

/*=========================================================
 * 3) 同 tag 冲突规则：交/子/超集禁止；相邻允许
 *=========================================================*/
TEST_F(HcclCommRegMemTest, RegMem_SameTag_OverlapRules)
{
    HcclRegMemAttr attr = BindShare(true);

    // 基准区间 [0x3000, 0x3000+200)
    void* base = nullptr;
    HcclMem mBase = MakeMem((void*)0x3000, 200, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opA", &mBase, attr, &base));
    ASSERT_NE(base, nullptr);

    // 交集：应失败
    void* h = nullptr;
    HcclMem ov1 = MakeMem((void*)0x30A0, 64, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", &ov1, attr, &h));

    // 子集：应失败
    HcclMem sub = MakeMem((void*)0x3020, 16, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", &sub, attr, &h));

    // 超集：应失败
    HcclMem sup = MakeMem((void*)0x2FF0, 0xE0, HCCL_MEM_TYPE_DEVICE); // 覆盖到 [0x2FF0, 0x30D0)
    EXPECT_EQ(HCCL_E_PARA, HcclCommRegMem(comm, "opA", &sup, attr, &h));

    // 相邻：OK（尾等于头不算重叠）
    void* rightAdj = nullptr; void* leftAdj = nullptr;
    HcclMem r = MakeMem((void*)0x30C8, 32, HCCL_MEM_TYPE_DEVICE);     // 0x3000+200=0x30C8
    HcclMem l = MakeMem((void*)0x2F00, 0x100, HCCL_MEM_TYPE_DEVICE);  // [0x2F00, 0x3000)
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opA", &r, attr, &rightAdj));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opA", &l, attr, &leftAdj));

    // 清理：三个 raw 都各自解绑一次
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "opA", rightAdj));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "opA", leftAdj));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "opA", base));  
}

/*=========================================================
 * 4) 不同 tag 允许交叠：句柄可能不同（不要求相同）
 *=========================================================*/
TEST_F(HcclCommRegMemTest, RegMem_DifferentTags_Overlap_Allowed)
{
    HcclRegMemAttr attr = BindShare(true);

    // 在 opA 下注册
    void* hA = nullptr;
    HcclMem mA = MakeMem((void*)0x5000, 256, HCCL_MEM_TYPE_DEVICE);
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opA", &mA, attr, &hA));
    ASSERT_NE(hA, nullptr);

    // 在 opB 下注册一个与上面重叠的区间 —— 应该允许
    void* hB = nullptr;
    HcclMem mB = MakeMem((void*)0x5100, 128, HCCL_MEM_TYPE_DEVICE); // 与 [0x5000,0x5100) 有交集
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "opB", &mB, attr, &hB));
    ASSERT_NE(hB, nullptr);

    // 不同 tag 分表管理，raw 可能不同；不做相等断言
    // 分别解绑各自的 raw
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "opA", hA));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "opB", hB));
}

/*=========================================================
 * 5) 不同地址多次注册：应各自成功
 *=========================================================*/
TEST_F(HcclCommRegMemTest, RegMem_DifferentAddrs)
{
    HcclRegMemAttr attr = BindShare(false);
    void* h1 = nullptr; void* h2 = nullptr; void* h3 = nullptr;

    HcclMem m1 = MakeMem((void*)0x6000, 128, HCCL_MEM_TYPE_DEVICE);
    HcclMem m2 = MakeMem((void*)0x6100, 128, HCCL_MEM_TYPE_DEVICE); // 相邻不重叠
    HcclMem m3 = MakeMem((void*)0x6200, 256, HCCL_MEM_TYPE_DEVICE);

    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "op1", &m1, attr, &h1));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "op2", &m2, attr, &h2));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommRegMem(comm, "op3", &m3, attr, &h3));

    EXPECT_NE(h1, nullptr);
    EXPECT_NE(h2, nullptr);
    EXPECT_NE(h3, nullptr);

    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "op1", h1));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "op2", h2));
    EXPECT_EQ(HCCL_SUCCESS, HcclCommUnregMem(comm, "op3", h3));
}

/*============================
 * 6) Dereg 参数校验
 *============================*/
TEST_F(HcclCommRegMemTest, Dereg_ParamCheck)
{
    EXPECT_EQ(HCCL_E_PARA, HcclCommUnregMem(nullptr, "opX", (void*)0x1234)); // comm null
    EXPECT_EQ(HCCL_E_PARA, HcclCommUnregMem(comm, nullptr, (void*)0x1234));  // memTag null
    EXPECT_EQ(HCCL_E_PARA, HcclCommUnregMem(comm, "", (void*)0x1234));       // memTag empty
    EXPECT_EQ(HCCL_E_PARA, HcclCommUnregMem(comm, "opX", nullptr));          // handle null
}