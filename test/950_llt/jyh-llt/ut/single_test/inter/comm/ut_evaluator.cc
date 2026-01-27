#include <iostream>
#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "comm_time_evaluator.h"
#include "log.h"

using namespace HcclEvaluator;

class EvaluatorTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--ExchangerBaseTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--ExchangerBaseTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

// 根据传入的判断函数fptr生成conn，根据conn生成port
void GeneResource(std::vector<HcclNode> &nodeList,
    std::vector<std::vector<HcclConnection>> &connList,
    std::vector<std::vector<HcclPort>> &portList,
    std::function<bool(int, int, int)> fptr)
{
    int bandWidth = 1024;
    int nodeNum = nodeList.size();
    for (int nodeId = 0; nodeId < nodeNum; nodeId++) {
        int index = 0;
        for (int peerNodeId = 0; peerNodeId < nodeNum; peerNodeId++) {
            if (fptr(nodeId, peerNodeId, nodeNum)) {
                continue;
            }
            connList[nodeId][index].peerNodeId = peerNodeId;
            connList[nodeId][index].port = &portList[nodeId][index];
            connList[nodeId][index].next = &connList[nodeId][index + 1];
            index++;
        }
        connList[nodeId][--index].next = nullptr;

        for (;index >= 0; index--) {
            portList[nodeId][index].protocol = HCCL_PROTOCOL_SHM_DMA;
            portList[nodeId][index].portId = index;
            portList[nodeId][index].bandWidth = bandWidth;
            portList[nodeId][index].staticDelay = 0;
            portList[nodeId][index].next = nullptr;
        }

        nodeList[nodeId].nodeId = nodeId;
        nodeList[nodeId].connectionList = &connList[nodeId][0];
        nodeList[nodeId].next = &nodeList[nodeId + 1];
    }
    nodeList[nodeNum - 1].next = nullptr;
}

bool JuegeMesh(int nodeId, int peerNodeId, int nodeNum)
{
    return nodeId == peerNodeId;
}

bool JuegeRing(int nodeId, int peerNodeId, int nodeNum)
{
    return peerNodeId != (nodeId + 1) % nodeNum;
}

bool JuegeMultiRing(int nodeId, int peerNodeId, int nodeNum)
{
    return ((nodeId == peerNodeId) ||
            ((nodeId % 4 != peerNodeId % 4) && (nodeId / 4 != peerNodeId / 4)) ||
            (nodeId / 8 != peerNodeId / 8));
}

TEST_F(EvaluatorTest, TestHockneyModel)
{
    HockneyModelParam param;
    param.alpha = 0.001;
    param.beta = 0.001;
    param.gamma = 0.001;
    HockneyModel costModel(param);
    u32 nodeNum = 8;
    u32 byteNum = 1000;
    SubgraphOpType op = SubgraphOpType::ALLGATHER;
    AlgType alg = AlgType::INVALID_VAL;
    float cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    alg = AlgType::BINARY_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 7.003) < 1e-6, true);

    op = SubgraphOpType::GATHER;
    alg = AlgType::PAIR_WISE;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::SCATTER;
    alg = AlgType::PAIR_WISE;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::INVALID_VAL;
    alg = AlgType::PAIR_WISE;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::GATHER;
    alg = AlgType::RING;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 7.007) < 1e-6, true);

    op = SubgraphOpType::SCATTER;
    alg = AlgType::RING;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 7.007) < 1e-6, true);

    op = SubgraphOpType::ALLTOALL;
    alg = AlgType::RING;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::INVALID_VAL;
    alg = AlgType::RING;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::GATHER;
    alg = AlgType::MESH;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 1.007) < 1e-6, true);

    op = SubgraphOpType::SCATTER;
    alg = AlgType::MESH;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 1.007) < 1e-6, true);

    op = SubgraphOpType::ALLTOALL;
    alg = AlgType::MESH;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::INVALID_VAL;
    alg = AlgType::MESH;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::GATHER;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 7.003) < 1e-6, true);

    op = SubgraphOpType::SCATTER;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 7.003) < 1e-6, true);

    op = SubgraphOpType::ALLTOALL;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::INVALID_VAL;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    nodeNum = 9;
    op = SubgraphOpType::GATHER;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 8.004) < 1e-6, true);

    op = SubgraphOpType::SCATTER;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 8.004) < 1e-6, true);

    op = SubgraphOpType::ALLTOALL;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);

    op = SubgraphOpType::REDUCE;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost - 4.632) < 1e-6, true);

    op = SubgraphOpType::INVALID_VAL;
    alg = AlgType::RECURSIVE_HD;
    cost = costModel.CalcCost(nodeNum, byteNum, op, alg);
    EXPECT_EQ(fabs(cost + 1) < 1e-6, true);
}

TEST_F(EvaluatorTest, EveluateMeshTopo)
{
    std::vector<int> nodeNumList{4, 8, 15, 16};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(nodeNum - 1));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(nodeNum - 1));
        GeneResource(nodeList, connList, portList, &JuegeMesh);

        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 * 1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;

        HcclResult ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]", cost);

        op.op = HCCL_OP_All_TO_ALL;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_All_GATHER;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_BROADCAST;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE_SCATTER;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);
    }
}

