#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <runtime/rt.h>


#include <assert.h>
#include <semaphore.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <cce/dnn.h>
#include <securec.h>
#define private public
#define protected public
#include "alltoall_operator.h"
#include "llt_hccl_stub.h"
#include "transport_ibverbs_pub.h"
#include "all_gather_pipeline_pub.h"
#include "hccl_communicator.h"
#include "dispatcher_pub.h"
#include "dispatcher_graph_pub.h"
#include "dispatcher_virtural_pub.h"
#include "dispatcher.h"
#undef protected
#undef private
#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#include "dlra_function.h"
#include "dltdt_function.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#include "sal.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "param_check_pub.h"
#include "externalinput.h"
#include "hcom_private.h"
#include "rank_consistentcy_checker.h"


#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;

class DispatcherVoid : public DispatcherPub{
public:
    explicit DispatcherVoid(const s32 deviceLogicId);
    ~DispatcherVoid() override;

    HcclResult SignalRecord(HcclRtNotify signal, hccl::Stream &stream, u32 userRank, u64 offset = INVALID_U64,
        s32 stage = INVALID_VALUE_STAGE, bool inchip = false, u64 signalAddr = INVALID_U64, u32 notifyId = INVALID_UINT) override;
    HcclResult SignalWait(HcclRtNotify signal, hccl::Stream &stream, u32 userRank, u32 remoteUserRank,
        s32 stage = INVALID_VALUE_STAGE, bool inchip = false, u32 notifyId = INVALID_UINT,
        u32 timeOut = NOTIFY_INVALID_WAIT_TIME) override;
    HcclResult MemcpyAsync(hccl::DeviceMem &dst, const hccl::DeviceMem &src, hccl::Stream &stream,
        u32 remoteUserRank = INVALID_VALUE_RANKID, hccl::LinkType inLinkType = hccl::LinkType::LINK_ONCHIP) override;
    HcclResult ReduceAsync(const void *src, void *dst, u64 dataCount, const HcclDataType datatype, HcclReduceOp redOp,
        Stream &stream, HcclReduceType reduceType = HcclReduceType::HCCL_TBE_REDUCE) override;
    HcclResult InlineReduceAsync(const void *src, u64 dataCount, const HcclDataType datatype, HcclReduceOp redOp,
        Stream &stream, void *dst, u32 remoteUserRank = INVALID_VALUE_RANKID,
        hccl::LinkType inLinkType = hccl::LinkType::LINK_ONCHIP) override;
    HcclResult RdmaSend(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream,
        u32 remoteUserRank = INVALID_VALUE_RANKID, bool isCapture = false) override;
    HcclResult RdmaSend(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream, u32 userRank,
        u64 offset, bool isCapture = false) override;
};

DispatcherVoid::DispatcherVoid(const s32 deviceLogicId)
    : DispatcherPub(deviceLogicId)
{}

DispatcherVoid::~DispatcherVoid()
{}

HcclResult DispatcherVoid::SignalRecord(HcclRtNotify signal, Stream &stream, u32 userRank, u64 offset, s32 stage,
    bool inchip, u64 signalAddr, u32 notifyId)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherVoid::SignalWait(HcclRtNotify signal, Stream &stream, u32 userRank, u32 remoteUserRank, s32 stage,
    bool inchip, u32 notifyId, u32 timeOut)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherVoid::MemcpyAsync(hccl::DeviceMem &dst, const hccl::DeviceMem &src, hccl::Stream &stream,
    u32 remoteUserRank, hccl::LinkType inLinkType)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherVoid::ReduceAsync(const void *src, void *dst, u64 dataCount, const HcclDataType datatype,
    HcclReduceOp redOp, Stream &stream, HcclReduceType reduceType)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherVoid::InlineReduceAsync(const void *src, u64 dataCount, const HcclDataType datatype,
    HcclReduceOp redOp, Stream &stream, void *dst, u32 remoteUserRank, hccl::LinkType inLinkType)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherVoid::RdmaSend(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream,
    u32 remoteUserRank, bool isCapture)
{
    return HCCL_SUCCESS;
}

HcclResult DispatcherVoid::RdmaSend(u32 dbindex, u64 dbinfo, const struct SendWr &wr, hccl::Stream &stream,
    u32 userRank, u64 offset, bool isCapture)
{
    return HCCL_SUCCESS;
}

HcclResult HcclDispatcherInit_stub(DispatcherType type, const s32 deviceLogicId, HcclDispatcher *dispatcher)
{
    DispatcherVoid* dispatcherVoid = new (std::nothrow) DispatcherVoid(deviceLogicId);
    *dispatcher = dispatcherVoid;
    return HCCL_SUCCESS;
}

