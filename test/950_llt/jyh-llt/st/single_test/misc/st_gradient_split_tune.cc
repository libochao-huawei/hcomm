#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <cstdio>
#include <slog.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include<iostream>
#include<vector>
#include<algorithm>
#include <cmath>
#include <climits>
#include <sstream>

#include "tune_log.h"
#include "llt_hccl_stub_pub.h"
#include "hcom_gradient_split_tune.h"
#include "gradient_split_tune.h"
#include "model.h"
#include "evaluator.h"
#include "cluster.h"
using namespace std;

extern TuneResult_t GetGradientInfo(const std::vector<struct GradientNode> &graNode,
    const std::vector<struct BPTimeInfo> &bpTimeInfo, std::vector<struct GradientInfo>& graInfo);

extern TuneResult_t GetBPTimeFromProfiling(const std::vector<std::vector<struct ProfilingMetric>> &taskProfilingList,
    std::vector<struct BPTimeInfo> &bpTimeInfo);

extern TuneResult_t GetGradientNode(const std::vector<std::vector<struct ProfilingMetric>> &taskProfilingList,
    std::string workPath, std::vector<struct GradientInfo> &graInfo);

extern TuneResult_t FileStreamRead(const std::string filePath, std::vector<struct GradientNode>& graNode);

class GradientSplitTuneTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "GradientSplitTuneTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "GradientSplitTuneTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        static uint32_t callCnt = 0;
        string name =std::to_string(callCnt++) + "_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name.c_str());
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

struct GradientDataInfo {
    unsigned long long dataSize;
    std::string dataType;
    unsigned long long graphId;
    std::string groupName;
    std::string gradientNodeName;
    std::string traceNodeName;
    std::string allReduceNodeName;
};

static uint32_t g_gradientNodeNo;

TEST_F(GradientSplitTuneTest, stGetGradientInfo)
{
    std::vector<struct GradientNode> graNodeVct;
    std::vector<struct BPTimeInfo> bpTimeInfo;
    std::vector<struct GradientInfo> graInfo;

    GradientNode graNode;
    graNode.traceNodeName = "NodeTest";
    graNode.groupName = "group";
    graNode.dataType = "fp32";
    graNode.gradientSize = 123;
    graNode.graphId = 1;
    graNodeVct.push_back(graNode);

    BPTimeInfo timeInfo;
    timeInfo.opName = "NodeTest";
    timeInfo.time = 321;
    bpTimeInfo.push_back(timeInfo);

    TuneResult_t ret;
    ret = GetGradientInfo(graNodeVct, bpTimeInfo, graInfo);
    EXPECT_EQ(ret, TUNE_SUCCESS); 
}

