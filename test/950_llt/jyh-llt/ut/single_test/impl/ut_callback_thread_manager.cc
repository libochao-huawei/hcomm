#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

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

#include "dlra_function.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "rank_consistentcy_checker.h"
#include "../inc/hccl/hccl_ex.h"
#include "externalinput_pub.h"
#include "callback_thread_manager.h"
#include "hccl_communicator.h"

using namespace std;
using namespace hccl;

class MPI_ThreadStreamManager_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_ThreadStreamManager_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_ThreadStreamManager_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(MPI_ThreadStreamManager_Test, ut_thread_stream_manager)
{
    HCCL_INFO("ut_stream_has_been_reged start....");
    int i = 1;
    int j = 2;
    rtStream_t key = NULL;
    bool result = true;
    HcclResult ret = HCCL_SUCCESS;

    result = ThreadStreamManager::Instance().StreamHasBeenReged(&i);
    EXPECT_EQ(result, false);
    ret = ThreadStreamManager::Instance().RegTidAndStream(i, &i);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    result = ThreadStreamManager::Instance().StreamHasBeenReged(&i);
    EXPECT_EQ(result, true);
    ret = ThreadStreamManager::Instance().GetStreamByTid(i, key);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ThreadStreamManager::Instance().ReleaseTidAndStream(key);
    ret = ThreadStreamManager::Instance().GetStreamByTid(10, key);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    GlobalMockObject::verify();
}