TEST_F(EvaluatorTest, EveluateStarTopo)
{
    std::vector<int> nodeNumList{4, 7, 8, 15, 16};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(nodeNum - 1));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(nodeNum - 1));
        GeneResource(nodeList, connList, portList, &JuegeMesh);
        // 先生成mesh，再破坏port独占条件，退化成star
        for (int nodeId = 0; nodeId < nodeNum; nodeId++) {
            for (int portIndex = 0; portIndex < portList[nodeId].size(); portIndex++) {
                portList[nodeId][portIndex].portId = 0;
            }
        }

        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        CommTimeEvaluator evaluator;
        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 * 1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;
        HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]", cost);

        op.op = HCCL_OP_All_TO_ALL;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_All_GATHER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_BROADCAST;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE_SCATTER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);
    }
}

TEST_F(EvaluatorTest, EveluateRingTopo)
{
    std::vector<int> nodeNumList{4, 8, 15, 16};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(1));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(1));
        GeneResource(nodeList, connList, portList, &JuegeRing);

        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        CommTimeEvaluator evaluator;
        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 *1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;
        HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]", cost);

        op.op = HCCL_OP_All_GATHER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_BROADCAST;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE_SCATTER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);
    }
}

TEST_F(EvaluatorTest, EveluateMultiRingTopo)
{
    std::vector<int> nodeNumList{8};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(4));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(4));
        GeneResource(nodeList, connList, portList, &JuegeMultiRing);
        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        CommTimeEvaluator evaluator;
        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 * 1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;
        HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]", cost);

        op.op = HCCL_OP_All_GATHER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_BROADCAST;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE_SCATTER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);
    }
}

TEST_F(EvaluatorTest, EveluateMultiServerMultiRingTopo)
{
    std::vector<int> nodeNumList{16};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(4));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(4));
        GeneResource(nodeList, connList, portList, &JuegeMultiRing);
        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        CommTimeEvaluator evaluator;
        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 * 1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;
        HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]", cost);

        op.op = HCCL_OP_All_GATHER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_BROADCAST;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE_SCATTER;
        ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);

        op.op = HCCL_OP_REDUCE;
        ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]\n", cost);
    }
}

TEST_F(EvaluatorTest, EveluateMultiServerMultiRingTopoException)
{
    std::vector<int> nodeNumList{15};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(4));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(4));
        GeneResource(nodeList, connList, portList, &JuegeMultiRing);
        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        CommTimeEvaluator evaluator;
        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 * 1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;
        HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    }
}

TEST_F(EvaluatorTest, EveluateExceptionTopo)
{
    int nodeNum = 20;
    std::vector<HcclNode> nodeList(nodeNum);
    std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(1));
    std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(1));
    GeneResource(nodeList, connList, portList, &JuegeRing);

    HcclTopology topo;
    topo.nodeList = &nodeList[0];
    topo.nodeNum = nodeNum;

    CommTimeEvaluator evaluator;
    HcclOpInfo op;
    op.op = HCCL_OP_All_REDUCE;
    op.count = 1024 *1024;
    op.dataType = HCCL_DATA_TYPE_INT32;
    float cost;
    HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(EvaluatorTest, EveluateStarTopo512)
{
    std::vector<int> nodeNumList{128, 256};
    for (auto nodeNum : nodeNumList) {
        std::vector<HcclNode> nodeList(nodeNum);
        std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(nodeNum - 1));
        std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(nodeNum - 1));
        GeneResource(nodeList, connList, portList, &JuegeMesh);
        // 先生成mesh，再破坏port独占条件，退化成star
        for (int nodeId = 0; nodeId < nodeNum; nodeId++) {
            for (int portIndex = 0; portIndex < portList[nodeId].size(); portIndex++) {
                portList[nodeId][portIndex].portId = 0;
            }
        }

        HcclTopology topo;
        topo.nodeList = &nodeList[0];
        topo.tag = nodeNum;
        topo.nodeNum = nodeNum;

        CommTimeEvaluator evaluator;
        HcclOpInfo op;
        op.op = HCCL_OP_All_REDUCE;
        op.count = 1024 * 1024;
        op.dataType = HCCL_DATA_TYPE_INT32;
        float cost;
        HcclResult ret = evaluator.EvaluateTimeCostRoughly(&op, &topo, &cost);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        HCCL_INFO("eveluate time cost[%f]", cost);
    }
}

TEST_F(EvaluatorTest, EvaluatorInvalidParam)
{
    int nodeNum = 4;
    std::vector<HcclNode> nodeList(nodeNum);
    std::vector<std::vector<HcclConnection>> connList(nodeNum, std::vector<HcclConnection>(1));
    std::vector<std::vector<HcclPort>> portList(nodeNum, std::vector<HcclPort>(1));
    GeneResource(nodeList, connList, portList, &JuegeRing);

    HcclTopology topo;
    topo.nodeList = &nodeList[0];
    topo.nodeNum = nodeNum;

    CommTimeEvaluator evaluator;
    HcclOpInfo op;
    op.count = 1024 *1024;
    float cost;

    op.dataType = HCCL_DATA_TYPE_INT32;
    op.op = HCCL_OP_NUM;

    HcclResult ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
    EXPECT_EQ(ret, HCCL_E_PARA);

    op.op = HCCL_OP_REDUCE;
    op.dataType = HCCL_DATA_TYPE_RESERVED;
    ret = HcclEvaluateTimeCostRoughly(&op, &topo, &cost);
    EXPECT_EQ(ret, HCCL_E_PARA);
}