TEST_F(GradientSplitTuneTest, stGradientSplitTune1)
{
    TuneResult_t ret;
    vector<vector<struct ProfilingMetric>> taskProfilingList(4, vector<struct ProfilingMetric>(2, {"tune", "trace_node_name", "TensorRedirect", 0, 1}));
    std::vector<struct BPTimeInfo> bpTimeInfo;
    std::string workPath = "/tmp/";
    std::vector<GradientDataInfo> recordInfos (4, {2, "gradient_size(byte)", 12, "group_name", "graph_id", "trace_node_name", "allredice_node_name"});

    taskProfilingList[0][1].taskStartTime = 2;
    taskProfilingList[0][1].taskEndTime = 3;
    taskProfilingList[1][1].taskStartTime = 4;
    taskProfilingList[1][1].taskEndTime = 5;
    taskProfilingList[2][1].taskStartTime = 4;
    taskProfilingList[2][1].taskEndTime = 5;
    taskProfilingList[3][1].taskStartTime = 4;
    taskProfilingList[3][1].taskEndTime = 5;

    std::string gradientInfosFile = std::string(workPath) + "gradient_summary.csv";

    std::ofstream fileStream(gradientInfosFile.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    if (fileStream.is_open()) {
        for (uint32_t i = 0; i < recordInfos.size(); i++) {
            g_gradientNodeNo++;
            std::cout<<"g_gradientNodeNo:"<<g_gradientNodeNo<<endl;
            fileStream << g_gradientNodeNo << "," << recordInfos[i].dataSize << "," << recordInfos[i].dataType <<
                "," << recordInfos[i].graphId << "," << recordInfos[i].groupName << "," <<
                recordInfos[i].gradientNodeName << "," << recordInfos[i].traceNodeName << "," <<
                recordInfos[i].allReduceNodeName << std::endl;
        }
        fileStream.close();
    } else {
        std::cout<<"fileStream failed for write"<<endl;
    }

    ret = HcomGradientFusionTune(taskProfilingList, workPath);

    std::string badPath = "zhl/";
    ret = HcomGradientFusionTune(taskProfilingList, badPath);

    remove(gradientInfosFile.c_str());
}

TEST_F(GradientSplitTuneTest, utGradientSplitTune2)
{

    float ret;
    vector<int> slices;
    vector<int> slices1;
    slices.push_back(1);
    Communication com(1);
    com.CheckParams(1, "direct");
    com.CheckParams(0, "direct");
    ret = com.CalculateXferFrequency(8, "ring", slices);
    ret = com.CalculateXferPercentage(8, "ring", slices);
    ret = com.CalculateXferBubbles(8, "ring", slices);
    ret = com.CalculateComputePercentage(8, "ring", slices);

    Scatter sca(1);
    ret = sca.CalculateXferFrequency(8, "ring", slices);
    ret = sca.CalculateXferPercentage(8, "ring", slices);
    ret = sca.CalculateXferFrequency(8, "sa", slices);
    ret = sca.CalculateXferPercentage(8, "sa", slices);

    Allgather allg(1);
    ret = allg.CalculateXferFrequency(8, "ring", slices);
    ret = allg.CalculateXferFrequency(8, "H-D", slices);
    ret = allg.CalculateXferFrequency(8, "mesh-serial", slices);
    ret = allg.CalculateXferFrequency(8, "mesh", slices);
    ret = allg.CalculateXferFrequency(8, "sa", slices);
    ret = allg.CalculateXferPercentage(8, "ring", slices);
    ret = allg.CalculateXferPercentage(8, "H-D", slices);
    ret = allg.CalculateXferPercentage(8, "mesh", slices);
    ret = allg.CalculateXferPercentage(8, "direct", slices);
    ret = allg.CalculateXferPercentage(8, "mesh-serial", slices);
    ret = allg.CalculateXferPercentage(8, "sa", slices);

    Allreduce allr(1);
    ret = allr.CalculateXferFrequency(8, "ring", slices);
    ret = allr.CalculateXferFrequency(8, "tree", slices);
    ret = allr.CalculateXferFrequency(8, "2D-torus", slices);
    ret = allr.CalculateXferPercentage(8, "ring", slices);
    ret = allr.CalculateXferPercentage(8, "H-D", slices);
    ret = allr.CalculateXferPercentage(8, "tree", slices);
    ret = allr.CalculateXferPercentage(8, "2D-torus", slices);
    ret = allr.CalculateXferBubbles(8, "ring", slices);
    ret = allr.CalculateXferBubbles(8, "tree", slices);
    ret = allr.CalculateComputePercentage(8, "ring", slices);
    ret = allr.CalculateComputePercentage(8, "H-D", slices);
    ret = allr.CalculateComputePercentage(8, "tree", slices);

    Broadcast broa(1);
    ret = broa.CalculateXferFrequency(1, "ring", slices);
    ret = broa.CalculateXferFrequency(8, "ring", slices);
    ret = broa.CalculateXferFrequency(8, "H-D", slices);
    ret = broa.CalculateXferFrequency(8, "mesh-serial", slices);
    ret = broa.CalculateXferFrequency(8, "mesh", slices);

    ret = broa.CalculateXferPercentage(8, "ring", slices);
    ret = broa.CalculateXferPercentage(8, "mesh", slices);
    ret = broa.CalculateXferPercentage(8, "tree", slices);
    ret = broa.CalculateXferBubbles(1, "ring", slices);
    ret = broa.CalculateXferBubbles(8, "ring", slices);
    ret = broa.CalculateXferBubbles(8, "H-D", slices);
    ret = broa.CalculateXferBubbles(8, "mesh", slices);
    ret = broa.CalculateXferBubbles(8, "tree", slices);
    Reduce redu(1);
    ret = redu.CalculateXferFrequency(8, "ring", slices);
    ret = redu.CalculateXferFrequency(1, "ring", slices);
    ret = redu.CalculateXferFrequency(8, "H-D", slices);
    ret = redu.CalculateXferFrequency(8, "mesh", slices);
    ret = redu.CalculateXferFrequency(8, "mesh-serial", slices);
    ret = redu.CalculateXferFrequency(8, "direct", slices);
    ret = redu.CalculateXferFrequency(2, "tree", slices);
    ret = redu.CalculateXferPercentage(8, "ring", slices);
    ret = redu.CalculateXferPercentage(8, "H-D", slices);
    ret = redu.CalculateXferPercentage(8, "mesh", slices);
    ret = redu.CalculateXferPercentage(8, "mesh-serial", slices);
    ret = redu.CalculateXferPercentage(8, "direct", slices);
    ret = redu.CalculateXferPercentage(2, "tree", slices);
    ret = redu.CalculateXferBubbles(8, "ring", slices);
    ret = redu.CalculateXferBubbles(1, "ring", slices);
    ret = redu.CalculateXferBubbles(8, "H-D", slices);
    ret = redu.CalculateXferBubbles(8, "mesh", slices);
    ret = redu.CalculateXferBubbles(8, "tree", slices);
    ret = redu.CalculateComputePercentage(8, "ring", slices);
    ret = redu.CalculateComputePercentage(1, "ring", slices);
    ret = redu.CalculateComputePercentage(8, "H-D", slices);
    ret = redu.CalculateComputePercentage(8, "mesh", slices);
    ret = redu.CalculateComputePercentage(2, "tree", slices);
    Reducescatter rs(1);
    ret = rs.CalculateXferFrequency(8, "mesh", slices);
    ret = rs.CalculateXferFrequency(8, "H-D", slices);
    ret = rs.CalculateXferFrequency(8, "direct", slices);
    ret = rs.CalculateXferPercentage(8, "H-D", slices);
    ret = rs.CalculateXferPercentage(8, "mesh", slices);
    ret = rs.CalculateComputePercentage(8, "H-D", slices);
    ret = rs.CalculateComputePercentage(8, "mesh", slices);
    Sendrecv sd(1);
    MultirootReduce Mr(1);
    ret = Mr.CalculateXferPercentage(8, "H-D", slices);
    ret = Mr.CalculateXferPercentage(8, "H-D", slices1);
    ret = Mr.CalculateComputePercentage(8, "H-D", slices);
    ret = Mr.CalculateComputePercentage(8, "H-D", slices1);
    MultirootBroadcast Mb(1);
    ret = Mb.CalculateXferPercentage(8, "H-D", slices);
    ret = Mb.CalculateXferPercentage(8, "H-D", slices1);
    AlltoAll aa(1);
    ret = aa.CalculateXferFrequency(8, "H-D", slices);
    ret = aa.CalculateXferFrequency(8, "mesh", slices);
    ret = aa.CalculateXferPercentage(8, "H-D", slices);
    ret = aa.CalculateXferPercentage(8, "mesh", slices);
    ret = aa.CalculateComputePercentage(8, "ring", slices);
}

TEST_F(GradientSplitTuneTest, utCalculateCostWithJetter)
{
    nlohmann::json rank_table =
    {
        {"status", "completed"},
        {"version", "1.0"},
        {"server_count", "1"},
        {
            "server_list",
            {
                {
                    {"server_id", "10.0.0.10"},
                    {"host_nic_ip", "192.168.0.12:0,192.168.1.12:199"},
                    {
                        "device",
                        {
                            {   {"rank_id", "0"},
                                {"device_id", "0"},
                                {"device_ip", "192.168.0.12,192.168.1.12"}
                            },
                            {   {"rank_id", "1"},
                                {"device_id", "1"},
                                {"device_ip", "192.168.0.13,192.168.1.13"}
                            },
                        }
                    },
                }
            }
        }
    };

    char file_name_t[] = "./ut_hcom_get_new_rank_info_muti_ip.json";
    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << rank_table << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    int ret = HCCL_SUCCESS;


    ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 localrankid = 0;
    u32 localranksize = 0;
    char* rank_table_file = "./ut_hcom_get_new_rank_info_muti_ip.json";
    char* rank_ID = "0";

    ret = HcomInitByFile(rank_table_file, rank_ID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::string workPath="/tmp/";
    std::string clusterFile = std::string(workPath) + "cluster_info.xml";
    Cluster cl(clusterFile);
    Communication op;
    op.mName_ = "Allgather";

    float ret0 = cl.CalculateCostWithJetter(op, 0, 0.5 ,0.5);

    float ret1 = cl.CalculateCostWithJetter(op, 1, 0.5, 0.5);

    float ret2 = cl.CalculateStartUpCost(op);

    Communication ops;
    ops.mName_ = "Allreduce";
    float ret3 = cl.CalculateStartUpCost(ops);
    ret = HcomDestroy();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    remove(file_name_t);
}

extern TuneResult_t GetSocVersion(std::string &configVersion);
extern TuneResult_t GetTuneFusionPath(std::string &fusionPath);
TEST_F(GradientSplitTuneTest, stFindPathFromHomeExport)
{
    TuneResult_t ret;
    std::string fusionSocVersion = "Ascend910A";
    std::string fusionPath;
    char* getDefPath = getenv("HOME");
    MOCKER(GetSocVersion)
    .stubs()
    .with(outBound(fusionSocVersion))
    .will(returnValue(TUNE_SUCCESS));
    std::string realPath = getDefPath;
    realPath = realPath + "/Ascend/latest/data/aoe/custom/graph/Ascend910A/Ascend910A_gradient_fusion.json";

    uint32_t beginCmpPath = 0;
    uint32_t endCmpPath = 0;

    std::string fullPath = "";
    if ('/' != realPath[0]) {
        fullPath = getcwd(nullptr, 0);
        beginCmpPath = fullPath.size();
        fullPath = fullPath + "/" + realPath;
    } else {
        fullPath = realPath;
        beginCmpPath = 1;
    }

    if (fullPath[fullPath.size() - 1] != '/') {
        fullPath += "/";
    }
    endCmpPath = fullPath.size();
    for (uint32_t i = beginCmpPath; i < endCmpPath; i++) {
        if ('/' == fullPath[i]) {
            std::string curPath = fullPath.substr(0, i);
            if (access(curPath.c_str(), F_OK) != 0) {
                mkdir(curPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
            }
        }
    }
    ret = GetTuneFusionPath(fusionPath);
    //路径字符拼接成功，realpath校验失败
    EXPECT_EQ(ret, TUNE_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(GradientSplitTuneTest, st_ErrorJsonFile)
{
    TuneResult_t ret;
    vector<vector<struct ProfilingMetric>> taskProfilingList(4, vector<struct ProfilingMetric>(2,
        {"tune", "trace_node_name", "TensorRedirect", 0, 1}));
    std::string workPath = "/tmp/";

    MOCKER(GetGradientNode)
    .expects(atMost(20))
    .will(returnValue(TUNE_SUCCESS));

    char file_name_t[] = "./error.json";

    std::ofstream outfile(file_name_t, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.is_open())
    {
        outfile << std::setw(1) << "}{}" << std::endl;
        HCCL_INFO("open %s success", file_name_t);
    }
    else
    {
        HCCL_ERROR("open %s failed", file_name_t);
    }

    outfile.close();
    std::string path = file_name_t;

    MOCKER(GetTuneFusionPath)
    .stubs()
    .with(outBound(path))
    .will(returnValue(TUNE_SUCCESS));

    ret = HcomGradientFusionTune(taskProfilingList, workPath);
    EXPECT_EQ(ret, TUNE_E_OPEN_FILE_FAILURE);

    GlobalMockObject::verify();
}


HcclResult stub_HcomGetAlgorithm_invalidAlgo(u32 level, std::string &algo)
{
    algo = "strange";
    return HCCL_SUCCESS;
}

extern void GetTopoInfoFromFile(vector<TopoInfo>& toposInfo);
TEST_F(GradientSplitTuneTest, stGetTopoInfoFromFile)
{
    vector<TopoInfo> toposInfo;
    u32 i = 1;
    MOCKER(HcomGetServerNumAndDeviceNumPerServer)
    .stubs()
    .with(outBound(i), outBound(i), outBound(i))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HcomGetAlgorithm)
    .expects(atMost(10))
    .will(invoke(stub_HcomGetAlgorithm_invalidAlgo));

    GetTopoInfoFromFile(toposInfo);
    GlobalMockObject::verify();
}