class MPI_ALLTOALL_910B_Test : public testing::Test
{
protected:
    static void SetUpTestCase() {
        std::cout << "MPI_AllToAll_910B_Test SetUP" << std::endl;
    }
    static void TearDownTestCase() {
        std::cout << "MPI_AllToAll_910B_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp() {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        MOCKER(HcclDispatcherInit)
        .stubs()
        .will(invoke(HcclDispatcherInit_stub));
        std::cout << "A MPI_AllToAll_910B_Test SetUP" << std::endl;
    }
    virtual void TearDown() {
        MPI_Barrier(MPI_COMM_WORLD);
        GlobalMockObject::verify();
        std::cout << "A MPI_AllToAll_910B_Test TearDown" << std::endl;
    }
};

#define GOUP_DEV_NUM 8
#define HCCL_ALLGATHER_DATA_SIZE 2
#define DEV_NUM_4 4
#define DEV_NUM_8 8
#define MPI_ALLGATHER_ALIGN_DATA_SIZE 128  //对齐的datacount
typedef struct para_struct
{
    HcclRootInfo rootInfo;
    std::string identify;
    s32 comm_num;
    s32 userRankSize;
    s32 device_id;
    s32 ranks_local; //本服务器内的rank数

    char* file_name;
    void *sendbuff;
    void *sendCounts;
    void *sdispls;
    void *recvbuff;
    void *recvCounts;
    void *rdispls;
    void *sendCountMatrix;
    s32 count;
    HcclDataType datatype;
    HcclDataType sendDataType;
    HcclDataType recvDataType;
    HcclReduceOp op;
    rtStream_t stream;
    rtStream_t stream1;
    int id;
    bool flag;
    volatile s32* sync_addr;
    const char* tag;
    char* groupName;
    u32 groupRanksNum;
    u32 *pGroupRanks;
} para_t;

//MPI, 每个进程内作为一个服务器，每个服务器内一个rank
void *inter_alltoallvc_pipeline_task(void *parg)
{
    HcclResult ret = HCCL_SUCCESS;
    para_t *para_info = (para_t *)parg;
    s32 rank_num_tmp;

    HcomInfo hcom_info;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    hrtSetDevice(para_info->device_id);
    set_chip_type_stub(para_info->device_id, static_cast<s32>(DevType::DEV_TYPE_910B));

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memset(hcom_info.params.id.internal, HCCL_ROOT_INFO_BYTES, 0, sizeof(hcom_info.params.id.internal));
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->rootInfo, sizeof(HcclRootInfo));


    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm(1024*1024*200, 1024*1024*200, HCCL_WORLD_GROUP));
    (void) SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    std::string algo = "others=level0:fullmesh;level1:pipeline";
    ret = SetHcclAlgoConfig(algo);

    HcclWorkflowMode mode = GetWorkflowMode();
    InitWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    hcom_info.pComm->communicator_->implAlg_->algConfigurator_->algType_[HcclCMDType::HCCL_CMD_ALLTOALL].algoLevel0 =
        AlgTypeLevel0::ALG_LEVEL0_NP_MESH;
    hcom_info.pComm->communicator_->implAlg_->algConfigurator_->algType_[HcclCMDType::HCCL_CMD_ALLTOALL].algoLevel1 =
        AlgTypeLevel1::ALG_LEVEL1_PIPELINE;

    ret = hcom_info.pComm->CreateCommCCLbuffer();

    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("dev[%d] task all_gather falis", para_info->device_id);
    }

    //-----------------Set Workspace Resource Start------------------//
    u64 stream_list_size = 4;
    HCCL_ERROR("==TMP== stream_list_size=%d", stream_list_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HCCL_ERROR("get stream_list_size[%d] success", stream_list_size);
    vector<HcclRtStream> streamList(stream_list_size);
    rtError_t rt_ret;
    //生成从stream
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtStreamCreateWithFlags(&streamList[i], 0, (RT_STREAM_PERSISTENT | RT_STREAM_FORCE_COPY));
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
        //从流bind到model
        rt_ret = rtModelBindStream(model, streamList[i], RT_MODEL_WAIT_ACTIVE_STREAM);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    string tag = "tag-alltoallv-pipeline";
    void *memptr = nullptr;
    ret = hrtMalloc(&memptr, 10);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcom_info.pComm->SetWorkspaceResource(tag, memptr, 10, streamList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    //-----------------Set Workspace Resource End------------------//

    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;

    do {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr, rank_num_tmp, rank_num_tmp + 1);
    } while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    { sched_yield(); }  // linux提供一个系统调用运行进程主动让出执行权

    __sync_synchronize();

    ret = hcom_info.pComm->AlltoAllVC(para_info->sendbuff,
        para_info->sendCountMatrix,
        para_info->sendDataType,
        para_info->recvbuff,
        para_info->recvDataType,
        para_info->stream,
        tag);

    rt_ret = RT_ERROR_NONE;
    rt_ret = rtStreamSynchronize(para_info->stream);

    if (rt_ret != RT_ERROR_NONE) {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }
    //--------------Resource destroy----------------//
    for (s32 i = 0; i < stream_list_size; i++)
    {
        rt_ret = rtModelUnbindStream(model, streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);

        rt_ret = rtStreamDestroy(streamList[i]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    hrtFree(memptr);
    return (nullptr);
}

#if 1
// graph fullmesh pairwise
TEST_F(MPI_ALLTOALL_910B_Test, ut_inter_fullmesh_pairwise_alltoall_pipeline_zcopy_2ranks_4server_int64)
{
    MOCKER_CPP(&AlltoAllOperator::IsSatisfyAlltoallPipelineCondition).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&hcclComm::AlltoAllVC).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcclResult ret;
    std::string algo = "level0:fullmesh;level1:pipeline";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_2server_4rank;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_zcopy_4ranks_2server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ResetInitState();
    SetFftsSwitch(false);
    InitExternalInput();

    s32 nnode, rank, noderank, errors = 0;

    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_8];
    s64* sendbuff[DEV_NUM_8];
    s64* recvbuff[DEV_NUM_8];
    u64* sendCounts[DEV_NUM_8];
    u64* sdispls[DEV_NUM_8];
    u64* recvCounts[DEV_NUM_8];
    u64* rdispls[DEV_NUM_8];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_8 * DEV_NUM_8 * sizeof(u64));

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_8];
    u64 ctotalRecvCounts[DEV_NUM_8];

    u64 count = 1;
    for (u32 i = 0; i < 8; i++ ) {
        for (u32 j = 0; j < 8; j++) {
            sendCountMatrix[i * 8 + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < 8; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(8 * sizeof(u64));
        for (u32 j = 0; j < 8; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * 8 + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]", i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(8 * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < 8; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(8 * sizeof(u64));
        for (u32 j = 0; j < 8; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + 8 * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]", i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(8 * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < 8; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i + ndev * noderank];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i + ndev * noderank];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i + ndev * noderank];
        para_info[i].sdispls = sdispls[i + ndev * noderank];

        para_info[i].recvCounts = recvCounts[i + ndev * noderank];
        para_info[i].rdispls = rdispls[i + ndev * noderank];
        para_info[i].id = i + ndev * noderank;

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallvc_pipeline_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < 8; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

    }

    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);
    EXPECT_EQ(SetHcclAlgoConfig("level0:NA;level1:H-D_R"), HCCL_SUCCESS);

    ResetInitState();

    GlobalMockObject::verify();
}
#endif

