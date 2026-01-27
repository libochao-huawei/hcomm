#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mpi.h>

#include <driver/ascend_hal.h>
#include <runtime/rt.h>


#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "stream_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "hccl_comm_pub.h"
#include "sal.h"
#include "hccl_impl.h"
#include "llt_hccl_stub_pub.h"
#include "config.h"
#include "topoinfo_ranktableParser_pub.h"

#include "externalinput_pub.h"
#include "hcom_ops_kernel_info_store.h"
#include "plugin_manager.h"
#include "external/ge/ge_api_types.h" // ge对内options
#include "framework/common/ge_types.h" // ge对外options

#include <iostream>
#include <fstream>

using namespace std;
using namespace hccl;
class HcomKernelInfoTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MPI_Barrier(MPI_COMM_WORLD);
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


class NodeTest : public ge::Node {
public:
    NodeTest(){;};
    ~NodeTest(){;};    
};


#define P2P_DATA_SIZE_LIGHT 20
#define P2P_DATA_SIZE_HEAVY 1200000
#define P2P_DATA_SIZE_S_HEAVY 30000000   /* 超大数据，约10M */

TEST_F(HcomKernelInfoTest, st_LoadTask_rec_send)
{
      // ranktable 的读取，直接使用进程
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "1"},
        {"para_plane_nic_name", {"eth0"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "2"},
                    {"server_num", "1"},
                    {"instance_count", "2"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };

    char file_name_t[] = "./st_mpi_send_receive_2ranks_2server_float.json";
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

    s32 ndev, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s32 count = P2P_DATA_SIZE_S_HEAVY;
    const s32 send_rank_id = 0;
    const s32 recv_rank_id = 1;
    char tag[] = "default_tag";
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    float* buffer;
    ret = hrtMalloc((void**)&buffer, (count * sizeof(float)));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memset(buffer, (count * sizeof(float)), 0, (count * sizeof(float)));
    rtStream_t stream;
    rt_ret = rtStreamCreate(&stream, 5);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    ra_set_work_mode(1);

    MPI_Comm_size(MPI_COMM_WORLD, &ndev);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    std::ostringstream rank_id("");
    rank_id << rank;
    HCCL_INFO("rank id:[%s]",rank_id.str().c_str());

    ge ::Status ge_ret;
    std::map<std::string,std::string> options;
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"./st_mpi_send_receive_2ranks_2server_float.json"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,rank_id.str()));
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));
    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);
    
    std::map<string, OpsKernelInfoStorePtr> opKernInfos;
    std::map<string, GraphOptimizerPtr> graphOptimizers;
    GetOpsKernelInfoStores(opKernInfos);
    GetGraphOptimizerObjs(graphOptimizers);
    OpsKernelInfoStorePtr opsKernerInfoStorePtr = opKernInfos.at(HCCL_OPS_LIB_NAME);
    ge_ret = opsKernerInfoStorePtr->Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);


    if (rank == send_rank_id)
    {
        ge::NodePtr nodeptr(new NodeTest);
        // ge::OpDesc op;
        std::string reason;
        std::string type = HCCL_KERNEL_OP_TYPE_SEND;
        nodeptr->GetOpDesc()->SetType(type);
        bool bRet = opsKernerInfoStorePtr->CheckSupported(nodeptr->GetOpDesc(), reason);
        EXPECT_EQ(bRet, true);

        HCCL_INFO("test GetAllOpsKernelInfo.");
        std::map<string, ge::OpInfo> infos;
        opsKernerInfoStorePtr->GetAllOpsKernelInfo(infos);

        
        s32 ge_ret = ge::SUCCESS;
        
        for (u32 i = 0; i < count; i++)
        {
            buffer[i] = i;
        }

        HcomOpsKernelInfoStore hcomKernelInfo;        
                
        ge::GETaskInfo task;
        HCCL_KERNEL_INFO_PRIVATE_DEF privateDefBuf;
        strcpy_s((char *)privateDefBuf.group, 128, "hccl_world_group");
        privateDefBuf.nodeNameHash = 123456789;
        privateDefBuf.graphId = 1;
        privateDefBuf.destRank = 1;
        privateDefBuf.originalGraphShapeType = 0;
        privateDefBuf.tensorNum = 0;
        privateDefBuf.privateDefSize = sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF);
        privateDefBuf.originalGraphShapeType = ORIGINAL_GRAPH_UNKNOWNSHAPE_TYPE;
        privateDefBuf.aivCoreLimit = 48;

        task.type = RT_MODEL_TASK_HCCL;
        task.stream = stream;
        task.privateDef = (void *)&privateDefBuf.group[0];
        task.privateDefLen = sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF);

 
        u64 memSize = HCCL_WORKSPACE_MEM_32_KB;
        DeviceMem workSpaceMem = DeviceMem::alloc(memSize);

        u32 rankId = 0;
        MOCKER(HcomGetRankId)
        .stubs()
        .with(any(), outBound(&rankId))
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtMemAsyncCopy)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtMemcpyAddrAsync)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        ge_ret = opsKernerInfoStorePtr->LoadTask(task);
        EXPECT_EQ(ge_ret, ge::SUCCESS);

        GlobalMockObject::verify();
    }

    if (rank == recv_rank_id)
    {
        ge::NodePtr nodeptr(new NodeTest);
        // ge::OpDesc op;
        std::string reason;
        std::string type = HCCL_KERNEL_OP_TYPE_RECEIVE;
        nodeptr->GetOpDesc()->SetType(type);
        bool bRet = opsKernerInfoStorePtr->CheckSupported(nodeptr->GetOpDesc(), reason);
        EXPECT_EQ(bRet, true);

        HCCL_INFO("test GetAllOpsKernelInfo.");
        std::map<string, ge::OpInfo> infos;
        opsKernerInfoStorePtr->GetAllOpsKernelInfo(infos);
        s32 ge_ret = ge::SUCCESS;
        
        for (u32 i = 0; i < count; i++)
        {
            buffer[i] = i;
        }

        HcomOpsKernelInfoStore hcomKernelInfo;        
            
        ge::GETaskInfo task;
        HCCL_KERNEL_INFO_PRIVATE_DEF privateDefBuf;
        strcpy_s((char *)privateDefBuf.group, 128, "hccl_world_group");
        privateDefBuf.nodeNameHash = 1234567890;
        privateDefBuf.graphId = 1;
        privateDefBuf.destRank = 1;
        privateDefBuf.originalGraphShapeType = 0;
        privateDefBuf.tensorNum = 0;
        privateDefBuf.privateDefSize = sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF);
        privateDefBuf.originalGraphShapeType = ORIGINAL_GRAPH_UNKNOWNSHAPE_TYPE;
        privateDefBuf.aivCoreLimit = 48;

        task.type = RT_MODEL_TASK_HCCL;
        task.stream = stream;
        task.privateDef = (void *)&privateDefBuf.group[0];
        task.privateDefLen = sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF);


        u64 memSize = HCCL_WORKSPACE_MEM_32_KB;
        DeviceMem workSpaceMem = DeviceMem::alloc(memSize);
 
        u32 rankId = 1;
        MOCKER(HcomGetRankId)
        .stubs()
        .with(any(), outBound(&rankId))
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtMemAsyncCopy)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtMemcpyAddrAsync)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));


        ge_ret = opsKernerInfoStorePtr->LoadTask(task);
        EXPECT_EQ(ge_ret, ge::SUCCESS);
        GlobalMockObject::verify();
    }

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == recv_rank_id)
    {
        for (int j = 0; j < count; j++)
        {
            if (abs(buffer[j] - j)>1e-6)
            {
                printf(" rank %d recvbuf[%d] %f ", rank, j, buffer[j]);
                errors ++;
            }
        }
        EXPECT_EQ(errors, 0);
        if (errors != 0)
        {
            printf("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            printf("Test PASSED.\n");
        }
        remove(file_name_t);
    }

    ge_ret = opsKernerInfoStorePtr->Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    ge_ret = Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    hrtFree(buffer);
    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

}

