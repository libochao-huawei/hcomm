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
#include "hccl_api.h"
 
// using namespace std;
// using namespace hccl;
 
constexpr const char* RANKTABLE_FILE_NAME = "./ut_independent_op_test.json";
 
class HcclCommEngineCtxTest : public BaseInit {
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
 
// HcclMem构造
static inline HcclMem MakeMem(void* addr, uint64_t size, HcclMemType type)
{
    HcclMem m{};
    m.addr = addr;
    m.size = size;
    m.type = type;
    return m;
}

// HcclMem相等
static inline bool IsMemEqual(const HcclMem& lhm, const HcclMem& rhm)
{
    return lhm.type == rhm.type && lhm.addr == rhm.addr && lhm.size == rhm.size;
}

// 创建入参异常校验：comm opTag engineCtx null判断 engineCtx->size 0判断 设备类型支持判断 tag最大长度判断
// 获取入参异常校验：comm opTag null判断 tag最大长度判断
// 销毁入参异常校验：comm engineCtx null判断
TEST_F(HcclCommEngineCtxTest, Ut_CommEngineCtx_When_Error_Input_Expect_Return_ERROR)
{
    // create: comm null
    HcclMem engineCtx = MakeMem(nullptr, 16, HCCL_MEM_TYPE_NUM);
    HcclResult ret = CommCreateEngineCtx(nullptr, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // create: opTag null
    ret = CommCreateEngineCtx(comm, nullptr, CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // create: engineCtx null
    ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // create: engine unsupport
    ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_CCU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // create: engineCtx->size 0
    HcclMem engineCtx0 = MakeMem(nullptr, 0, HCCL_MEM_TYPE_NUM);
    ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx0);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // create: opTag too long
    char opTag[HCCL_OP_TAG_LEN_MAX+2];
    memset(opTag, 'A', HCCL_OP_TAG_LEN_MAX+1);
    opTag[HCCL_OP_TAG_LEN_MAX+1] = '\0';
    ret = CommCreateEngineCtx(comm, opTag, CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // get: comm null
    ret = CommGetEngineCtx(nullptr, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // get: opTag null
    ret = CommGetEngineCtx(comm, nullptr, CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // get: opTag too long
    ret = CommGetEngineCtx(comm, opTag, CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // destroy: comm null
    ret = HcommEngineCtxDestroy(nullptr, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // destroy: engineCtx null
    ret = HcommEngineCtxDestroy(comm, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);

}

// 重复创建异常校验 HCCL_E_PARA 
TEST_F(HcclCommEngineCtxTest, Ut_CommCreateEngineCtx_When_Duplicate_Creation_Expect_Return_EPARA)
{
    HcclMem engineCtx = MakeMem(nullptr, 16, HCCL_MEM_TYPE_NUM);
    HcclResult ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(engineCtx.addr, nullptr);
    EXPECT_EQ(engineCtx.type, HCCL_MEM_TYPE_HOST);

    ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = HcommEngineCtxDestroy(comm, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 未创建时获取失败校验 HCCL_SUCCESS
TEST_F(HcclCommEngineCtxTest, Ut_CommGetEngineCtx_When_Uncreated_Expect_Return_SUCCESS)
{
    HcclMem* engineCtxPtr;
    HcclResult ret = CommGetEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, engineCtxPtr);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// 获取时设备不支持或不匹配校验 HCCL_E_PARA
TEST_F(HcclCommEngineCtxTest, Ut_CommGetEngineCtx_When_Error_Engine_Expect_Return_EPARA)
{
    HcclMem engineCtx = MakeMem(nullptr, 16, HCCL_MEM_TYPE_NUM);
    HcclResult ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(engineCtx.addr, nullptr);
    EXPECT_EQ(engineCtx.type, HCCL_MEM_TYPE_HOST);

    HcclMem* engineCtxPtr;
    ret = CommGetEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_AICPU, engineCtxPtr);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = CommGetEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_CCU, engineCtxPtr);
    EXPECT_EQ(ret, HCCL_E_PARA);

    ret = HcommEngineCtxDestroy(comm, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 未创建时销毁失败校验 HCCL_E_PARA
TEST_F(HcclCommEngineCtxTest, Ut_HcommEngineCtxDestroy_When_Uncreated_Expect_Return_EPARA)
{
    HcclMem engineCtx = MakeMem((void*)0x1, 16, HCCL_MEM_TYPE_HOST);
    HcclResult ret = HcommEngineCtxDestroy(comm, &engineCtx);
    EXPECT_EQ(ret, HCCL_E_PARA);
}


// 正常申请、获取和销毁host侧Context HCCL_SUCCESS
TEST_F(HcclCommEngineCtxTest, Ut_CommEngineCtx_When_Host_Expect_Return_SUCCESS)
{
    HcclMem engineCtx = MakeMem(nullptr, 16, HCCL_MEM_TYPE_NUM);
    HcclResult ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(engineCtx.addr, nullptr);
    EXPECT_EQ(engineCtx.type, HCCL_MEM_TYPE_HOST);

    HcclMem engineCtxGet;
    ret = CommGetEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_HOSTCPU, &engineCtxGet);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(IsMemEqual(engineCtx, engineCtxGet), true);

    ret = HcommEngineCtxDestroy(comm, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 正常申请、获取和销毁device侧Context HCCL_SUCCESS
TEST_F(HcclCommEngineCtxTest, Ut_CommEngineCtx_When_Device_Expect_Return_SUCCESS)
{
    HcclMem engineCtx = MakeMem(nullptr, 16, HCCL_MEM_TYPE_NUM);
    HcclResult ret = CommCreateEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_AICPU, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(engineCtx.addr, nullptr);
    EXPECT_EQ(engineCtx.type, HCCL_MEM_TYPE_DEVICE);

    HcclMem engineCtxGet;
    ret = CommGetEngineCtx(comm, "opTag", CommEngine::COMM_ENGINE_AICPU, &engineCtxGet);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(IsMemEqual(engineCtx, engineCtxGet), true);

    ret = HcommEngineCtxDestroy(comm, &engineCtx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

