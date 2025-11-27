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
#include <stdio.h>
#define private public
#define protected public
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "all_reduce_doubling_pub.h"
#undef private
#undef protected

#include "alg_template_register.h"

using namespace std;
using namespace hccl;

class StubTransportBase : public TransportBase
{
public:
    StubTransportBase(const HcclDispatcher &dispatcher,
    MachinePara &machinePara,
    std::chrono::milliseconds timeout, void *remotePtr)
    : TransportBase(reinterpret_cast<DispatcherPub*>(dispatcher), nullptr, machinePara, timeout), remotePtr_(remotePtr)
    {
    }

    HcclResult GetRemoteMem(UserMemType memType, void **remotePtr) {
        *remotePtr = remotePtr_;
        return HCCL_SUCCESS;
    }
private:
    void *remotePtr_;
};
class AllReduceDoublingInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllReduceDoublingInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllReduceDoublingInterTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }

    static HcclDispatcher dispatcherPtr;
    static DispatcherPub *dispatcher;
};
HcclDispatcher AllReduceDoublingInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllReduceDoublingInterTest::dispatcher = nullptr;

#if 1
TEST_F(AllReduceDoublingInterTest, destructor_D0)
{
    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);
    u64 reduceAttrBitMap = 0x01;
    AllReduceDoubling* tempAlg = new AllReduceDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    delete tempAlg;
}
#endif

void run_async_test_allreduce_doubling(hccl::DispatcherPub *dispatcher, u32 rank, u32 rankSize, s32 nLinks = -1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(128);
    DeviceMem input =  DeviceMem::alloc(128);
    nLinks = (nLinks == -1) ? rankSize : nLinks;
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(nLinks);
    for (int i = 0; i < nLinks; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }
    u64 reduceAttrBitMap = 0x01;
    AllReduceDoubling* tempAlg = new AllReduceDoubling(dispatcher);
    tempAlg->Prepare(reduceAttrBitMap);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 32, HCCL_DATA_TYPE_FP32, stream, HcclReduceOp::HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    if(nLinks == rankSize) {
        EXPECT_EQ(ret, HCCL_SUCCESS);
    } else {
        EXPECT_NE(ret, HCCL_SUCCESS);
    }

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

#if 1
//对应代码中，ranksize ==1
TEST_F(AllReduceDoublingInterTest, run_async_00)
{
    run_async_test_allreduce_doubling(dispatcher, /*rank=*/0, /*rankSize=*/4);
}
#endif

TEST_F(AllReduceDoublingInterTest, AllReduceDoubling_Constructor)
{
    u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_DOUBLING, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceDoubling *alg = dynamic_cast<AllReduceDoubling *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}
