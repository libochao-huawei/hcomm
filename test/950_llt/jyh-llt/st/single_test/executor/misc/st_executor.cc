#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"

#define private public
#define protected public
#include "alg_template_base_pub.h"
#include "all_reduce_ring_pub.h"
#include "all_reduce_chunk_mesh_pub.h"
#include "all_reduce_local_reduce_bcast_pub.h"
#include "all_reduce_local_reduce_pub.h"
#include "all_reduce_mesh_oneshot_pub.h"
#include "all_reduce_mesh_opbase_pub.h"
#include "all_reduce_opbase_pipeline_pub.h"
#include "all_reduce_ahc_broke_pub.h"
#include "all_reduce_ahc_pub.h"
#include "reduce_ring_pub.h"
#include "reduce_nhr_oneshot_pub.h"
#include "all_gather_halving_doubling_pub.h"
#include "all_gather_mesh_atomic.h"
#include "all_gather_mesh_direct.h"
#include "all_gather_mesh_mix_pub.h"
#include "all_gather_mesh_pub.h"
#include "all_gather_recursive_hd_pub.h"
#include "all_gather_ring_pub.h"
#include "all_gather_hd_stage_pub.h"
#include "all_gather_nb_pub.h"
#include "all_gather_nhr_pub.h"
#include "all_gather_nhr_v1_pub.h"
#include "all_gather_pipeline_pub.h"
#include "all_gather_unified_march_pub.h"
#include "multi_root_scatter_ring_pub.h"
#include "scatter_ring_pub.h"
#include "scatter_mesh_pub.h"
#include "scatter_nb_pub.h"
#include "scatter_nhr_pub.h"
#include "allltoall_pipeline_mesh_pairwise_ccl_enough_pub.h"
#include "allltoall_pipeline_mesh_pairwise_ping_pong_pub.h"
#include "gather_mesh_pub.h"
#include "gather_ring_pub.h"
#include "gather_star_pub.h"
#include "all_gather_ring_direct_pub.h"
#undef private
#undef protected
#include "comm_base_pub.h"
#include "reduce_scatter_mesh_pub.h"
#include "alg_template_register.h"
#include "sal.h"

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include "transport_base_pub.h"

using namespace std;
using namespace hccl;

class ExecutorTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        s32 ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr);
        if (ret != HCCL_SUCCESS) return;
        if (dispatcherPtr == nullptr) return;
        dispatcher = reinterpret_cast<DispatcherPub*>(dispatcherPtr);
        std::cout << "\033[36m--ExecutorTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        if (dispatcherPtr != nullptr) {
            s32 ret = HcclDispatcherDestroy(dispatcherPtr);
            EXPECT_EQ(ret, HCCL_SUCCESS);
            dispatcherPtr = nullptr;
            dispatcher = nullptr;
        }
        std::cout << "\033[36m--ExecutorTest TearDown--\033[0m" << std::endl;
    }

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
HcclDispatcher ExecutorTest::dispatcherPtr = nullptr;
DispatcherPub *ExecutorTest::dispatcher = nullptr;

class CommMeshExecutorTest : public CommBase
{
public:
    explicit CommMeshExecutorTest(const std::string& collectiveId,
                        const s32 userRank,
                        const s32 user_rank_size,
                        const s32 rank,
                        const s32 rank_size,
                        const TopoType topoFlag,
                        HcclDispatcher dispatcher,
                        std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
                        const IntraExchanger &exchanger,
                        const std::vector<RankInfo> para_vector,
                        const DeviceMem& inputMem,const DeviceMem& outputMem);
    virtual ~CommMeshExecutorTest();

    HcclResult Init() override;

protected:
    std::vector<std::shared_ptr<Transport> > link_info_hd_;  // 当前rank与其他rank对应的link信息

    HcclResult CreateLinks() override;
    DispatcherPub *dispatcher;

};

CommMeshExecutorTest::CommMeshExecutorTest(const std::string& collectiveId,
                       const s32 userRank,
                       const s32 user_rank_size,
                       const s32 rank,
                       const s32 rank_size,
                       const TopoType topoFlag,
                       HcclDispatcher dispatcher,
                       std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
                       const IntraExchanger &exchanger,
                       const std::vector<RankInfo> para_vector,
                       const DeviceMem& inputMem,const DeviceMem& outputMem)
    : CommBase(collectiveId, userRank, user_rank_size, rank, rank_size, para_vector, topoFlag, dispatcher, nullptr,
        netDevCtxMap, exchanger, inputMem, outputMem, true), dispatcher(reinterpret_cast<DispatcherPub*>(dispatcher))
{

}

CommMeshExecutorTest::~CommMeshExecutorTest()
{

}

HcclResult CommMeshExecutorTest::Init()
{
    SetRankMap();
    CreateLinks();
    return HCCL_SUCCESS;
}

