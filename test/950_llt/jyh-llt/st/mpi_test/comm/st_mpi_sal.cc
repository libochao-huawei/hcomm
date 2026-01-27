#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>


#include "sal.h"
#include "adapter_rts.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include "hccl_comm_pub.h"
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_ge.h"
#include "mem_name_repository_pub.h"

using namespace std;
using namespace hccl;
constexpr int32_t INVALID_PAGESIZE = -1;

class MPI_SAL_Test : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_SAL_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_SAL_Test TearDown" << std::endl;
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
        std::cout << "A TestCase SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        MPI_Barrier(MPI_COMM_WORLD);
        std::cout << "A TestCase TearDown" << std::endl;
    }
};

/* MPI机制测试用例 */
TEST_F(MPI_SAL_Test, ut_mpi_sal_get_unique_id_test_01)
{
    s32 size, rank;
    char rootInfo[SAL_UNIQUE_ID_BYTES];
    s32 ret = SAL_OK;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HCCL_INFO("ut_mpi_sal_get_unique_id_test_01; rank[%d] size[%d]", rank, size);

    ret = SalGetUniqueId(rootInfo);
    EXPECT_EQ(ret, SAL_OK);
    HCCL_INFO("ut_mpi_sal_get_unique_id_test_01; rank[%d] rootInfo[%s]", rank, rootInfo);

    ret = SalGetUniqueId(rootInfo);
    EXPECT_EQ(ret, SAL_OK);
    HCCL_INFO("ut_mpi_sal_get_unique_id_test_01; rank[%d] rootInfo[%s]", rank, rootInfo);

    HCCL_INFO("ut_mpi_sal_get_unique_id_test_01: rank[%d] exit", rank);
}

/* 跨进程ipc memory相关接口(设置、打开、关闭) */

