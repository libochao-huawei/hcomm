#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <runtime/rt.h>
#include <fstream>

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


#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>

#include "hcom_executor_internel.h"
#include "../inc/hccl/base.h"
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "dlra_function.h"
#include "hccl/inc/remote_access.h"
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "config.h"
#include "param_check_pub.h"

using namespace std;
using namespace hccl;

class MPI_REMOTE_ACCESS_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_REMOTE_ACCESS_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_REMOTE_ACCESS_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "MPI_REMOTE_ACCESS_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "MPI_REMOTE_ACCESS_Test TearDown" << std::endl;
    }
};

static HcclResult g_excutorStatus;

void setExecutorStatus(HcclResult status)
{
    HCCL_INFO("this is setExecutorStatus fuction");
    g_excutorStatus = status;
}

void getExecutorStatus(HcclResult &status)
{
    status = g_excutorStatus;
}

const s32 MRAddrNum = 2;
struct RemoteAcceesParams {
    HcclRootInfo unique_id;
    std::string identify;
    char* file_name;
    s32 device_id;
    s32 ranks_local; //本服务器内的rank数
    rtStream_t stream;
    // 内存注册参数
    MemRegisterAddr addrList[MRAddrNum];

    // hcom 执行器参数
    string remoteAccessType;
    vector<HcomRemoteAccessAddrInfo> addrInfos;


    volatile s32* sync_addr;
};


void* inter_remote_access_task(void* para)
{
    HcclResult ret = HCCL_SUCCESS;

    RemoteAcceesParams* para_info = (RemoteAcceesParams*) para;
    s32 rank_num_tmp;
    u32 uRankId = -1;
    rtError_t rt_ret = RT_ERROR_NONE;
    std::string ranktable_file = para_info->file_name;
    std::string rankTableM;
    std::string realFilePath;

    HcomInfo hcom_info;
    hrtSetDevice(para_info->device_id);

    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcomLoadRanktableFile(ranktable_file.c_str(), rankTableM, realFilePath);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = CfgGetClusterInfo(rankTableM, para_info->identify, hcom_info.params, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &para_info->unique_id, sizeof(HcclRootInfo));
    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm());
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);

    shared_ptr<RemoteAccess> RemoteAccess;
    RemoteAccess.reset(new (std::nothrow) hccl::RemoteAccess());
    vector<MemRegisterAddr> addrInfos;
    for (u32 i = 0; i < MRAddrNum; i++) {
        addrInfos.push_back(para_info->addrList[i]);
    }
    RemoteAccess->Init(hcom_info.params.rank, addrInfos, hcom_info.rankTable);
    bool swapped;
    rank_num_tmp = *(para_info->sync_addr) - 1;
    do
    {
        rank_num_tmp += 1;

        swapped = __sync_bool_compare_and_swap(para_info->sync_addr /** &rank_num */, rank_num_tmp, rank_num_tmp + 1);
    }
    while (!swapped);

    while (*(para_info->sync_addr) < para_info->ranks_local)
    {
        sched_yield(); // linux提供一个系统调用运行进程主动让出执行权
    }

    RemoteAccess->RemoteRead(para_info->addrInfos, para_info->stream);

    rt_ret = rtStreamSynchronize(para_info->stream);

    if ( rt_ret != RT_ERROR_NONE)
    {
        HCCL_ERROR("rank[%d] task allgather falis", hcom_info.params.rank);
    }

    return (NULL);
}



#define DEV_NUM_8 8

