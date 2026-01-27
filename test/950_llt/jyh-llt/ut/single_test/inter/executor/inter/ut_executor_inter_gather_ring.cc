#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "gather_ring_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "sender_pub.h"

using namespace std;
using namespace hccl;

class GatherInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--GatherInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--GatherInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher GatherInterTest::dispatcherPtr = nullptr;
DispatcherPub *GatherInterTest::dispatcher = nullptr;

TEST_F(GatherInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    GatherRing* tempAlg = new GatherRing(dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}

//ranksize ==1
TEST_F(GatherInterTest, run_async_ranksize1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_ranksize1";

    std::vector<RankInfo> para_vector(1);
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


    GatherRing* tempAlg = new GatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(1);

    DeviceMem input =  DeviceMem::alloc(1);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output,  output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}



// 带入slicevector， 4个rank， root next节点
TEST_F(GatherInterTest, run_async_root_next)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_root_next";

    std::vector<RankInfo> para_vector(1);
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


    GatherRing* tempAlg = new GatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(400);
    DeviceMem input =  DeviceMem::alloc(400);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 100;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 100;
    slice2.offset =100;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 200;
    Slice slice4;
    slice4.size = 100;
    slice4.offset = 300;

    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    slice.push_back(slice4);
    ret = tempAlg->Prepare(input, input, output,400, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1,4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

// 4个rank， root 节点
TEST_F(GatherInterTest, run_async_root)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_root_next";

    std::vector<RankInfo> para_vector(1);
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


    GatherRing* tempAlg = new GatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(400);
    DeviceMem input =  DeviceMem::alloc(400);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 100;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 100;
    slice2.offset =100;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 200;
    Slice slice4;
    slice4.size = 100;
    slice4.offset = 300;

    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    slice.push_back(slice4);
    ret = tempAlg->Prepare(input, input, output,400, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(0, 4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
//4个rank。others node
TEST_F(GatherInterTest, run_async_others)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_others";

    std::vector<RankInfo> para_vector(1);
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


    GatherRing* tempAlg = new GatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(400);
    DeviceMem input =  DeviceMem::alloc(400);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 100;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 100;
    slice2.offset =100;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 200;
    Slice slice4;
    slice4.size = 100;
    slice4.offset = 300;

    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    slice.push_back(slice4);
    ret = tempAlg->Prepare(input, input, output,400, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(3, 4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
// 4个节点其他rank

// 4个rank。slice为空
TEST_F(GatherInterTest, run_async_slice_null)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_slicenull";

    std::vector<RankInfo> para_vector(1);
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


    GatherRing* tempAlg = new GatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(400);
    DeviceMem input =  DeviceMem::alloc(100);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;

    ret = tempAlg->Prepare(input, input, output,100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(3, 4, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}



TEST_F(GatherInterTest, run_async_linkerr)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_linkerr";

    std::vector<RankInfo> para_vector(1);
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

    GatherRing* tempAlg = new GatherRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> slice;

    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(2, 4, link);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

