#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#define __HCCL_SAL_GLOBAL_RES_INCLUDE__

#define private public
#define protected public
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "alltoall_operator.h"
#undef private
#undef protected

#include "sal.h"     //FIXME!! why include private .h files //FIXME!! why include private .h files
#include "llt_hccl_stub_pub.h"
#include "alltoallv_staged_calculator_pub.h"


#include <hccl/hccl_types.h>
#include <hccl/hccl.h>
#include <semaphore.h>
#include <sys/time.h> /* 获取时间 */
#include <sys/mman.h>
#include <fcntl.h>
#include <securec.h>
#include <dirent.h>

using namespace std;
using namespace hccl;

class AlltoAllVStagedCalculatorTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AlltoAllVStagedCalculatorTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AlltoAllVStagedCalculatorTest TearDown" << std::endl;
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
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(AlltoAllVStagedCalculatorTest, ut_alltoallv_staged_static)
{
    AlltoAllUserRankInfo userRankInfo;
    userRankInfo.userRankSize = 12;
    userRankInfo.userRank = 5;

    u32 userRankSize = 12;
    u32 userRank = 5;

    // 生成数据
    std::vector<SendRecvInfo> allSendRecvInfo;
    for (u32 i = 0; i < userRankSize; i++) {
        SendRecvInfo curSendRecvInfo;
        curSendRecvInfo.sendLength.resize(userRankSize);
        curSendRecvInfo.sendOffset.resize(userRankSize);
        curSendRecvInfo.recvLength.resize(userRankSize);
        curSendRecvInfo.recvOffset.resize(userRankSize);
        u64 sdisp = 0;
        u64 rdisp = 0;
        cout << "rank " << i << endl;
        for (u32 j = 0; j < userRankSize; j++) {
            curSendRecvInfo.sendLength[j] = j + 1;
            curSendRecvInfo.sendOffset[j] = sdisp;
            sdisp += curSendRecvInfo.sendLength[j];

            curSendRecvInfo.recvLength[j] = i + 1; // 从每个rank收到的数据是一样的
            curSendRecvInfo.recvOffset[j] = rdisp;
            rdisp += curSendRecvInfo.recvLength[j];
            cout << curSendRecvInfo.sendLength[j] << "," << curSendRecvInfo.sendOffset[j] << "," <<
                curSendRecvInfo.recvLength[j] << "," << curSendRecvInfo.recvOffset[j] << endl;
        }
        allSendRecvInfo.push_back(curSendRecvInfo);
    }

    u64 workspaceMemSize = 0;
    AlltoAllVStagedCalculator::CalcWorkSpaceMemSize(userRankInfo, allSendRecvInfo, workspaceMemSize, 4);
    EXPECT_EQ(workspaceMemSize, 72);
}

class AlltoAllVStagedAlgSelectTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AlltoAllVStagedAlgSelectTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AlltoAllVStagedAlgSelectTest TearDown" << std::endl;
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
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(AlltoAllVStagedCalculatorTest, ut_alltoallv_staged_static_size_0)
{
    u32 userRankSize = 8;
    for (u32 rankIndex = 0; rankIndex < userRankSize; rankIndex++) {

        u32 userRank = rankIndex;

        AlltoAllUserRankInfo userRankInfo;
        userRankInfo.userRankSize = userRankSize;
        userRankInfo.userRank = userRank;
        // 生成数据
        std::vector<SendRecvInfo> allSendRecvInfo;
        for (u32 i = 0; i < userRankSize; i++) {
            SendRecvInfo curSendRecvInfo;
            curSendRecvInfo.sendLength.resize(userRankSize);
            curSendRecvInfo.sendOffset.resize(userRankSize);
            curSendRecvInfo.recvLength.resize(userRankSize);
            curSendRecvInfo.recvOffset.resize(userRankSize);

            for (u32 j = 0; j < userRankSize; j++) {
                curSendRecvInfo.sendLength[j] = 0;
                curSendRecvInfo.sendOffset[j] = 0;
                curSendRecvInfo.recvLength[j] = 0;
                curSendRecvInfo.recvOffset[j] = 0;
            }
            allSendRecvInfo.push_back(curSendRecvInfo);
        }

        u64 workspaceMemSize = 0;
        AlltoAllVStagedCalculator::CalcWorkSpaceMemSize(userRankInfo, allSendRecvInfo, workspaceMemSize, 1);
        EXPECT_EQ(workspaceMemSize, 2 * 1024 * 1024);
    }
}