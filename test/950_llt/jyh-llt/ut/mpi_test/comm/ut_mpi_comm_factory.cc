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


#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>
#include <runtime/rt.h>


#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>

#include "comm_factory.h"
#include "profiler_manager.h"

using namespace std;
using namespace hccl;

class MPI_CommFactory_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_CommFactory_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_CommFactory_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");
        std::cout << "MPI_CommFactory_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "MPI_CommFactory_Test TearDown" << std::endl;
    }
};

TEST_F(MPI_CommFactory_Test, ut_mpi_comm_factory_init)
{
    s32 ret;
    s32 size, rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HcclRootInfo rootInfo;

    ret = hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // set device
    s32 device_id = 0;
    ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MPI_Bcast(&rootInfo, sizeof(rootInfo), MPI_BYTE, 0, MPI_COMM_WORLD);
    // 创建dispatcher
    void *dispatcher = nullptr;
    ret = HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, device_id, &dispatcher);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(dispatcher, nullptr);

    std::vector<RankInfo> rank_vector;
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;


    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    rank_vector.push_back(tmp_para_0);
    rank_vector.push_back(tmp_para_1);

    std::shared_ptr<CommFactory> comm_factory = nullptr;
    std::map<HcclIpAddress, HcclNetDevCtx> netDevCtxMap;

    std::shared_ptr<TopoInfoExtractor> topoInfoExt;
    topoInfoExt.reset(new TopoInfoExtractor(rootInfo.internal, rank, size, TopoType::TOPO_TYPE_COMMON,
        DevType::DEV_TYPE_910, rank_vector));

    comm_factory.reset(new CommFactory(rootInfo.internal, rank, size, dispatcher, nullptr, netDevCtxMap, topoInfoExt,
        true, TopoType::TOPO_TYPE_COMMON, DevType::DEV_TYPE_910, rank_vector));

    ret = comm_factory->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    if (dispatcher != nullptr) {
        ret = HcclDispatcherDestroy(dispatcher);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        dispatcher = nullptr;
    }
    MPI_Barrier(MPI_COMM_WORLD);//mpi通信域所有进程同步
}

