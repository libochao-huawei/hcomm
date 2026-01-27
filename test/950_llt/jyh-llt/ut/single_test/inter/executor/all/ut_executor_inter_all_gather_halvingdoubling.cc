#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "all_gather_halving_doubling_pub.h"
#include "sal.h"
#include "reducer_pub.h"
#include "sender_pub.h"
using namespace std;
using namespace hccl;

class AllGatherHDInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--AllGatherHDInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--AllGatherHDInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher AllGatherHDInterTest::dispatcherPtr = nullptr;
DispatcherPub *AllGatherHDInterTest::dispatcher = nullptr;

#if 1
TEST_F(AllGatherHDInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    AllGatherHalvingDoubling* tempAlg = new AllGatherHalvingDoubling(dispatcher);
    ret = tempAlg->Prepare(0, UserMemType::OUTPUT_MEM, UserMemType::INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(AllGatherHDInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_inter";
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
    AlgTemplateBase* tempAlg = new AllGatherHalvingDoubling(dispatcher);
    ret = tempAlg->Prepare(1, UserMemType::OUTPUT_MEM, UserMemType::INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output = DeviceMem::alloc(10);
    DeviceMem input = DeviceMem::alloc(10);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 0, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

#if 1
TEST_F(AllGatherHDInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_01_inter";

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(2);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    for (int i = 0; i < 2; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));

    }

    AlgTemplateBase* tempAlg = new AllGatherHalvingDoubling(dispatcher);
    ret = tempAlg->Prepare(2, UserMemType::OUTPUT_MEM, UserMemType::INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem output =  DeviceMem::alloc(0);

    DeviceMem input =  DeviceMem::alloc(0);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 0, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// 高阶为空，低阶不为空，数据不够均分
TEST_F(AllGatherHDInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_03_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(4);

    for (int i = 0; i < 4; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();

    }

    AlgTemplateBase* tempAlg = new AllGatherHalvingDoubling(dispatcher);
    ret = tempAlg->Prepare(4, UserMemType::OUTPUT_MEM, UserMemType::INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(9*4);
    DeviceMem input =  DeviceMem::alloc(9*4);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_FP32, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(AllGatherHDInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_04_inter";
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(8);

    for (int i = 0; i < 8; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AlgTemplateBase* tempAlg = new AllGatherHalvingDoubling(dispatcher);
    ret = tempAlg->Prepare(8, UserMemType::OUTPUT_MEM, UserMemType::INPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(7);

    DeviceMem input =  DeviceMem::alloc(7);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 7, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(2, 8, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif



