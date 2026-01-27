#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>
#include "llt_hccl_stub_sal_pub.h"

#define private public
#define protected public

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

#include "externalinput.h"

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hcom_pub.h"
#include "hcom.h"
#include "../inc/hccl/hcom_executor.h"
#include "llt_hccl_stub_pub.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"

#include "sal.h"
#include "dlra_function.h"
#include "hccl_comm_pub.h"
#include "config.h"
#include "topo/topoinfo_ranktableParser_pub.h"
#include "externalinput_pub.h"
#include "v80_rank_table.h"
#include "dltdt_function.h"
#include <iostream>
#include <fstream>
#include "opexecounter_pub.h"
#include "hccl_communicator.h"
#include "op_base.h"
#include "env_config.h"
#include "heartbeat.h"
#include "topoinfo_exchange_agent.h"

#include "param_check_pub.h"
#undef private

using namespace std;
using namespace hccl;

class MPI_AllGatherMix_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_AllGatherMix_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_AllGatherMix_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1, 2);
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_MIX_TEST");
        ra_set_work_mode(0);
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        MOCKER_CPP(&Heartbeat::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::RegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Heartbeat::UnRegisterRanks)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
        std::cout << "A MPI_MIX_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        MPI_Barrier(MPI_COMM_WORLD);
        GlobalMockObject::verify();
        std::cout << "A MPI_MIX_Test TearDown" << std::endl;
    }
};

#define HCCL_COM_DATA_SIZE 1024
#define DEV_NUM_8 8

HcclResult FakeHrtGetDeviceType(DevType &devType)
{
    s32 rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        devType = DevType::DEV_TYPE_910B;
    } else {
        devType = DevType::DEV_TYPE_910_93;
    }
    
    return HCCL_SUCCESS;
}