HcclResult CommMeshExecutorTest::CreateLinks()
{
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;
    for (int i = 0; i < userRankSize_; i++)
    {
        link_info_hd_[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link_info_hd_[i]->Init();
    }

    return HCCL_SUCCESS;
}
//  只带入一个inputmem,slice不为空
TEST_F(ExecutorTest, base_prepare_run_01)
{
    s32 ret = HCCL_SUCCESS;
    s32 data_size;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    DeviceMem input = DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> sliceList;
    Slice slice;
    slice.size = 1;
    slice.offset = 0;
    sliceList.push_back(slice);

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);

    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ret = tempAlg->Prepare(input, input, 1, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_RESERVED, 0, sliceList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->PrepareRunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RunStage stage=RunStage::RUN_PREPARE;
    ret = tempAlg->RunAsyncStaged(0, 1, link, stage);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}
TEST_F(ExecutorTest, base_prepare)
{
    s32 ret = HCCL_SUCCESS;
    s32 data_size;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

    DeviceMem input = DeviceMem::alloc(1);
    DeviceMem output = DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    std::vector<Slice> sliceList;
    Slice slice;
    slice.size = 1;
    slice.offset = 0;
    sliceList.push_back(slice);

    ret = tempAlg->Prepare(input,output, 1, HCCL_DATA_TYPE_RESERVED, stream, HCCL_REDUCE_RESERVED, 0, sliceList);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}


// tempAlg inter 的函数覆盖
TEST_F(ExecutorTest, inter_function)
{
    s32 ret = HCCL_SUCCESS;
    AlgTemplateBase* tempAlg = new AlgTemplateBase(dispatcher);

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
    ret = tempAlg->RunAsync(0,1,link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

   delete tempAlg;
}

// allgather mesh == 1的分支
TEST_F(ExecutorTest, allgather_mesh_01)
{
    s32 ret = HCCL_SUCCESS;
    s32 err = 0;
    std::string collect_id_inter = "st_allgather_mesh_01";


    IntraExchanger exchanger {};
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector<RankInfo> para_vector(1);
    TopoType topoFlag = TopoType::TOPO_TYPE_4P_MESH;
    DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommMeshExecutorTest(collect_id_inter, 0, 1, 0, 1, topoFlag, dispatcher,
        netDevCtxMap, exchanger, para_vector, inputMem, outputMem));

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);

    std::vector<Stream> mesh_streams;
    mesh_streams.resize(1);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(1);

    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(1);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    AlgTemplateBase* tempAlg = new AllGatherMesh(dispatcher);
    ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 10, nullptr, 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem output =  DeviceMem::alloc(10);

    DeviceMem input =  DeviceMem::alloc(10);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    char* test = (char*)input.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        test[i] = 3;
    }

    ret = tempAlg->Prepare(input, output, 10, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(0,1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    char* testout = (char*)output.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        if ( testout[i] = !3)
        {
            err++;
        }
    }

    EXPECT_EQ(err, 0);
    delete tempAlg;
}

// reducescatter mesh == 1的分支
TEST_F(ExecutorTest, reducescatter_mesh_01)
{
    s32 ret = HCCL_SUCCESS;
    s32 err = 0;
    std::string collect_id_inter = "reducescatter_mesh_01";


    IntraExchanger exchanger {};
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector<RankInfo> para_vector(1);
    TopoType topoFlag = TopoType::TOPO_TYPE_4P_MESH;
    DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommMeshExecutorTest(collect_id_inter, 0, 1, 0, 1, topoFlag, dispatcher,
        netDevCtxMap, exchanger, para_vector, inputMem, outputMem));

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
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL, (u32)0UL));

    DeviceMem output =  DeviceMem::alloc(10);

    DeviceMem input =  DeviceMem::alloc(10);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    char* test = (char*)input.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        test[i] = 3;
    }

    ret = tempAlg->Prepare(input, output, 10, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    char* testout = (char*)output.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        if ( testout[i] = !3)
        {
            err++;
        }
    }


    EXPECT_EQ(err, 0);
}

// reducescatter mesh slice == null 并且获取resultcount
TEST_F(ExecutorTest, reducescatter_mesh_02)
{
    s32 ret = HCCL_SUCCESS;
    std::string collect_id = "reducescatter_mesh_02";
    std::string collect_id_inter = "reducescatter_mesh_02";


    IntraExchanger exchanger {};
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector<RankInfo> para_vector(3);
    TopoType topoFlag = TopoType::TOPO_TYPE_4P_MESH;
        DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommMeshExecutorTest(collect_id_inter, 2, 3, 2, 3, topoFlag, dispatcher,
        netDevCtxMap, exchanger, para_vector,inputMem, outputMem));

//    ret =  comm_inner->Init();
 //   EXPECT_EQ(ret, HCCL_SUCCESS);

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
    EXPECT_EQ(HCCL_SUCCESS, tempAlg->Prepare((u64)0ULL, (u32)0UL));
    DeviceMem output =  DeviceMem::alloc(300);
    DeviceMem input =  DeviceMem::alloc(300);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(2, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// scattering 算法， ranksize ==1
TEST_F(ExecutorTest, scatter_ring_01)
{
    s32 ret = HCCL_SUCCESS;
    s32 err = 0;
    std::string collect_id = "scatter_ring_01";


    IntraExchanger exchanger {};
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector<RankInfo> para_vector(1);
    DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommBase(collect_id, 0, 1, 0, 1, para_vector, topoFlag,
        dispatcher, nullptr, netDevCtxMap, exchanger, inputMem, outputMem, true));
    std::vector< std::shared_ptr<Transport> > link;
    link.resize(1);
    for (int i = 0; i < 1; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }


    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(10);

    DeviceMem input =  DeviceMem::alloc(10);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    char* test = (char*)input.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        test[i] = 3;
    }

    ret = tempAlg->Prepare(input, output, 1, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    char* testout = (char*)output.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        if ( testout[i] = !3)
        {
            err++;
        }
    }


    delete tempAlg;
}

// root节点，input != ouput ,slice为空
TEST_F(ExecutorTest, scatter_ring_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id = "scatter_ring_02";

    IntraExchanger exchanger {};
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;


    std::vector<RankInfo> para_vector(1);
    DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    TopoType topoFlag = TopoType::TOPO_TYPE_8P_RING;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommBase(collect_id, 1, 3, 1, 3, para_vector, topoFlag, dispatcher, nullptr,
        netDevCtxMap, exchanger, inputMem, outputMem, true));

    std::vector< std::shared_ptr<Transport> > link;
    link.resize(3);
    for (int i = 0; i < 3; i++)
    {
        link[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link[i]->Init();
    }

    ScatterRing* tempAlg = new ScatterRing(dispatcher);

    DeviceMem output =  DeviceMem::alloc(100);
    DeviceMem input =  DeviceMem::alloc(300);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 100, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
    delete tempAlg;
}

// scatter mesh ranksize == 1
TEST_F(ExecutorTest, scatter_mesh_01)
{
    s32 ret = HCCL_SUCCESS;
    s32 err= 0;
    std::string collect_id_inter = "scatter_mesh_01";


    IntraExchanger exchanger {};
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    std::vector<RankInfo> para_vector(1);
        DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    TopoType topoFlag = TopoType::TOPO_TYPE_4P_MESH;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommMeshExecutorTest(collect_id_inter, 0, 1, 0, 1, topoFlag, dispatcher,
        netDevCtxMap, exchanger, para_vector,inputMem, outputMem));
  //  comm_inner->Init();

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

    DeviceMem output =  DeviceMem::alloc(7);

    DeviceMem input =  DeviceMem::alloc(7);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
   char* test = (char*)input.ptr();

    // input赋值
    for (s32 i = 0; i < 7; i++)
    {
        test[i] = 3;
    }

    ret = tempAlg->Prepare(input, output, 7, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(0, 1, link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);
   char* testout = (char*)output.ptr();

    // input赋值
    for (s32 i = 0; i < 10; i++)
    {
        if ( testout[i] = !3)
        {
            err++;
        }
    }
    delete tempAlg;
}

TEST_F(ExecutorTest, scatter_mesh_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "scatter_mesh_02";
    RaResourceInfo raResourceInfo;

    IntraExchanger exchanger {};
    std::vector<RankInfo> para_vector(3);
    DeviceMem inputMem = DeviceMem::alloc(1024);
    DeviceMem outputMem= DeviceMem::alloc(1024);
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    TopoType topoFlag = TopoType::TOPO_TYPE_4P_MESH;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;
    std::unique_ptr<CommBase> comm_inner(new CommMeshExecutorTest(collect_id_inter, 2, 3, 2, 3, topoFlag, dispatcher,
        netDevCtxMap, exchanger, para_vector,inputMem, outputMem));
   // comm_inner->Init();

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

    DeviceMem output =  DeviceMem::alloc(16384);

    DeviceMem input =  DeviceMem::alloc(49152);

    Stream stream(StreamType::STREAM_TYPE_OFFLINE);

    ret = tempAlg->Prepare(input, output, 49152, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_RESERVED, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    ret = tempAlg->RunAsync(2, 3 , link);  //中间分支如何保证执行?
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret =rt_stream_synchronize(stream.ptr());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    delete tempAlg;
}
// 针对hd算法的测试类
class CommHDExecutorTest : public CommBase
{
public:
    explicit CommHDExecutorTest(const std::string& collectiveId,
                        const s32 userRank,
                        const s32 user_rank_size,
                        const s32 rank,
                        const s32 rank_size,
                        const TopoType topoFlag,
                        HcclDispatcher dispatcher,
                        std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
                        const IntraExchanger& exchanger,
                        const std::vector<RankInfo> para_vector,
                        const DeviceMem& inputMem,const DeviceMem& outputMem);
    virtual ~CommHDExecutorTest();

    HcclResult Init() override;

protected:
   std::vector<std::shared_ptr<Transport> > link_info_hd_;  // 当前rank与其他rank对应的link信息

    HcclResult CreateLinks() override;
    DispatcherPub *dispatcher;
};

CommHDExecutorTest::CommHDExecutorTest(const std::string& collectiveId,
                       const s32 userRank,
                       const s32 user_rank_size,
                       const s32 rank,
                       const s32 rank_size,
                        const TopoType topoFlag,
                       HcclDispatcher dispatcher,
                       std::map<HcclIpAddress, HcclNetDevCtx> &netDevCtxMap,
                       const IntraExchanger& exchanger,
                       const std::vector<RankInfo> para_vector,
                       const DeviceMem& inputMem,const DeviceMem& outputMem)
    : CommBase(collectiveId, userRank, user_rank_size, rank, rank_size, para_vector, topoFlag, dispatcher, nullptr,
        netDevCtxMap, exchanger, inputMem, outputMem, true), dispatcher(reinterpret_cast<DispatcherPub*>(dispatcher))
{
}

CommHDExecutorTest::~CommHDExecutorTest()
{

}

HcclResult CommHDExecutorTest::Init()
{
    SetRankMap();
    CreateLinks();
    return HCCL_SUCCESS;
}

HcclResult CommHDExecutorTest::CreateLinks()
{
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
    const std::string tag;

    for (int i = 0; i < userRankSize_; i++)
    {
        link_info_hd_[i].reset(new(std::nothrow) Transport(new (std::nothrow) TransportBase(
            dispatcher, nullptr, machinePara, timeout)));
        link_info_hd_[i]->Init();
    }

    return HCCL_SUCCESS;
}

TEST_F(ExecutorTest, run_async_00)
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

// root= 0， ranksize =3 ，rank= 0, root block内的操作，root发送
TEST_F(ExecutorTest, bcast_run_01)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_01";

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

// root= 0， ranksize =3 ，rank= 2， 低于root所在block的场景，不存在高于root所在block
TEST_F(ExecutorTest, bcast_run_02)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_02";

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

// root= 2， ranksize =3 ，rank= 2， rootblock向低阶发送数据
TEST_F(ExecutorTest, bcast_run_03)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_03";

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

    ret = tempAlg->RunAsync(2,3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// root= 2， ranksize =3 ，rank= 1， rootblock向高阶发送数据
TEST_F(ExecutorTest, bcast_run_04)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_04";

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

    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// root= 0， ranksize =3 ，rank= 1， 本rank从rootrank在bcast阶段接收数据

TEST_F(ExecutorTest, bcast_run_05)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_05";

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

    ret = tempAlg->Prepare(input, output, 9, HCCL_DATA_TYPE_INT8, stream, HCCL_REDUCE_SUM, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->RunAsync(1, 3, link);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 本rank 高于root所在block，并且本block还存在更高阶，ranksize = 7， root=6， rank=5
TEST_F(ExecutorTest, bcast_run_06)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_06";

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

// 本rank 低于root所在block，并且本block还存在更低阶，ranksize = 7， root = 0， rank=4

TEST_F(ExecutorTest, bcast_run_07)
{
    s32 ret = HCCL_SUCCESS;

    std::string collect_id_inter = "bcast_run_07";

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

#if 1
TEST_F(ExecutorTest, calculate_links_relation_08)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = 1;

    MOCKER(&AlgTemplateBase::CalcBinaryBlockHalvingDoubleLinkReleation)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    HalvingDoublingType type = HalvingDoublingType::BINARY_BLOCK_HALVING_DOUBLING;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

#if 1
TEST_F(ExecutorTest, calculate_links_relation_09)
{
    s32 ret = HCCL_SUCCESS;
    std::vector<bool> linkRelation;
    s32 rank = 11;
    s32 rankSize = 13;
    s32 rootRank = 1;

    MOCKER(&AlgTemplateBase::CalcBinaryBlockHalvingDoubleLinkReleation)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_PARA));

    HalvingDoublingType type = HalvingDoublingType::RESERVED_ALGORITHM_TYPE;
    linkRelation = AlgTemplateBase::CalcLinksRelation(rank,rankSize,rootRank,type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
#endif

TEST_F(ExecutorTest, st_ExecEmptyTask)
{
    s32 ret = HCCL_SUCCESS;
    DeviceMem input = DeviceMem::alloc(1);
    DeviceMem output = DeviceMem::alloc(1);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    ret = AlgTemplateBase::ExecEmptyTask(input, output, stream, dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ExecutorTest, AllReduceRing_Constructor)
{
   u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_RING, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceRing *alg = dynamic_cast<AllReduceRing *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}

TEST_F(ExecutorTest, AllReduceChunkMesh_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_CHUNK_MESH, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    Stream streamInstance = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> meshStreams = {streamInstance, streamInstance, streamInstance};
    std::shared_ptr<LocalNotify> signalPtr{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> meshSignal = {signalPtr, signalPtr, signalPtr};
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux = {signalPtr, signalPtr, signalPtr};
    u32 interRank = 0;
    u32 interRankSize = 0;
    u32 userRank = 0;
    HcomCollOpInfo opInfo{0};

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, interRank, interRankSize, userRank, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceChunkMesh *alg = dynamic_cast<AllReduceChunkMesh *>(tempAlg.get());

	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->localRank_, interRank);
    EXPECT_EQ(alg->localRankSize_, interRankSize);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->opInfo_, &opInfo);

    EXPECT_EQ(alg->meshStreams_.size(), meshStreams.size());
    for (u32 i = 0; i < meshStreams.size(); ++i) {
        EXPECT_EQ(alg->meshStreams_[i].id(), meshStreams[i].id());
    }

    EXPECT_EQ((*(alg->meshSignal_)).size(), meshSignal.size());
    for (u32 i = 0; i < meshSignal.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignal_))[i], meshSignal[i]);
    }

     EXPECT_EQ((*(alg->meshSignalAux_)).size(), meshSignalAux.size());
    for (u32 i = 0; i < meshSignalAux.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignalAux_))[i], meshSignalAux[i]);
    }
}

TEST_F(ExecutorTest, AllReduceLocalReduceBcast_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_LOCAL_REDUCE_BCAST, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    Stream streamInstance = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> meshStreams = {streamInstance, streamInstance, streamInstance};
    std::shared_ptr<LocalNotify> signalPtr{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> meshSignal = {signalPtr, signalPtr, signalPtr};
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux = {signalPtr, signalPtr, signalPtr};
    u32 interRank = 0;
    u32 interRankSize = 0;
    u32 userRank = 0;
    HcomCollOpInfo opInfo{0};

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, interRank, interRankSize, userRank, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceLocalReduceBcast *alg = dynamic_cast<AllReduceLocalReduceBcast *>(tempAlg.get());

	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->localRank_, interRank);
    EXPECT_EQ(alg->localRankSize_, interRankSize);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->opInfo_, &opInfo);

    EXPECT_EQ(alg->meshStreams_.size(), meshStreams.size());
    for (u32 i = 0; i < meshStreams.size(); ++i) {
        EXPECT_EQ(alg->meshStreams_[i].id(), meshStreams[i].id());
    }

    EXPECT_EQ((*(alg->meshSignal_)).size(), meshSignal.size());
    for (u32 i = 0; i < meshSignal.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignal_))[i], meshSignal[i]);
    }

     EXPECT_EQ((*(alg->meshSignalAux_)).size(), meshSignalAux.size());
    for (u32 i = 0; i < meshSignalAux.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignalAux_))[i], meshSignalAux[i]);
    }
}

TEST_F(ExecutorTest, AllReduceLocalReduce_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_LOCAL_REDUCE, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    Stream streamInstance = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> meshStreams = {streamInstance, streamInstance, streamInstance};
    std::shared_ptr<LocalNotify> signalPtr{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> meshSignal = {signalPtr, signalPtr, signalPtr};
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux = {signalPtr, signalPtr, signalPtr};
    u32 interRank = 0;
    u32 interRankSize = 0;
    u32 userRank = 0;
    HcomCollOpInfo opInfo{0};

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, interRank, interRankSize, userRank, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceLocalReduce *alg = dynamic_cast<AllReduceLocalReduce *>(tempAlg.get());

	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->localRank_, interRank);
    EXPECT_EQ(alg->localRankSize_, interRankSize);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->opInfo_, &opInfo);

    EXPECT_EQ(alg->meshStreams_.size(), meshStreams.size());
    for (u32 i = 0; i < meshStreams.size(); ++i) {
        EXPECT_EQ(alg->meshStreams_[i].id(), meshStreams[i].id());
    }

    EXPECT_EQ((*(alg->meshSignal_)).size(), meshSignal.size());
    for (u32 i = 0; i < meshSignal.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignal_))[i], meshSignal[i]);
    }

     EXPECT_EQ((*(alg->meshSignalAux_)).size(), meshSignalAux.size());
    for (u32 i = 0; i < meshSignalAux.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignalAux_))[i], meshSignalAux[i]);
    }
}

TEST_F(ExecutorTest, AllReduceMeshDirectOneshot_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_MESH_DIRECT_ONESHOT, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    Stream streamInstance = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> meshStreams = {streamInstance, streamInstance, streamInstance};
    std::shared_ptr<LocalNotify> signalPtr{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> meshSignal = {signalPtr, signalPtr, signalPtr};
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux = {signalPtr, signalPtr, signalPtr};
    u32 interRank = 0;
    u32 interRankSize = 0;
    u32 userRank = 0;
    HcomCollOpInfo opInfo{0};

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, interRank, interRankSize, userRank, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceMeshDirectOneshot *alg = dynamic_cast<AllReduceMeshDirectOneshot *>(tempAlg.get());

	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->localRank_, interRank);
    EXPECT_EQ(alg->localRankSize_, interRankSize);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->opInfo_, &opInfo);

    EXPECT_EQ(alg->meshStreams_.size(), meshStreams.size());
    for (u32 i = 0; i < meshStreams.size(); ++i) {
        EXPECT_EQ(alg->meshStreams_[i].id(), meshStreams[i].id());
    }

    EXPECT_EQ((*(alg->meshSignal_)).size(), meshSignal.size());
    for (u32 i = 0; i < meshSignal.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignal_))[i], meshSignal[i]);
    }

     EXPECT_EQ((*(alg->meshSignalAux_)).size(), meshSignalAux.size());
    for (u32 i = 0; i < meshSignalAux.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignalAux_))[i], meshSignalAux[i]);
    }
}

TEST_F(ExecutorTest, AllReduceMeshDirect_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_MESH_DIRECT, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    Stream streamInstance = Stream(StreamType::STREAM_TYPE_OFFLINE);
    std::vector<Stream> meshStreams = {streamInstance, streamInstance, streamInstance};
    std::shared_ptr<LocalNotify> signalPtr{new LocalNotify()};
    std::vector<std::shared_ptr<LocalNotify>> meshSignal = {signalPtr, signalPtr, signalPtr};
    std::vector<std::shared_ptr<LocalNotify>> meshSignalAux = {signalPtr, signalPtr, signalPtr};
    u32 interRank = 0;
    u32 interRankSize = 0;
    u32 userRank = 0;
    HcomCollOpInfo opInfo{0};

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(
        reduceAttrBitMap, meshStreams, meshSignal, meshSignalAux, interRank, interRankSize, userRank, &opInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceMeshDirect *alg = dynamic_cast<AllReduceMeshDirect *>(tempAlg.get());

	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->localRank_, interRank);
    EXPECT_EQ(alg->localRankSize_, interRankSize);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->opInfo_, &opInfo);

    EXPECT_EQ(alg->meshStreams_.size(), meshStreams.size());
    for (u32 i = 0; i < meshStreams.size(); ++i) {
        EXPECT_EQ(alg->meshStreams_[i].id(), meshStreams[i].id());
    }

    EXPECT_EQ((*(alg->meshSignal_)).size(), meshSignal.size());
    for (u32 i = 0; i < meshSignal.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignal_))[i], meshSignal[i]);
    }

     EXPECT_EQ((*(alg->meshSignalAux_)).size(), meshSignalAux.size());
    for (u32 i = 0; i < meshSignalAux.size(); ++i) {
        EXPECT_EQ((*(alg->meshSignalAux_))[i], meshSignalAux[i]);
    }
}

TEST_F(ExecutorTest, AllReduceOpbasePipeline_Constructor)
{
   u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_OPBASE_PIPELINE, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceOpbasePipeline *alg = dynamic_cast<AllReduceOpbasePipeline *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}

TEST_F(ExecutorTest, AllReduceAHC_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_AHC, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    u64 totalCount = 1;
    std::vector<std::vector<std::vector<u32>>> globalSubGroupsInstance = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}};
    std::map<AHCConcOpType, TemplateType> ahcAlgOptionInstance= {
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_NB},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_NB},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_RING},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_NB},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_NB},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_RING}
    };

	// 调用原有和AHC新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->Prepare(totalCount, globalSubGroupsInstance, ahcAlgOptionInstance);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceAHC *alg = dynamic_cast<AllReduceAHC *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->totalCount_, totalCount);

    EXPECT_EQ(alg->globalSubGroups_.size(), globalSubGroupsInstance.size());
    for (u32 i = 0; i < globalSubGroupsInstance.size(); ++i) {
        EXPECT_EQ(alg->globalSubGroups_[i].size(), globalSubGroupsInstance[i].size());
        for (u32 j = 0; j<alg->globalSubGroups_[i].size(); j++) {
            EXPECT_EQ(alg->globalSubGroups_[i][j].size(), globalSubGroupsInstance[i][j].size());
            for (u32 k = 0; k<alg->globalSubGroups_[i][j].size(); k++) {
                EXPECT_EQ(alg->globalSubGroups_[i][j][k], globalSubGroupsInstance[i][j][k]);
            }
        }  
    }

    EXPECT_EQ(alg->ahcAlgOption_.size(), ahcAlgOptionInstance.size());
    for (auto it = ahcAlgOptionInstance.begin(); it != ahcAlgOptionInstance.end(); ++it) {
        EXPECT_NE(alg->ahcAlgOption_.find(it->first), alg->ahcAlgOption_.end());
        EXPECT_EQ(alg->ahcAlgOption_[it->first], it->second);
    }
}

TEST_F(ExecutorTest, AllReduceAHCBroke_Constructor)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_REDUCE_AHC_BROKE, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

    u64 reduceAttrBitMap = 0x01;
    u64 totalCount = 1;
    std::vector<std::vector<std::vector<u32>>> globalSubGroupsInstance = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}};
    std::map<AHCConcOpType, TemplateType> ahcAlgOptionInstance= {
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_NB},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_NB},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_RING},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_NB},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_NB},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_RING}
    };

	// 调用原有和AHC新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = tempAlg->Prepare(totalCount, globalSubGroupsInstance, ahcAlgOptionInstance);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    AllReduceAHCBroke *alg = dynamic_cast<AllReduceAHCBroke *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
    EXPECT_EQ(alg->totalCount_, totalCount);

    EXPECT_EQ(alg->globalSubGroups_.size(), globalSubGroupsInstance.size());
    for (u32 i = 0; i < globalSubGroupsInstance.size(); ++i) {
        EXPECT_EQ(alg->globalSubGroups_[i].size(), globalSubGroupsInstance[i].size());
        for (u32 j = 0; j<alg->globalSubGroups_[i].size(); j++) {
            EXPECT_EQ(alg->globalSubGroups_[i][j].size(), globalSubGroupsInstance[i][j].size());
            for (u32 k = 0; k<alg->globalSubGroups_[i][j].size(); k++) {
                EXPECT_EQ(alg->globalSubGroups_[i][j][k], globalSubGroupsInstance[i][j][k]);
            }
        }  
    }

    EXPECT_EQ(alg->ahcAlgOption_.size(), ahcAlgOptionInstance.size());
    for (auto it = ahcAlgOptionInstance.begin(); it != ahcAlgOptionInstance.end(); ++it) {
        EXPECT_NE(alg->ahcAlgOption_.find(it->first), alg->ahcAlgOption_.end());
        EXPECT_EQ(alg->ahcAlgOption_[it->first], it->second);
    }    
}

TEST_F(ExecutorTest, ReduceNHROneshot_Constructor)
{
   u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCE_NHR_ONE_SHOT, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    ReduceNHROneshot *alg = dynamic_cast<ReduceNHROneshot *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}

TEST_F(ExecutorTest, ReduceRing_Constructor)
{
   u64 reduceAttrBitMap = 0x01;

    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_REDUCE_RING, dispatcher);
	EXPECT_NE(tempAlg, nullptr);

	// 调用新增的prepare
    s32 ret = tempAlg->Prepare(reduceAttrBitMap);
    EXPECT_EQ(ret, HCCL_SUCCESS);

	// 转换子类指针
    ReduceRing *alg = dynamic_cast<ReduceRing *>(tempAlg.get());
	// 校验成员变量
    EXPECT_EQ(alg->reduceAttr_, reduceAttrBitMap);
}

TEST_F(ExecutorTest, AllGatherHalvingDoubling_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_HALVING_DOUBLING, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherHalvingDoubling *alg = dynamic_cast<AllGatherHalvingDoubling *>(tempAlg.get());
    // 调用新增的prepare
    s32 ret = tempAlg->Prepare(12U, UserMemType::INPUT_MEM, UserMemType::OUTPUT_MEM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->blockSize_, 12U);
    EXPECT_EQ(alg->hdInputMemType_, UserMemType::INPUT_MEM);
    EXPECT_EQ(alg->hdOutputMemType_, UserMemType::OUTPUT_MEM);
}

TEST_F(ExecutorTest, AllGatherMeshAtomic_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_MESH_ATOMIC, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherMeshAtomic *alg = dynamic_cast<AllGatherMeshAtomic *>(tempAlg.get());
    // 调用新增的prepare
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(1);
    s32 ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 10, nullptr, 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->meshStreams_.size(), mesh_streams.size());
    EXPECT_EQ(alg->meshSignal_, &mesh_signal);
    EXPECT_EQ(alg->meshSignalAux_, &mesh_signal_aux);
    EXPECT_EQ(alg->interRank_, 0U);
    EXPECT_EQ(alg->interRankSize_, 1U);
    EXPECT_EQ(alg->userRank_, 10U);
}

TEST_F(ExecutorTest, AllgatherMeshDirect_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_MESH_DIRECT, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllgatherMeshDirect *alg = dynamic_cast<AllgatherMeshDirect *>(tempAlg.get());
    // 调用新增的prepare
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(1);
    HcomCollOpInfo opInfo;
    s32 ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 10, &opInfo, 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->meshStreams_.size(), mesh_streams.size());
    EXPECT_EQ(alg->meshSignal_, &mesh_signal);
    EXPECT_EQ(alg->meshSignalAux_, &mesh_signal_aux);
    EXPECT_EQ(alg->opInfo_, &opInfo);
    EXPECT_EQ(alg->interRank_, 0U);
    EXPECT_EQ(alg->interRankSize_, 1U);
    EXPECT_EQ(alg->userRank_, 10U);
}

TEST_F(ExecutorTest, AllgatherMeshMix_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_MESH_MIX, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllgatherMeshMix *alg = dynamic_cast<AllgatherMeshMix *>(tempAlg.get());
    // 调用新增的prepare
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(1);
    HcomCollOpInfo opInfo;
    s32 ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 10, &opInfo, 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->meshStreams_.size(), mesh_streams.size());
    EXPECT_EQ(alg->meshSignal_, &mesh_signal);
    EXPECT_EQ(alg->meshSignalAux_, &mesh_signal_aux);
    EXPECT_EQ(alg->opInfo_, &opInfo);
    EXPECT_EQ(alg->interRank_, 0U);
    EXPECT_EQ(alg->interRankSize_, 1U);
}

TEST_F(ExecutorTest, AllGatherMesh_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_MESH, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherMesh *alg = dynamic_cast<AllGatherMesh *>(tempAlg.get());
    // 调用新增的prepare
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(1);
    s32 ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 10, nullptr, 0, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->meshStreams_.size(), mesh_streams.size());
    EXPECT_EQ(alg->meshSignal_, &mesh_signal);
    EXPECT_EQ(alg->meshSignalAux_, &mesh_signal_aux);
    EXPECT_EQ(alg->interRank_, 0U);
    EXPECT_EQ(alg->interRankSize_, 1U);
    EXPECT_EQ(alg->userRank_, 10U);
}

TEST_F(ExecutorTest, AllGatherRecursiveHalvingDoubling_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_GATHER_RECURSIVE_HALVING_DOUBLING, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherRecursiveHalvingDoubling *alg = dynamic_cast<AllGatherRecursiveHalvingDoubling *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AllGatherRing_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherRing *alg = dynamic_cast<AllGatherRing *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AllGatherHDStage_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_HD_STAGE, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherHDStage *alg = dynamic_cast<AllGatherHDStage *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AllGatherNB_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NB, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherNB *alg = dynamic_cast<AllGatherNB *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AllGatherNHRV1_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHRV1, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherNHRV1 *alg = dynamic_cast<AllGatherNHRV1 *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AllGatherNHR_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHR, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherNHR *alg = dynamic_cast<AllGatherNHR *>(tempAlg.get());
    EXPECT_FALSE(alg->isNeedMerge);
    tempAlg->Prepare(true);
    EXPECT_TRUE(alg->isNeedMerge);
}

TEST_F(ExecutorTest, AllGatherPipeline_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_PIPELINE, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherPipeline *alg = dynamic_cast<AllGatherPipeline *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AllGatherUnifiedMarch_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_UNIFIED_MARCH, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherUnifiedMarch *alg = dynamic_cast<AllGatherUnifiedMarch *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, MultiRootScatterRing_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_MULTI_ROOT_SCATTER_RING, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    MultiRootScatterRing *alg = dynamic_cast<MultiRootScatterRing *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, ScatterMesh_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_SCATTER_MESH, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    ScatterMesh *alg = dynamic_cast<ScatterMesh *>(tempAlg.get());

    u32 interRank = 5;
    u32 interRankSize = 8;
    tempAlg->Prepare(interRank, interRankSize);

    EXPECT_EQ(alg->interRank_, interRank);
    EXPECT_EQ(alg->interRankSize_, interRankSize);
}

TEST_F(ExecutorTest, ScatterNB_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_SCATTER_NB, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    ScatterNB *alg = dynamic_cast<ScatterNB *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, ScatterNHR_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_SCATTER_NHR, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    ScatterNHR *alg = dynamic_cast<ScatterNHR *>(tempAlg.get());
    EXPECT_FALSE(alg->isNeedMerge);
    tempAlg->Prepare(true);
    EXPECT_TRUE(alg->isNeedMerge);
}

TEST_F(ExecutorTest, AlltoallPipelineMeshPairwiseCCLEnough_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_2_ALL_PIPELINE_MESH_PAIRWISE_CCL_ENOUGH, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AlltoallPipelineMeshPairwiseCCLEnough *alg = dynamic_cast<AlltoallPipelineMeshPairwiseCCLEnough *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, AlltoallPipelineMeshPairwisePingPong_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_2_ALL_PIPELINE_MESH_PAIRWISE_PING_PONG, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AlltoallPipelineMeshPairwisePingPong *alg = dynamic_cast<AlltoallPipelineMeshPairwisePingPong *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, GatherMesh_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_GATHER_MESH, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    GatherMesh *alg = dynamic_cast<GatherMesh *>(tempAlg.get());
    std::vector<Stream> mesh_streams;
    mesh_streams.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal;
    mesh_signal.resize(1);
    std::vector<std::shared_ptr<LocalNotify>> mesh_signal_aux;
    mesh_signal_aux.resize(1);
    s32 ret = tempAlg->Prepare(mesh_streams, mesh_signal, mesh_signal_aux, 10U);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(alg->meshStreams_.size(), mesh_streams.size());
    EXPECT_EQ(alg->meshSignal_, &mesh_signal);
    EXPECT_EQ(alg->meshSignalAux_, &mesh_signal_aux);
    EXPECT_EQ(alg->userRank_, 10U);
}

TEST_F(ExecutorTest, GatherRing_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_GATHER_RING, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    GatherRing *alg = dynamic_cast<GatherRing *>(tempAlg.get());
    EXPECT_NE(alg, nullptr);
}

TEST_F(ExecutorTest, GatherStar_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_GATHER_STAR, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    GatherStar *alg = dynamic_cast<GatherStar *>(tempAlg.get());
    s32 ret = tempAlg->Prepare(10U);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(alg->userRank_, 10U);
}

TEST_F(ExecutorTest, AllGatherRingDirect_create_prepare_destructor_001)
{
    // 获取模板实例
    std::unique_ptr<AlgTemplateBase> tempAlg =
        AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING_DIRECT, dispatcher);
    EXPECT_NE(tempAlg, nullptr);
    // 转换子类指针
    AllGatherRingDirect *alg = dynamic_cast<AllGatherRingDirect *>(tempAlg.get());
    HcomCollOpInfo opInfo;
    u32 userRank = 8;
    Slice slice1;
    slice1.size = 90;
    slice1.offset = 0;
    Slice slice2;
    slice2.size = 90;
    slice2.offset = 90;
    std::vector<Slice> userMemInputSlices;
    userMemInputSlices.push_back(slice1);
    userMemInputSlices.push_back(slice2);
    s32 ret = tempAlg->Prepare(&opInfo, userRank, userMemInputSlices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(alg->opInfo_, &opInfo);
    EXPECT_EQ(alg->userRank_, userRank);
    EXPECT_EQ(alg->userMemOutputSlices_.size(), userMemInputSlices.size());
    EXPECT_TRUE(alg->isSdma_);
    for (u32 i = 0; i < userMemInputSlices.size(); ++i) {
        EXPECT_EQ(alg->userMemOutputSlices_[i].offset, userMemInputSlices[i].offset);
    }
}