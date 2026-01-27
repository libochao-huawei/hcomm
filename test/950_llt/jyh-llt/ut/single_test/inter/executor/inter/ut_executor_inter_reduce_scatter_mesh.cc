#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "reduce_scatter_mesh_pub.h"
#include "alg_template_register.h"
#include "sal.h"
#include "reducer_pub.h"
#include "sender_pub.h"
using namespace std;
using namespace hccl;

class ReduceScatterMeshInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ReduceScatterMeshInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ReduceScatterMeshInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher ReduceScatterMeshInterTest::dispatcherPtr = nullptr;
DispatcherPub *ReduceScatterMeshInterTest::dispatcher = nullptr;

class CommRSMeshTest : public CommBase
{
public:
    explicit CommRSMeshTest(const std::string& collectiveId,
                        const s32 userRank,
                        const s32 user_rank_size,
                        const s32 rank,
                        const s32 rank_size,
                        const TopoType topoFlag,
                        HcclDispatcher dispatcher,
                        std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
                        const IntraExchanger& exchanger,
                        const std::vector<RankInfo> para_vector,
                        const DeviceMem &inputMem, const DeviceMem &outputMem);
    virtual ~CommRSMeshTest();

    HcclResult Init() override;

protected:
    std::vector<std::shared_ptr<Transport> > link_info_mesh_;  // 当前rank与其他rank对应的link信息
    HcclResult CreateLinks() override;
    DispatcherPub *dispatcher;
};


CommRSMeshTest::CommRSMeshTest(const std::string& collectiveId,
                       const s32 userRank,
                       const s32 user_rank_size,
                       const s32 rank,
                       const s32 rank_size,
                       const TopoType topoFlag,
                       HcclDispatcher dispatcher,
                       std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
                       const IntraExchanger& exchanger,
                       const std::vector<RankInfo> para_vector,
                       const DeviceMem &inputMem, const DeviceMem &outputMem)
    : CommBase(collectiveId, userRank, user_rank_size, rank, rank_size, para_vector, topoFlag, dispatcher, nullptr, netDevCtxMap, exchanger, inputMem, outputMem, true, nullptr, 0),
      dispatcher(reinterpret_cast<DispatcherPub*>(dispatcher))
{
}

CommRSMeshTest::~CommRSMeshTest()
{

}

HcclResult CommRSMeshTest::Init()
{
    SetRankMap();
    CreateLinks();
    return HCCL_SUCCESS;
}

HcclResult CommRSMeshTest::CreateLinks()
{
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    for (int i = 0; i < userRankSize_; i++)
    {
        link_info_mesh_[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link_info_mesh_[i]->Init();
    }

    return HCCL_SUCCESS;
}

TEST_F(ReduceScatterMeshInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";

    std::vector<RankInfo> para_vector(5);

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    ret = tempAlg->Prepare((u64)0UL, (u32)0UL);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#if 1
// rank_size == 3
TEST_F(ReduceScatterMeshInterTest, run_async_00)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_00_inter";

    std::vector<RankInfo> para_vector(3);
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
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0UL, (u32)0UL));

    DeviceMem output =  DeviceMem::alloc(1024*1024*50);
    DeviceMem input =  DeviceMem::alloc(1024*1024*50);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 1024, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
// rank_size == 1
TEST_F(ReduceScatterMeshInterTest, run_async_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_01";

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

    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0UL, (u32)0UL));

    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterMeshInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_02";


    std::vector<RankInfo> para_vector(3);
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

    std::unique_ptr<AlgTemplateBase> executor1 = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, executor1->Prepare((u64)0UL, (u32)0UL));

    DeviceMem output =  DeviceMem::alloc(384);
    DeviceMem input =  DeviceMem::alloc(384);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = executor1->Prepare(input, output,  output,128, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = executor1->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterMeshInterTest, run_async_03)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id_inter = "test_run_async_03";
    std::vector<RankInfo> para_vector(2);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(2);
    for (int i = 0; i < 2; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }
    std::unique_ptr<AlgTemplateBase> executor1 = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, executor1->Prepare((u64)0UL, (u32)0UL));
    DeviceMem output =  DeviceMem::alloc(129 *2);
    DeviceMem input =  DeviceMem::alloc(129*2);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = executor1->Prepare(input, output, 129, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = executor1->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterMeshInterTest, run_async_04)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test_run_async_04_combine";
     std::string collect_id_inter = "test_run_async_3_mesh";
    std::vector<RankInfo> para_vector(3);
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
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0UL, (u32)0UL));
    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Slice> slice;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 0;
    slice2.offset =90;
    Slice slice3;
    slice3.size = 100;
    slice3.offset = 190;
    slice.push_back(slice1);
    slice.push_back(slice2);
    slice.push_back(slice3);
    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0, slice);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(1, 2, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterMeshInterTest, run_async_05)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id_inter = "test_input_equal_output";
    std::vector<RankInfo> para_vector(3);
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
     DeviceMem input =  DeviceMem::alloc(300);
    DeviceMem output =  DeviceMem::alloc(300);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0UL, (u32)0UL));
    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif
#if 1
TEST_F(ReduceScatterMeshInterTest, run_async_06)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id_inter = "test_run_async_06";
    std::vector<RankInfo> para_vector(5);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(5);
    for (int i = 0; i < 5; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }
    DeviceMem output =  DeviceMem::alloc(640);
     DeviceMem input =  DeviceMem::alloc(128);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCESCATTER_MESH, dispatcher);
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0UL, (u32)0UL));
    ret = tempAlg->Prepare(input, output, 300, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_SUM, 0 );
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = tempAlg->RunAsync(0, 5, link);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
#endif



