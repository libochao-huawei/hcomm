#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#include "comm_base_pub.h"

#define private public
#include "gather_mesh_pub.h"
#undef private

#include "sal.h"
#include "reducer_pub.h"
#include "sender_pub.h"

using namespace std;
using namespace hccl;

class GatherMeshInterTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--GatherMeshInterTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--GatherMeshInterTest TearDown--\033[0m" << std::endl;
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
HcclDispatcher GatherMeshInterTest::dispatcherPtr = nullptr;
DispatcherPub *GatherMeshInterTest::dispatcher = nullptr;

class CommGatherMeshTest : public CommBase
{
public:
    explicit CommGatherMeshTest(const std::string& collectiveId,
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
    virtual ~CommGatherMeshTest();

    HcclResult Init() override;

protected:
    std::vector<std::shared_ptr<Transport> > link_info_mesh_;  // 当前rank与其他rank对应的link信息

    HcclResult CreateLinks() override;
    DispatcherPub *dispatcher;
};

CommGatherMeshTest::CommGatherMeshTest(const std::string& collectiveId,
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

CommGatherMeshTest::~CommGatherMeshTest()
{

}

HcclResult CommGatherMeshTest::Init()
{
    SetRankMap();
    CreateLinks();
    return HCCL_SUCCESS;
}

HcclResult CommGatherMeshTest::CreateLinks()
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
TEST_F(GatherMeshInterTest, destructor_D0)
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
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(3);

    AlgTemplateBase* tempAlg = new GatherMesh(dispatcher);
    ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

#if 1
TEST_F(GatherMeshInterTest, run_async_0)
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
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(3);

    AlgTemplateBase* tempAlg = new GatherMesh(dispatcher);
    ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem output =  DeviceMem::alloc(384);

    DeviceMem input =  DeviceMem::alloc(384);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output,  output, 384, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(2, 3,  link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

#if 1
// rank_size == 1
TEST_F(GatherMeshInterTest, run_async_01)
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

    std::vector<Stream> mesh_streams;
    mesh_streams.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(3);

    AlgTemplateBase* tempAlg = new GatherMesh(dispatcher);
    ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem output =  DeviceMem::alloc(9);
    DeviceMem input =  DeviceMem::alloc(9);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
#endif

#if 1
TEST_F(GatherMeshInterTest, run_async_02)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "test_run_async_02_combine";
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
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(3);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(3);

    AlgTemplateBase* tempAlg = new GatherMesh(dispatcher);
    ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
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
    delete tempAlg;
}
#endif