TEST_F(HcomKernelInfoTest, st_LoadTask_overflow)
{
      // ranktable 的读取，直接使用进程
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"deploy_mode", "lab"},
        {"group_count", "1"},
        {"chip_info", "910"},
        {"board_id", "0x0000"},
        {"para_plane_nic_location", "device"},
        {"para_plane_nic_num", "1"},
        {"para_plane_nic_name", {"eth0"}},
        {
            "group_list",
            {
                {
                    {"group_name", ""},
                    {"device_num", "2"},
                    {"server_num", "1"},
                    {"instance_count", "2"},
                        {
                            "instance_list",
                            {
                                {   {"rank_id", "0"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "0"}, {"device_ip", "192.168.0.12"}}}
                                    }
                                },

                                {   {"rank_id", "1"}, {"server_id", "10.0.0.10"},
                                    {
                                        "devices", {{{"device_id", "1"}, {"device_ip", "192.168.0.14"}}}
                                    }
                                },
                            }
                        },
                        {
                            "server_list",
                            {
                                {
                                    {"server_id", "192.168.10.2"},
                                    {
                                        "para_plane_info",
                                        {{
                                                {"eth1", "192.168.210.2"},
                                            },
                                            {
                                                {"eth0", "192.168.200.2"},
                                            }
                                        }
                                    }

                                },
                            }
                        }
                }
            }
        }
    };
    s8* sendbuf;
    s8* recvbuf;
    char file_name_t[] = "./st_mpi_send_receive_2ranks_2server_float.json";
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

    s32 ndev, rank, errors = 0;
    int ret = HCCL_SUCCESS;
    rtError_t rt_ret = RT_ERROR_NONE;
    s32 count = P2P_DATA_SIZE_LIGHT;
    const s32 send_rank_id = 0;
    const s32 recv_rank_id = 1;
    char tag[] = "default_tag";
    HcclDataType dataType = HCCL_DATA_TYPE_FP32;
    float* buffer;
    ret = hrtMalloc((void**)&buffer, (count * sizeof(float)));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    sal_memset(buffer, (count * sizeof(float)), 0, (count * sizeof(float)));

    sendbuf= (s8*)sal_malloc(count+512);
     sal_memset(sendbuf, count+512 , 0, count+512 );
    recvbuf= (s8*)sal_malloc(count+512);
     sal_memset(recvbuf, count+512 , 0, count+512 );

    for (int j = 0; j < count; j++)
    {
        sendbuf[j+128] = 2;
    }

    rtStream_t stream;
    rt_ret = rtStreamCreate(&stream, 5);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    ra_set_work_mode(1);

    MPI_Comm_size(MPI_COMM_WORLD, &ndev);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    hrtSetDevice(rank);

    std::ostringstream rank_id("");
    rank_id << rank;
    HCCL_INFO("rank id:[%s]",rank_id.str().c_str());

    ge ::Status ge_ret;
    std::map<std::string,std::string> options;
    options.insert(pair<string,string> (ge::OPTION_EXEC_IS_USEHCOM,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_TABLE_FILE,"./st_mpi_send_receive_2ranks_2server_float.json"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_RANK_ID,rank_id.str()));
    options.insert(pair<string,string> (ge::OPTION_EXEC_DEPLOY_MODE,"0"));
    options.insert(pair<string,string> (ge::OPTION_GRAPH_RUN_MODE,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_ENABLE_DUMP_DEBUG,"1"));
    options.insert(pair<string,string> (ge::OPTION_EXEC_PROFILING_MODE,"0"));

    ret = Initialize(options);
    EXPECT_EQ(ret, ge::SUCCESS);
    
    std::map<string, OpsKernelInfoStorePtr> opKernInfos;
    std::map<string, GraphOptimizerPtr> graphOptimizers;
    GetOpsKernelInfoStores(opKernInfos);
    GetGraphOptimizerObjs(graphOptimizers);
    OpsKernelInfoStorePtr opsKernerInfoStorePtr = opKernInfos.at(HCCL_OPS_LIB_NAME);
    ge_ret = opsKernerInfoStorePtr->Initialize(options);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    ge::NodePtr nodeptr(new NodeTest);
    // ge::OpDesc op;
    std::string reason;
    std::string type = HCCL_KERNEL_OP_TYPE_ALLREDUCE;
    nodeptr->GetOpDesc()->SetType(type);
    bool bRet = opsKernerInfoStorePtr->CheckSupported(nodeptr->GetOpDesc(), reason);
    EXPECT_EQ(bRet, true);

    HCCL_INFO("test GetAllOpsKernelInfo.");
    std::map<string, ge::OpInfo> infos;
    opsKernerInfoStorePtr->GetAllOpsKernelInfo(infos);
    for (u32 i = 0; i < count; i++)
    {
        buffer[i] = i;
    }

    HcomOpsKernelInfoStore hcomKernelInfo;        
                
    ge::GETaskInfo task;
    HCCL_KERNEL_INFO_PRIVATE_DEF privateDefBuf;
    strcpy_s((char *)privateDefBuf.group, 128, "hccl_world_group");
    privateDefBuf.nodeNameHash = 123456789;
    privateDefBuf.graphId = 1;
    privateDefBuf.destRank = 1;
    privateDefBuf.originalGraphShapeType = 0;
    privateDefBuf.dataType = dataType;
    privateDefBuf.tensorNum = 0;
    privateDefBuf.privateDefSize = sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF);
    privateDefBuf.aivCoreLimit = 48;
    task.type = RT_MODEL_TASK_HCCL;
    task.stream = stream;
    task.privateDef = (void *)&privateDefBuf.group[0];
    task.privateDefLen = sizeof(HCCL_KERNEL_INFO_PRIVATE_DEF);

    u64 memSize = HCCL_WORKSPACE_MEM_32_KB;
    DeviceMem workSpaceMem = DeviceMem::alloc(memSize);

    u32 rankId = 0;
    MOCKER(HcomGetRankId)
    .stubs()
    .with(any(), outBound(&rankId))
    .will(returnValue(HCCL_SUCCESS));

    ge_ret = opsKernerInfoStorePtr->LoadTask(task);
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    GlobalMockObject::verify();

    rt_ret = rtStreamSynchronize(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == recv_rank_id)
    {
        for (int j = 0; j < count; j++)
        {
            if (abs(buffer[j] - j)>1e-6)
            {
                printf(" rank %d recvbuf[%d] %f ", rank, j, buffer[j]);
                errors ++;
            }
        }
        EXPECT_EQ(errors, 0);
        if (errors != 0)
        {
            printf("%d errors. Test FAILED.\n", errors);
        }
        else
        {
            printf("Test PASSED.\n");
        }
        remove(file_name_t);
    }

    sal_free(sendbuf);
    sal_free(recvbuf);

    ge_ret = opsKernerInfoStorePtr->Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    ge_ret = Finalize();
    EXPECT_EQ(ge_ret, ge::SUCCESS);

    hrtFree(buffer);
    rt_ret = rtStreamDestroy(stream);
    EXPECT_EQ(rt_ret, RT_ERROR_NONE);
}
