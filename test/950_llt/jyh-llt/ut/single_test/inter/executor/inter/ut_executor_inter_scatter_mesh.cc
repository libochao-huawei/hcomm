#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"
#include "scatter_mesh_pub.h"
#include "sal.h"
#include "comm_ring_pub.h"
#include "reducer_pub.h"
#include "sender_pub.h"

using namespace std;
using namespace hccl;

class ScatterMeshInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ScatterMeshInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ScatterMeshInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher ScatterMeshInterTest::dispatcherPtr = nullptr;
DispatcherPub *ScatterMeshInterTest::dispatcher = nullptr;

class CommScatterMeshTest : public CommBase
{
public:
    explicit CommScatterMeshTest(const std::string& collectiveId,
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
    virtual ~CommScatterMeshTest();

    HcclResult Init() override;

protected:
    std::vector<std::shared_ptr<Transport> > link_info_mesh_;  // 当前rank与其他rank对应的link信息

    HcclResult CreateLinks() override;
    DispatcherPub *dispatcher = nullptr;
};

CommScatterMeshTest::CommScatterMeshTest(const std::string& collectiveId,
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
    : CommBase(collectiveId, userRank, user_rank_size, rank, rank_size, para_vector, topoFlag, dispatcher, nullptr, netDevCtxMap, exchanger, inputMem, outputMem, true),
      dispatcher(reinterpret_cast<DispatcherPub*>(dispatcher))
{

}

CommScatterMeshTest::~CommScatterMeshTest()
{

}

HcclResult CommScatterMeshTest::Init()
{
    SetRankMap();
    CreateLinks();
    return HCCL_SUCCESS;
}

HcclResult CommScatterMeshTest::CreateLinks()
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
#if 1
TEST_F(ScatterMeshInterTest, destructor_D0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "test-destructor";


    std::vector<RankInfo> para_vector(5);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector< std::unique_ptr<Transport> > link;
    link.resize(5);

    for (int i = 0; i < 5; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
    }

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 0;
    u32 interRankSize = 5;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(ScatterMeshInterTest, run_async_0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_0_mesh";

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

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 2;
    u32 interRankSize = 3;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem output =  DeviceMem::alloc(49152);

    DeviceMem input =  DeviceMem::alloc(49152);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output,  output, 49152, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(2, 3,  link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;

}
#endif
#if 1
//rank_size == 1的分支
TEST_F(ScatterMeshInterTest, run_async_1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_1_mesh";
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

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 0;
    u32 interRankSize = 1;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(16384);

    DeviceMem input =  DeviceMem::alloc(16384);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 16384, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(0, 1, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
// chunksize 带入特别大的值，被rxmem替换
TEST_F(ScatterMeshInterTest, run_async_2)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_2_mesh";
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

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 0;
    u32 interRankSize = 3;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(49152);

    DeviceMem input =  DeviceMem::alloc(49152);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 49152, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(0, 3, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
#if 1
TEST_F(ScatterMeshInterTest, run_async_3)
{
    s32 ret = HCCL_SUCCESS;

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

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 2;
    u32 interRankSize = 3;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(49152);

    DeviceMem input =  DeviceMem::alloc(49152);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 49152, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(2, 3, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif
TEST_F(ScatterMeshInterTest, run_async_linkerr_0)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_linkerr";
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

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 2;
    u32 interRankSize = 4;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(512);

    DeviceMem input =  DeviceMem::alloc(512);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 384, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(2, 4, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
TEST_F(ScatterMeshInterTest, run_async_linkerr_1)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "test_run_async_linkerr";
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

    AlgTemplateBase* tempAlg = new ScatterMesh(dispatcher);
    u32 interRank = 2;
    u32 interRankSize = 4;
    ret = tempAlg->Prepare(interRank, interRankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DeviceMem output =  DeviceMem::alloc(512);

    DeviceMem input =  DeviceMem::alloc(512);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 384, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(0, 4, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
