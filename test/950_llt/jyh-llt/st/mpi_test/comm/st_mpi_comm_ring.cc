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
#include "llt_hccl_stub.h"
#include <sys/mman.h>
#include <fcntl.h>

#include "comm_impl.h"

#include <vector>
#include "comm_factory_pub.h"

#include "dlra_function.h"
#include "profiler_manager.h"
#include "network_manager_pub.h"
#include "env_config.h"

using namespace std;
using namespace hccl;

class MPI_CommInnerRing_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_CommInnerRing_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_CommInnerRing_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        static s32 call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "ST_MPI_TEST");
        ra_set_work_mode(1);
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "MPI_CommInnerRing_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        ra_set_work_mode(0);
        std::cout << "MPI_CommInnerRing_Test TearDown" << std::endl;
    }
};

void* InitRaNicResourceRing(u32 deviceId, const char *deviceIp)
{
    HcclResult ret;
    HcclIpAddress ipaddr(deviceIp);
    CHK_PRT_RET(ipaddr.IsInvalid(), HCCL_ERROR("ip[%s] change failed", deviceIp), nullptr);

    struct rdev nicRdevInfo;
    nicRdevInfo.phyId = deviceId;
    nicRdevInfo.family = ipaddr.IsIPv6() ? AF_INET6 : AF_INET;
    nicRdevInfo.localIp.addr = ipaddr.GetBinaryAddress().addr;
    nicRdevInfo.localIp.addr6 = ipaddr.GetBinaryAddress().addr6;
    void *socketHandle = nullptr;
    ret = hrtRaSocketInit(NETWORK_OFFLINE, nicRdevInfo, socketHandle);
    CHK_PRT_RET(ret, HCCL_ERROR("errNo[0x%016llx] deviceId[%d] init nic resource ring failed", HCCL_ERROR_CODE(ret), deviceId), nullptr);
    return socketHandle;
}

void* InitRaVnicResourceRing(u32 deviceId)
{
    struct rdev nicRdevInfo;
    nicRdevInfo.phyId = deviceId;
    nicRdevInfo.family = AF_INET;
    nicRdevInfo.localIp.addr.s_addr = deviceId;
    void *socketHandle = nullptr;
    HcclResult ret = hrtRaSocketInit(NETWORK_OFFLINE, nicRdevInfo, socketHandle);
    CHK_PRT_RET(ret, HCCL_ERROR("errNo[0x%016llx] deviceId[%d] init nic resource ring failed", HCCL_ERROR_CODE(ret), deviceId), nullptr);
    return socketHandle;
}