/* 一个进程申请设备内存，设置初始值，rtIpcSetMemoryName后，
其他进程用该名称rtIpcOpenMemory，读取数据预期与初始值一致 */
TEST_F(MPI_SAL_Test, ut_mpi_ipc_memory_set_open_close_1)
{
    s32 ndev, rank;
    s32 err_cnt;
    void* addr;
    void* relate_ptr;
    HcclResult h_ret;
    const u64 count = 50;
    HcclResult ret;
    u64 size = 0;
    SecIpcName_t secIpcName;

    MPI_Comm_size(MPI_COMM_WORLD, &ndev);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    s32 pid = 0; 
    u32 tmppid = 0;
    if (0 != rank) {
        ret = SalGetBareTgid(&tmppid);    // 当前进程id
        EXPECT_EQ(ret, HCCL_SUCCESS);
        pid = tmppid;
    }
    MPI_Bcast(&pid, sizeof(pid), MPI_BYTE, 1, MPI_COMM_WORLD);
    
    MPI_Barrier(MPI_COMM_WORLD);

    /* 执行进程间send receive */
    if (0 == rank)
    {
        h_ret = hrtMalloc(&addr, count*sizeof(s32));
        EXPECT_EQ(h_ret, HCCL_SUCCESS);

        for (int i = 0; i < count; i++)
        {
            ((s32*)addr)[i] = i;
        }

        size = (u64)count*sizeof(s32);
        MOCKER(hrtGetPointAttr)
        .expects(atMost(1))
        .will(returnValue(HCCL_E_INTERNAL));
        ret = hrtDevMemAlignWithPage(addr, size);
        GlobalMockObject::verify();
        ret = hrtDevMemAlignWithPage(addr, size);
        
        h_ret = hrtIpcSetMemoryName(addr, secIpcName.ipcName, size, sizeof(secIpcName.ipcName));
        EXPECT_EQ(h_ret, HCCL_SUCCESS);
        h_ret = hrtIpcSetMemoryPid(secIpcName.ipcName, &pid, 1);
        EXPECT_EQ(h_ret, HCCL_SUCCESS);        
    }

    MPI_Bcast(&secIpcName, sizeof(secIpcName), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (0 != rank)
    {
        h_ret = hrtIpcOpenMemory(&addr, secIpcName.ipcName);
        EXPECT_EQ(h_ret, HCCL_SUCCESS);

        err_cnt = 0;
        for (int i = 0; i < count; i++)
        {
            if (((s32*)addr)[i] != i)
            {
                err_cnt++;
            }
        }

        EXPECT_EQ(err_cnt, 0);

        h_ret = hrtIpcCloseMemory(addr);
        EXPECT_EQ(h_ret, HCCL_SUCCESS);
    }
    if (rank == 0) {
        SaluSleep(10000);
        hrtFree(addr);
    }

}

/* 已经free的内存不能再在别的进程用rtIpcOpenMemory打开 */
TEST_F(MPI_SAL_Test, ut_mpi_ipc_memory_set_open_close_2)
{
    s32 ndev, rank;
    s32 err_cnt;
    void* addr;
    void* relate_ptr;
    HcclResult h_ret;
    const u64 count = 50;
    HcclResult ret;
    u64 size = 0;
    HcclRootInfo commId;

    MPI_Comm_size(MPI_COMM_WORLD, &ndev);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    ret = hcclComm::GetUniqueId(&commId);
    MPI_Bcast(&commId, sizeof(commId), MPI_BYTE, 0, MPI_COMM_WORLD);

    s32 pid = 0;
    u32 tmppid = 0;
    if (0 != rank) {
        ret = SalGetBareTgid(&tmppid);    // 当前进程id
        EXPECT_EQ(ret, HCCL_SUCCESS);
        pid = tmppid;
    }
    MPI_Bcast(&pid, sizeof(pid), MPI_BYTE, 1, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    
    /* 执行进程间send receive */
    if (0 == rank)
    {
        h_ret = hrtMalloc(&addr, count*sizeof(s32));
        EXPECT_EQ(h_ret, HCCL_SUCCESS);

        for (int i = 0; i < count; i++)
        {
            ((s32*)addr)[i] = i;
        }

        size = (u64)count*sizeof(s32);
        ret = hrtDevMemAlignWithPage(addr, size);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        h_ret = hrtIpcSetMemoryName(addr, reinterpret_cast<u8 *>(commId.internal), size, sizeof(commId.internal));
        EXPECT_EQ(h_ret, HCCL_SUCCESS);

        h_ret = hrtIpcSetMemoryPid(reinterpret_cast<u8 *>(commId.internal), &pid, 1);
        EXPECT_EQ(h_ret, HCCL_SUCCESS);

        hrtFree(addr);

        /* MPI同步 */
        MPI_Barrier(MPI_COMM_WORLD);

    }

    if (0 != rank)
    {
        /* MPI同步 */
        MPI_Barrier(MPI_COMM_WORLD);

        h_ret = hrtIpcOpenMemory(&addr, (u8*)commId.internal);
        EXPECT_NE(h_ret, HCCL_SUCCESS);

    }

}

typedef struct ipc_test_s
{
    u8* name;
    s32 count;
    const void* ptr;
}ipc_test_t;

void* rt_ipc_open_read(void* para)
{
    HcclResult ret = HCCL_SUCCESS;
    void* addr;
    ipc_test_t* para_info = (ipc_test_t*)para;
    
    s32 pid = 0;
    u32 tmppid = 0;
    ret = SalGetBareTgid(&tmppid);    // 当前进程id
    EXPECT_EQ(ret, HCCL_SUCCESS);
    pid = tmppid;

    ret = hrtIpcSetMemoryPid(para_info->name, &pid, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hrtIpcOpenMemory(&addr, para_info->name);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    s32 err_cnt = 0;
    for (int i = 0; i < para_info->count; i++)
    {
        if (((s32*)addr)[i] != i)
        {
            err_cnt++;
        }
    }

    s32 rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    HCCL_INFO("rank[%d] sub-thread open memory err_cnt[%d]", rank, err_cnt);

    EXPECT_EQ(err_cnt, 0);

    //ret = hrtIpcCloseMemory(addr, offset);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    return (NULL);
}

TEST_F(MPI_SAL_Test, ut_mpi_ipc_memory_set_open_close_4)
{
    s32 ndev, rank;
    s32 err_cnt;
    const s32 mem_num = 10;
    const u64 count = 50;
    void* addr[mem_num];
    void* relate_ptr;
    HcclResult h_ret;
    HcclResult ret;
    u8 name[mem_num][50] = {"test_names0", "test_names1", "test_names2", "test_names3", "test_names4",
                              "test_names5", "test_names6", "test_names7", "test_names8", "test_names9"};
    //char name[mem_num][count] = {"test_names0"};
    u64 size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &ndev);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    s32 pid = 0;
    u32 tmppid = 0;
    if (0 != rank) {
        h_ret = SalGetBareTgid(&tmppid);    // 当前进程id
        EXPECT_EQ(h_ret, HCCL_SUCCESS);
        pid = tmppid;
    }
    MPI_Bcast(&pid, sizeof(pid), MPI_BYTE, 1, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    /* 执行进程间send receive */
    if (0 == rank)
    {
        for (int i= 0; i < mem_num; i++)
        {
            h_ret = hrtMalloc(&(addr[i]), count*sizeof(s32));
            EXPECT_EQ(h_ret, HCCL_SUCCESS);

            for (int j = 0; j < count; j++)
            {
                ((s32*)addr[i])[j] = i * count + j;
            }

            size = (u64)count*sizeof(s32);
            ret = hrtDevMemAlignWithPage(addr[i], size);
            EXPECT_EQ(ret, HCCL_SUCCESS);

            h_ret = hrtIpcSetMemoryName(addr[i], name[i], size, sizeof(name[i]));
            EXPECT_EQ(h_ret, HCCL_SUCCESS);

            h_ret = hrtIpcSetMemoryPid(name[i], &pid, 1);
            EXPECT_EQ(h_ret, HCCL_SUCCESS);
        }
    }

    /* MPI同步 */

    for (int i= 0; i < mem_num; i++)
    {
        MPI_Bcast(&name[i], sizeof(name[i]), MPI_BYTE, 0, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (0 != rank)
    {
        void* new_addr[mem_num];
        for (int i= 0; i < mem_num; i++)
        {
            h_ret = hrtIpcOpenMemory(&(new_addr[i]), name[i]);
            EXPECT_EQ(h_ret, HCCL_SUCCESS);

            for (int j = 0; j < count; j++)
            {
               EXPECT_EQ( ((s32*)new_addr[i])[j], i * count + j);
            }
        }
        for(int i= 0; i < mem_num; i++) {
            h_ret = hrtIpcCloseMemory(new_addr[i]);
        }
    }
    if (0 == rank) {
        /* 等待其他进程读取完成 */
        SaluSleep(SAL_MILLISECOND_USEC * 100);

        for (int i= 0; i < mem_num; i++)
        {
            hrtFree(addr[i]);
        }
    }
}
