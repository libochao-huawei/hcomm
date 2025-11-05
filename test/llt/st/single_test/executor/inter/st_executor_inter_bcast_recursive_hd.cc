/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

clude "bcast_recursive_halvingdoubling_pub.h"
#include "alg_template_register.h"
#include "sal.h"
#include "transport_base_pub.h"

using namespace std;
using namespace hccl;

class BcastRecursiveHDInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--BcastRecursiveHDInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--BcastRecursiveHDInterTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
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
HcclDispatcher BcastRecursiveHDInterTest::dispatcherPtr = nullptr;
DispatcherPub *BcastRecursiveHDInterTest::dispatcher = nullptr;

#if 1
TEST_F(BcastRecursiveHDInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif


#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > links;
    links.resize(1);

    for (int i = 0; i < 1; i++)
    {
        links[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        links[i]->Init();
    }


    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    s32 count = 1;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    DeviceMem output =  DeviceMem::alloc(dataBytes);

    DeviceMem input =  DeviceMem::alloc(dataBytes);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);


    s32 root = 0;
    s32 rank = 0;
    s32 rankSize = 1;

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, links);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 0;
    s32 rank = 0;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 1;
    s32 rank = 0;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 12;
    s32 rank = 0;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 0;
    s32 rank = 1;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_05)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 1;
    s32 rank = 1;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_06)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 1;
    s32 rank = 5;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_07)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 5;
    s32 rank = 1;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_08)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 5;
    s32 rank = 11;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(BcastRecursiveHDInterTest, run_async_09)
{
    s32 ret = HCCL_SUCCESS;

    s32 count = 9;
    s32 bytesPerData = 1;
    s32 dataBytes = count*bytesPerData;

    s32 root = 0;
    s32 rank = 2;
    s32 rankSize = 13;

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector< std::shared_ptr<Transport> > link;
    link.resize(rankSize);

    for (int i = 0; i < rankSize; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(dataBytes);
    DeviceMem input =  DeviceMem::alloc(dataBytes);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, count, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, root);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(rank, rankSize, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif


