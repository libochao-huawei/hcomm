#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#define private public
#define protected public
#include "alg_template_base_pub.h"
#include "reduce_scatter_ring_pub.h"
#include "alg_template_register.h"
#undef private
#undef protected
#include "sal.h"
#include "comm_ring_pub.h"
#include "reducer_pub.h"
#include "sender_pub.h"

using namespace std;
using namespace hccl;

class StubTransportBase : public TransportBase
{
public:
    StubTransportBase(const HcclDispatcher dispatcher,
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

class ReduceScatterInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ReduceScatterInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ReduceScatterInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher ReduceScatterInterTest::dispatcherPtr = nullptr;
DispatcherPub *ReduceScatterInterTest::dispatcher = nullptr;

#if 1
TEST_F(ReduceScatterInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(1);

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    ret = tempAlg->Prepare((u64)0ULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
//用例中comminter类型需要改变为commcombine的对应类型
TEST_F(ReduceScatterInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_00_combine";

    std::vector<RankInfo> para_vector(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(1);

    DeviceMem input =  DeviceMem::alloc(1);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);

    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_01_combine";

    std::vector<RankInfo> para_vector(2);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(3);

    DeviceMem input =  DeviceMem::alloc(6);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(2);

    for (int i = 0; i < 2; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test_run_async_02_combine";

    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(3);
    DeviceMem input =  DeviceMem::alloc(9);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
//chunksize更新
TEST_F(ReduceScatterInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test_run_async_03_combine";

    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(3);
    DeviceMem input =  DeviceMem::alloc(9);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // tx_mem 和rx_mem 不能为空，需要在link中申请txmem，和rxmem
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test_run_async_04_combine";
    std::vector<RankInfo> para_vector(3);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 80;
    slice2.offset =90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, input, output, 300, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterInterTest, run_async_05)
{
     s32 ret = HCCL_SUCCESS;
    std::string collect_id = "run_async_05";

    std::vector<RankInfo> para_vector(1);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    DeviceMem input =  DeviceMem::alloc(9);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    ret = tempAlg->Prepare((u64)0ULL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->Prepare(input, input, 3, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterInterTest, run_async_06)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test_run_async_06_combine";
    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 80;
    slice2.offset =90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, input, output, 300, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterInterTest, run_async_07)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test_run_async_07_combine";
    std::vector<RankInfo> para_vector(3);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    DeviceMem output =  DeviceMem::alloc(100);
    DeviceMem input =  DeviceMem::alloc(300);
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);

    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) StubTransportBase(
            dispatcher, machinePara, timeout, input.ptr())));
        link[i]->Init();
    }

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_RING, dispatcher);
    EXPECT_EQ(HcclResult::HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL));
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 80;
    slice2.offset = 90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, output, output, 100, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
#endif
