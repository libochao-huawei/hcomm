#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <runtime/rt.h>
#include <assert.h>
#include <securec.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlra_function.h"

#define private public
#define protected public
#include "topoinfo_detect.h"
#undef protected
#undef private

#include <hccl/hccl.h>
#include "hccl_comm_pub.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_ge.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "externalinput.h"
#include "hcom_private.h"

using namespace std;
using namespace hccl;
class MPI_TOPO_DETECT_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "MPI_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        GlobalMockObject::verify();
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MPI_Barrier(MPI_COMM_WORLD);
        static s32  call_cnt = 0;
        string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());

        ra_set_test_type(0, "ST_MPI_TOPODETECT_TEST");
        ResetInitState();
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};

TEST_F(MPI_TOPO_DETECT_Test, st_hcclMasterInfo_single_server_2p_success_normal_opretry)
{
    HcclResult ret;
    s32 nnode, rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &nnode); // nnode 为mpi进程数，即服务器个数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    setenv("HCCL_OP_RETRY_ENABLE", "L0:1, L1:1, L2:1", 1);
    setenv("HCCL_WHITELIST_DISABLE", "1", 1);
    setenv("HCCL_IF_IP", "127.0.0.1", 1);
    setenv("HCCL_BUFFSIZE", "200", 1);
    string masterIp = "127.0.0.1";
    string masterPort = "6000";
    string masterID = "0";
    string rankIp = "127.0.0.1";
    string rankSize = "2";

    NetworkManager::GetInstance(0).hostNicInitRef_.Clear();
    ret = HcomInitByMasterInfo(masterIp, masterPort, masterID, rankSize, rankIp);
    EXPECT_EQ(ret, HCCL_SUCCESS);


    MPI_Barrier(MPI_COMM_WORLD);

    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ResetInitState();
    unsetenv("HCCL_WHITELIST_DISABLE");
    unsetenv("HCCL_IF_IP");
    unsetenv("HCCL_BUFFSIZE");
    
}