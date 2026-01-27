#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "transport_base_pub.h"
#include "comm_base_pub.h"
#include "alg_template_register.h"
#include "sal.h"
#include "reducer_pub.h"
#include "sender_pub.h"
using namespace std;
using namespace hccl;

class BcastHDInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--BcastHDInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--BcastHDInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher BcastHDInterTest::dispatcherPtr = nullptr;
DispatcherPub *BcastHDInterTest::dispatcher = nullptr;

#if 1
TEST_F(BcastHDInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(BcastHDInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_00_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);

    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(0);

    DeviceMem input =  DeviceMem::alloc(0);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 0, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// root= 0， ranksize =3 ，rank= 0, root block内的操作，root发送
TEST_F(BcastHDInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_01_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// root= 0， ranksize =3 ，rank= 2， 低于root所在block的场景，不存在高于root所在block
TEST_F(BcastHDInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_02_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// root= 2， ranksize =3 ，rank= 2， rootblock向低阶发送数据
TEST_F(BcastHDInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_03_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// root= 2， ranksize =3 ，rank= 1， rootblock向高阶发送数据
TEST_F(BcastHDInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_04_inter";

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// root= 0， ranksize =3 ，rank= 1， 本rank从rootrank在bcast阶段接收数据
TEST_F(BcastHDInterTest, run_async_05)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_05_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// 本rank 高于root所在block，并且本block还存在更高阶，ranksize = 7， root=6， rank=5
TEST_F(BcastHDInterTest, run_async_06)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_06_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(7);

    for (int i = 0; i < 7; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 6);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(5, 7, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// 本rank 低于root所在block，并且本block还存在更低阶，ranksize = 7， root = 0， rank=4

TEST_F(BcastHDInterTest, run_async_07)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_07_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(7);

    for (int i = 0; i < 7; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_BROADCAST_RECURSIVE_HD, dispatcher);

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(4, 7, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
