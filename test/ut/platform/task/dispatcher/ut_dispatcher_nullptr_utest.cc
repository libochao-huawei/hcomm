/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "dispatcher_pub.h"
#include "hccl_common.h"
#include "llt_hccl_stub_mc2.h"

using namespace std;
using namespace hccl;

class DispatcherNullptr_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "DispatcherNullptr_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "DispatcherNullptr_UT TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "DispatcherNullptr_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "DispatcherNullptr_UT Test TearDown" << std::endl;
    }
};

// TbeReduceAsync 测试用例
TEST_F(DispatcherNullptr_UT, ut_TbeReduceAsync_src1_nullptr)
{
    void *src1 = nullptr;
    void *src2 = reinterpret_cast<void *>(0x2000);
    u64 count = 1024;
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = reinterpret_cast<void *>(0x3000);

    DispatcherPub *dispatcher = new (std::nothrow) DispatcherPub(1);
    HcclResult ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherNullptr_UT, ut_TbeReduceAsync_src2_nullptr)
{
    void *src1 = reinterpret_cast<void *>(0x1000);
    void *src2 = nullptr;
    u64 count = 1024;
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = reinterpret_cast<void *>(0x3000);

    DispatcherPub *dispatcher = new (std::nothrow) DispatcherPub(1);
    HcclResult ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherNullptr_UT, ut_TbeReduceAsync_dst_nullptr)
{
    void *src1 = reinterpret_cast<void *>(0x1000);
    void *src2 = reinterpret_cast<void *>(0x2000);
    u64 count = 1024;
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = nullptr;

    DispatcherPub *dispatcher = new (std::nothrow) DispatcherPub(1);
    HcclResult ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherNullptr_UT, ut_TbeReduceAsync_count_zero)
{
    void *src1 = reinterpret_cast<void *>(0x1000);
    void *src2 = reinterpret_cast<void *>(0x2000);
    u64 count = 0;
    HcclDataType datatype = HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HCCL_REDUCE_SUM;
    Stream stream;
    void *dst = reinterpret_cast<void *>(0x3000);

    DispatcherPub *dispatcher = new (std::nothrow) DispatcherPub(1);
    HcclResult ret = dispatcher->TbeReduceAsync(src1, src2, count, datatype, redOp, stream, dst);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete dispatcher;
}

// SetHostNicTcpSendThreadPara 测试用例
TEST_F(DispatcherNullptr_UT, ut_SetHostNicTcpSendThreadPara_fnData_nullptr)
{
    void *fnData = nullptr;

    DispatcherPub *dispatcher = new (std::nothrow) DispatcherPub(1);
    HcclResult ret = dispatcher->SetHostNicTcpSendThreadPara(fnData);
    EXPECT_EQ(ret, HCCL_E_PTR);
    delete dispatcher;
}

TEST_F(DispatcherNullptr_UT, ut_SetHostNicTcpSendThreadPara_normal)
{
    void *fnData = reinterpret_cast<void *>(0x1000);

    DispatcherPub *dispatcher = new (std::nothrow) DispatcherPub(1);
    HcclResult ret = dispatcher->SetHostNicTcpSendThreadPara(fnData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete dispatcher;
}