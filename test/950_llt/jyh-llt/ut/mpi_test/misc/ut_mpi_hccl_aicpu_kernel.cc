#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>
#include <fcntl.h>

#include "sal.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_comm_pub.h"
#include "llt_hccl_stub_pub.h"

using namespace std;
using namespace hccl;

class MPI_HCCL_CPU_KERNEL_CPU_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_HCCL_CPU_KERNEL_CPU_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_HCCL_CPU_KERNEL_CPU_Test TearDown" << std::endl;
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
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};