#if 1 //checker 已看护
// graph fullmesh pairwise
TEST_F(MPI_ALLTOALL_910B_Test, ut_inter_fullmesh_pairwise_alltoall_pipeline_bcopy_2ranks_4server_int64)
{

    s32 portNum = 7;
    MOCKER(hrtGetHccsPortNum)
        .stubs()
        .with(any(), outBound(portNum))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AlltoAllOperator::IsSatisfyAlltoallPipelineCondition).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&hcclComm::AlltoAllVC).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclResult ret;
    std::string algo = "level0:fullmesh;level1:pipeline";
    ret = SetHcclAlgoConfig(algo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    RankConsistentcyChecker::GetInstance().ClearCheckInfo();

    nlohmann::json rank_table = rank_table_910_2server_4rank_new;
    char file_name_t[] = "./ut_inter_alltoallvc_staged_zcopy_4ranks_2server_int64.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    ResetInitState();
    SetFftsSwitch(false);
    InitExternalInput();

    s32 nnode, rank, noderank, errors = 0;

    ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s64* result_buff[DEV_NUM_8];
    s64* sendbuff[DEV_NUM_8];
    s64* recvbuff[DEV_NUM_8];
    u64* sendCounts[DEV_NUM_8];
    u64* sdispls[DEV_NUM_8];
    u64* recvCounts[DEV_NUM_8];
    u64* rdispls[DEV_NUM_8];
    u64* sendCountMatrix = (u64*)sal_malloc(DEV_NUM_8 * DEV_NUM_8 * sizeof(u64));

    s32 sync_value = 0;

    rtStream_t stream[DEV_NUM_4];
    sal_thread_t tid[DEV_NUM_4];
    para_t para_info[DEV_NUM_4];

    HcclDataType sendDataType = HCCL_DATA_TYPE_INT64;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT64;

    s32 ndev = DEV_NUM_4;
    set_board_id(0x0000);

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);

    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u64 totalRecvCounts[DEV_NUM_8];
    u64 ctotalRecvCounts[DEV_NUM_8];

    u64 count = 550000;
    for (u32 i = 0; i < 8; i++ ) {
        for (u32 j = 0; j < 8; j++) {
            sendCountMatrix[i * 8 + j] = (j + 1) * count;
        }
    }
    for (u32 i = 0; i < 8; i++)
    {
        u64 totalSendCounts = 0;
        u64 ctotalSendCounts = 0;
        sendCounts[i] = (u64*)sal_malloc(8 * sizeof(u64));
        for (u32 j = 0; j < 8; j++) {
            sendCounts[i][j] = (j + 1) * count;
            totalSendCounts += sendCounts[i][j];
            ctotalSendCounts += sendCountMatrix[i * 8 + j];
        }

        HCCL_INFO("==TMP== rank%u totalSendCounts[%llu], ctotalSendCounts[%llu]", i, totalSendCounts, ctotalSendCounts);

        sdispls[i] = (u64*)sal_malloc(8 * sizeof(u64));
        u64 curSDispl = 0;
        for (u32 j = 0; j < 8; j++) {
            sdispls[i][j] = curSDispl;
            curSDispl += sendCounts[i][j];
        }

        totalRecvCounts[i] = 0;
        ctotalRecvCounts[i] = 0;
        recvCounts[i] = (u64*)sal_malloc(8 * sizeof(u64));
        for (u32 j = 0; j < 8; j++) {
            recvCounts[i][j] = (1 + i) * count;
            totalRecvCounts[i] += recvCounts[i][j];
            ctotalRecvCounts[i] += sendCountMatrix[i + 8 * j];
        }
        HCCL_INFO("==TMP== rank%u totalRecvCounts[%llu], ctotalRecvCounts[%llu]", i, totalRecvCounts[i], ctotalRecvCounts[i]);

        rdispls[i] = (u64*)sal_malloc(8 * sizeof(u64));
        u64 curRDispl = 0;
        for (u32 j = 0; j < 8; j++) {
            rdispls[i][j] = curRDispl;
            curRDispl += recvCounts[i][j];
        }

        ret = hrtMalloc((void**)&sendbuff[i], totalSendCounts * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        for (u32 j = 0; j < totalSendCounts; j++) {
            sendbuff[i][j] = 1;
        }

        ret = hrtMalloc((void**)&recvbuff[i], totalRecvCounts[i] * sizeof(s64));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(recvbuff[i], totalRecvCounts[i] * sizeof(s64), 0, totalRecvCounts[i] * sizeof(s64));

        result_buff[i] = (s64*)sal_malloc(totalRecvCounts[i] * sizeof(s64));
        for (u32 j = 0; j < totalRecvCounts[i]; j++) {
            result_buff[i][j] = 1;
        }
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (s32 i = 0; i < ndev; i++)
    {
        sal_memcpy(&para_info[i].rootInfo, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << (i + ndev * noderank);
        para_info[i].identify = identify.str();
        para_info[i].userRankSize = ndev * nnode;
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;


        para_info[i].sendbuff = sendbuff[i + ndev * noderank];
        para_info[i].stream = stream[i];
        para_info[i].recvbuff = recvbuff[i + ndev * noderank];
        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].sendCounts = sendCounts[i + ndev * noderank];
        para_info[i].sdispls = sdispls[i + ndev * noderank];

        para_info[i].recvCounts = recvCounts[i + ndev * noderank];
        para_info[i].rdispls = rdispls[i + ndev * noderank];
        para_info[i].id = i + ndev * noderank;

        para_info[i].sendCountMatrix = sendCountMatrix;

        para_info[i].sendDataType = sendDataType;
        para_info[i].recvDataType = recvDataType;
    }
    for (s32 i = 0; i < ndev; ++i)
    {
        tid[i] = sal_thread_create("thread", inter_alltoallvc_pipeline_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

    SaluSleep(SAL_SECOND_USEC);

    for (s32 j = 0; j < 8; j++)
    {
        hrtFree(sendbuff[j]);
        hrtFree(recvbuff[j]);
        sal_free(result_buff[j]);

        sal_free(sendCounts[j]);
        sal_free(sdispls[j]);
        sal_free(recvCounts[j]);
        sal_free(rdispls[j]);

    }

    for (s32 j = 0; j < ndev; j++)
    {
        rt_ret = rtStreamDestroy(stream[j]);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    sal_free(sendCountMatrix);

    set_board_id(0x0000);
    remove(file_name_t);
    EXPECT_EQ(errors, 0);

    ResetInitState();

    GlobalMockObject::verify();
}
#endif