TEST_F(MPI_REMOTE_ACCESS_Test, st_mpi_remote_read)
{
    nlohmann::json g_rank_table_16p_V80 = rank_table_910_2server_8rank;
    char file_name_t[] = "./st_mpi_remote_read.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(4) << g_rank_table_16p_V80 << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();

    s32 nnode, rank, errors = 0;
    set_board_id(0);
    s32 sync_value = 0;
    s32 noderank = 0;

    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;

    float* hostDDR_buff[DEV_NUM_8];
    float* deviceHBM_buff[DEV_NUM_8];
    
    float checkData = 2.0f;
    rtStream_t stream[DEV_NUM_8];
    sal_thread_t tid[DEV_NUM_8];
    RemoteAcceesParams para_info[DEV_NUM_8];

    u32 count = 1024;
    u32 ndev = DEV_NUM_8;

    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &noderank);
    HcclRootInfo unique_id;
    ret = hccl::hcclComm::GetUniqueId(&unique_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MPI_Bcast(&unique_id, sizeof(unique_id), MPI_BYTE, 0, MPI_COMM_WORLD);

    // 初始化缓存数据
    for (u32 i = 0; i < ndev; i++) {
        
        ret = hrtMalloc((void **)&hostDDR_buff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(hostDDR_buff[i], count * sizeof(float), 0, count * sizeof(float));
        
        ret = hrtMalloc((void **)&deviceHBM_buff[i], count * sizeof(float));
        EXPECT_EQ(ret, HCCL_SUCCESS);
        sal_memset(deviceHBM_buff[i], count * sizeof(float), 0, count * sizeof(float));
    }
    //sendbuf 赋值
    for (u32 j = 0; j < ndev; j++) {
        for (u32 i = 0; i < count; i++) {
            hostDDR_buff[j][i] = checkData;
        }
    }
    u64 HostAddressesLocal[DEV_NUM_8];
    for (int i = 0; i < ndev; i++) {
        HostAddressesLocal[i] = (u64)hostDDR_buff[i];
       // HCCL_INFO("HostAddressesLocal[%d]: [%llu]", i, HostAddressesLocal[i]);
    }
    u64 HostAddressesAll[DEV_NUM_8 * nnode];
    MPI_Allgather(HostAddressesLocal, DEV_NUM_8, MPI_UNSIGNED_LONG_LONG,
                  HostAddressesAll, DEV_NUM_8, MPI_UNSIGNED_LONG_LONG,
                  MPI_COMM_WORLD);
    for (int i = 0; i < DEV_NUM_8 * nnode; i++) {
        // HCCL_INFO("HostAddressesAll[%d]: [%llu]", i, HostAddressesAll[i]);
    }
    u64 HostAddressesRemote[DEV_NUM_8];
    for (int i = 0; i < ndev; i++) {
        if (noderank == 0) {
            HostAddressesRemote[i] = HostAddressesAll[i + DEV_NUM_8];
        } else {
            HostAddressesRemote[i] = HostAddressesAll[i];
        }
        // HCCL_INFO("HostAddressesRemote[%d]: [%llu]", i, HostAddressesRemote[i]);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        rt_ret = rtStreamCreate(&stream[i], 5);
        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    for (u32 i = 0; i < ndev; i++) {
        sal_memcpy(&para_info[i].unique_id, sizeof(HcclRootInfo), &unique_id, sizeof(HcclRootInfo));
        std::ostringstream identify("");
        identify << i + ndev * noderank;
        para_info[i].identify = identify.str();
        para_info[i].device_id = i ;
        para_info[i].ranks_local = ndev;

        para_info[i].sync_addr = &sync_value;
        para_info[i].file_name = file_name_t;

        para_info[i].addrList[0].addr = HostAddressesLocal[i];
        para_info[i].addrList[0].length = count * sizeof(float);

        para_info[i].addrList[1].addr = (u64)deviceHBM_buff[i];
        para_info[i].addrList[1].length = count * sizeof(float);
        para_info[i].remoteAccessType = HCOM_REMOTE_READ;

        para_info[i].stream = stream[i];

        HcomRemoteAccessAddrInfo info;
        info.length = count * sizeof(float);
        info.localAddr = (u64)deviceHBM_buff[i];
        info.remoteAddr = HostAddressesRemote[i];
        if (noderank == 0) {
            info.remotetRankID = ndev + i;
        } else {
            info.remotetRankID = i;
        }
    }

    // 创建每个dev对应的任务线程
    for (s32 i = 0; i < ndev; i++) {
        tid[i] = sal_thread_create("thread", inter_remote_access_task, (void*)&para_info[i]);
        EXPECT_NE(tid[i], (sal_thread_t )NULL);
    }

    for (s32 i = 0; i < ndev; ++i)
    {
        while ( sal_thread_is_running(tid[i]))
        {
            SaluSleep(SAL_MILLISECOND_USEC * 10);
        }
    }

    //获取stream的操作的同步信号量
    for (s32 j = 0; j < ndev; j++)
    {
       rt_ret = rtStreamSynchronize(stream[j]);
       EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /*check result*/
    for (s32 j = 0; j < ndev; j++)
    {
        for (s32 i = 0; i < count; i++)
        {
            float recv = deviceHBM_buff[j][i];

            if (abs(checkData - recv) > 1e-6)
            {
               HCCL_ERROR("noderank:%d recvbuf[%d][%d]:%f res[%d][%d]:%f\n", noderank, j, i, recv,j,i,checkData);

               errors++;
               break;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &errors, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    if (errors)
    {
        HCCL_ERROR("%d errors. Test FAILED.\n", errors);
    }
    else
    {
        HCCL_INFO("Test PASSED.\n");
    }

  //  EXPECT_EQ(errors, 0);

    for (s32 j = 0; j < ndev; j++)
    {
        hrtFree(hostDDR_buff[j]);
        hrtFree(deviceHBM_buff[j]);

        rt_ret = rtStreamDestroy(stream[j]);

        EXPECT_EQ(rt_ret, RT_ERROR_NONE);
    }
    remove(file_name_t);
}
