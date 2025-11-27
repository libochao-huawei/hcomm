/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include <stdio.h>
#include <stdlib.h>
#include "rt_external.h"
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
#include "externalinput.h"

#include <nlohmann/json.hpp>


#define private public
#define protected public
#include "topoinfo_detect.h"
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "hccl_nslbdp_pub.h"
#include "hccl_nslbdp.h"
#include "coll_batch_send_recv_executor.h"
#include "coll_reduce_scatter_v_executor.h"
#include "coll_all_gather_v_executor.h"
#include "hccl_network.h"
#include "preempt_port_manager.h"
#undef protected
#undef private

#include "common/src/topo/hccl_whitelist.h"
#include "profiling_manager.h"
#include <hccl/hccl.h>
#include "llt_hccl_stub_pub.h"
#include <iostream>
#include <fstream>
#include "hccl/base.h"
#include "hccl/hccl_ex.h"
#include <hccl/hccl_types.h>
#include "topoinfo_ranktableParser_pub.h"
#include "v80_rank_table.h"
#include "tsd/tsd_client.h"
#include "dltdt_function.h"
#include "externalinput_pub.h"
#include "op_base.h"
#include "adapter_rts.h"
#include "param_check_pub.h"
#include "hcom_pub.h"
#include "comm_config_pub.h"
#include "exception_handler.h"
#include "plugin_runner_pub.h"
#include "task_exception_handler_pub.h"

using namespace hccl;
using namespace std;
class STnslbdpTest : public testing::TestWithParam<bool>
{
protected:
    // static void SetUpTestCase()
    // {
    //     std::cout << "OpbaseTest SetUP" << std::endl;
    // }
    // static void TearDownTestCase()
    // {
    //     std::cout << "OpbaseTest TearDown" << std::endl;
    // }
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        static s32  call_cnt = 0;
        std::string name = std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        DlTdtFunction::GetInstance().DlTdtFunctionInit();
        TsdOpen(1,2);
        ra_set_shm_name(name.c_str());
        ResetInitState();

        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        TsdClose(1);
        std::cout << "A Test TearDown" << std::endl;
        GlobalMockObject::verify();
    }
};

void getRankTableForNSLB(RankTable_t &localRankTable)
{
    localRankTable.collectiveId = "192.168.0.101-8000-8001";
    std::vector<RankInfo_t> rankVec(2);
    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1); // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";
    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2); // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.102";
    localRankTable.rankList.assign(rankVec.begin(), rankVec.end());
    localRankTable.deviceNum = 2;
    localRankTable.serverNum = 2;
    aclrtSetDevice(0);
}

void getBasicRankTableForNSLB(HcclBasicRankInfo &localRankInfo)
{
    localRankInfo.hostIP.SetReadableAddress("101.0.168.192");
    localRankInfo.devicePhysicID = 0;
    HcclIpAddress ipAddr1(1694542016);
    HcclIpAddress ipAddr2(1711319232);
    localRankInfo.deviceIP.push_back(ipAddr1);
    localRankInfo.deviceIP.push_back(ipAddr2);
    aclrtSetDevice(0);
}

TEST_F(STnslbdpTest, hccl_prepare_ut_env)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclCommunicator comm;
    MOCKER_CPP_VIRTUAL(comm, &HcclCommunicator::InitHccp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    uint32_t masterIp = 0xC0A80001;
    uint32_t masterPort = 8080;
    uint32_t totalRankSize = 8;
    uint32_t nodeID = 1;
    uint32_t localRankSize = 1;
    HcclBasicRankInfo localRankInfo;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    u32 rootRank = 0;
    HcclRootHandle rootInfo;
    u32 srcLocalRankId = 0;
    u8 algType = 0;
    std::string identifier = "192.168.0.101-8000-8001";
    u32 nRanks = 8;
    u32 rank = 1;
    u32 rankSize = 8;
    u64 count = 100;
    std::string algName = "ReduceScatterVMeshOpbaseExecutor";
    hcclNslbDp::GetInstance().SetGlobalCommNodeId(1);
    hcclNslbDp::GetInstance().SetGlobalCommLocalRankNum(localRankSize);
    hcclNslbDp::GetInstance().SetGlobalCommTaskId(25771649526027);
    hcclNslbDp::GetInstance().SetGlobalCommRankTotalNum(totalRankSize);
    RankTable_t localRankTable;
    getRankTableForNSLB(localRankTable);

    hcclNslbDp::GetInstance().SetGlobalRank_RankTableExit(localRankTable);
    hcclNslbDp::GetInstance().SendGlobalRankTable(rank);

    u32 nodeid = hcclNslbDp::GetInstance().GetGlobalCommNodeId();
    HCCL_INFO("GetGlobalCommNodeId = %u", nodeid);
    u32 ranknum = hcclNslbDp::GetInstance().GetGlobalCommLocalRankNum();
    HCCL_INFO("GetGlobalCommLocalRankNum = %u", ranknum); 
    u32 totalnum = hcclNslbDp::GetInstance().GetGlobalCommRankTotalNum();
    HCCL_INFO("GetGlobalCommRankTotalNum = %u", totalnum);

    u32 buffer_size = 4096;
    void *tlv_handle = nullptr;

    hcclNslbDp::GetInstance().setHccpInfo(buffer_size, tlv_handle);

    hcclNslbDp::GetInstance().SetCommInfo_RankTableExit(localRankTable);
    hcclNslbDp::GetInstance().GenerateOpAndAdjTable(opType, rootRank, srcLocalRankId, algType, identifier, count, rankSize);
    hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    hcclNslbDp::GetInstance().SendTableFir(rank);
    return;
}

void getRankListForNSLB(std::vector<RankInfo> &rankLists)
{
    vector<RankInfo> rankVec(2);
    rankVec[0].userRank = 0;
    rankVec[0].superPodIdx = 0;
    rankVec[1].userRank = 1;
    rankVec[1].superPodIdx = 1;
    rankLists.assign(rankVec.begin(), rankVec.end());
}

TEST_F(STnslbdpTest, st_hccl_noranktable)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    std::vector<RankInfo> rankInfoList;
    uint32_t masterIp = 0xC0A80001;
    uint32_t masterPort = 8080;
    uint32_t totalRankSize = 8;
    uint32_t nodeID = 1;
    uint32_t localRankSize = 1;
    HcclBasicRankInfo localRankInfo;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    u32 rootRank = 0;
    HcclRootHandle rootInfo;
    u32 srcLocalRankId = 0;
    u8 algType = 0;
    std::string identifier = "192.168.0.101-8000-8001";
    u32 nRanks = 8;
    u32 rank = 1;
    u32 rankSize = 8;
    u64 count = 1000000;
    std::string algName = "ReduceScatterVMeshOpbaseExecutor";
    hcclNslbDp::GetInstance().SetGlobalCommNodeId(1);
    hcclNslbDp::GetInstance().SetGlobalCommLocalRankNum(localRankSize);
    hcclNslbDp::GetInstance().SetGlobalCommTaskId(25771649526027);
    hcclNslbDp::GetInstance().SetGlobalCommRankTotalNum(totalRankSize);
    hcclNslbDp::GetInstance().HcclSetGlobalRankTotalNum(nRanks);
    RankTable_t localRankTable;
    getRankTableForNSLB(localRankTable);
    getBasicRankTableForNSLB(localRankInfo);
    getRankListForNSLB(rankInfoList);
    hcclNslbDp::GetInstance().SetGlobalCommRankTable_RootInfo(localRankTable, localRankInfo, rankInfoList, identifier, nRanks, rank);
    hcclNslbDp::GetInstance().SetCommInfo_NoRankTable(localRankTable, identifier);
    hcclNslbDp::GetInstance().GenerateOpAndAdjTable(opType, rootRank, srcLocalRankId, algType, identifier, count, rankSize);
    hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    hcclNslbDp::GetInstance().CheckAhcSupport(12, "192.168.0.101-8000-8001");

    AdjInfo nslbAdjInfo;
    for(int step = 1; step <= nRanks; step++) {
        NslbDpAdjInfo nextAdjInfo = {0};
        nextAdjInfo.dstLocalRankId = (step + 1) % nRanks;
        nextAdjInfo.phaseId = step;
        nextAdjInfo.rev = 0;
        nslbAdjInfo.nsAdjInfo.push_back(nextAdjInfo);
    }
    nslbAdjInfo.dstRankNum = nRanks;

    hcclNslbDp::GetInstance().GetAlgAdjacencyTable(opType, srcLocalRankId, rootRank, algType, identifier, nslbAdjInfo);

    hcclNslbDp::GetInstance().SetGlobalDisRankTable(localRankInfo);

    hcclNslbDp::GetInstance().SetNslbDpRootRank(opType, rootRank, identifier, algType);
    hcclNslbDp::GetInstance().SendRootRankTable();

    hcclNslbDp::GetInstance().SendTableFir(rank);
    hcclNslbDp::GetInstance().SendGlobalDisRankTable();
    hcclNslbDp::GetInstance().SendOpAndAdjTable();
    hcclNslbDp::GetInstance().SendAlgorithmInfoTable();
    return;
}

TEST_F(STnslbdpTest, st_sendtable1)
{
    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    NslbDpCommConfigVal globalCommInfo{};

    std::string identifier = "192.168.0.101-8000-8001";
    // 获取通信域唯一标识
    s32 ret = strncpy_s(globalCommInfo.commDesc, COMM_DESC_MAX_LENGTH,  identifier.c_str(), identifier.size());
    if (ret != EOK) {
        HCCL_INFO("strncpy_s globalCommInfo.commDesc fail");
    }

    std::string times = "8001";
    globalCommInfo.commInitTime = std::stoull(times);
    globalCommInfo.commInitTime = globalCommInfo.commInitTime / NSLBDP_INITIME_MILLISENDS;

    u64 taskId = 0xC0A800011;
    u16 nRanks = 8;

    globalCommInfo.taskId = taskId;
    globalCommInfo.rankTotalNum = nRanks;
    for(u32 rankIndex = 0; rankIndex < 8; rankIndex++) {
        NslbDpRankInfo dpRankInfo;
        dpRankInfo.deviceIp = 0xC0A80001 + rankIndex;
        dpRankInfo.serverIp = 0xC0A80101;
        dpRankInfo.podId = 0;
        dpRankInfo.rev = 0;
        globalCommInfo.rankInfo.push_back(dpRankInfo);
    }
    u32 rank = 0;
    hcclNslbDp::GetInstance().SendCommRankTable(rank, globalCommInfo);

}


#if 0
TEST_F(STnslbdpTest, st_init_hccp)
{
    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    int ret = HCCL_SUCCESS;
    uint32_t ndev = 1;
    int32_t devices[ndev] = {0};
    HcclComm comms[ndev];
    for (int i = 0; i < ndev; i++) {
        ret = hrtSetDevice(devices[i]);
        EXPECT_EQ(ret, 0);
    }
    ret = HcclCommInitAll(ndev, devices, comms);
    EXPECT_EQ(ret, 0);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(comms[0]);
    std::string groupName = hcclComm->GetIdentifier();
    hcclNslbDp::GetInstance().clearnHccpInitFlag();
    hcclComm->InitHccp();
    hcclComm->GetRankLists();
    u32 bufferSize = hcclNslbDp::GetInstance().getBufferSize();
    HCCL_INFO("bufferSize = %u", bufferSize);

    u64 memSize = 0;
    ret = HcomGetWorkspaceMemSize("HcomReduceScatter", 1, HCCL_DATA_TYPE_INT8, groupName.c_str(), memSize);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < ndev; i++) {
        ret = hrtResetDevice(devices[i]);
        EXPECT_EQ(ret, 0);
        ret = HcclCommDestroy(comms[i]);
        EXPECT_EQ(ret, 0);
    }
    GlobalMockObject::verify();
}
#endif

TEST_F(STnslbdpTest, st_checklevel1_910b)
{
    u8 typel1 = 0;
    AlgTypeLevel1 algValue = AlgTypeLevel1::ALG_LEVEL1_RING;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_RING);

    algValue = AlgTypeLevel1::ALG_LEVEL1_PIPELINE;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_PIPELINE);

    algValue = AlgTypeLevel1::ALG_LEVEL1_HD;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_HDR);

    algValue = AlgTypeLevel1::ALG_LEVEL1_NHR;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_NHR);

    algValue = AlgTypeLevel1::ALG_LEVEL1_NHR_V1;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_NHR_V1);

    algValue = AlgTypeLevel1::ALG_LEVEL1_NB;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_NB);

    algValue = AlgTypeLevel1::ALG_LEVEL1_WHOLE_RING;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, hccl::NSLB_ALGO_TYPE_NA);

    algValue = AlgTypeLevel1::ALG_LEVEL1_AHC;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, NSLB_ALGO_TYPE_AHC);

    algValue = AlgTypeLevel1::ALG_LEVEL1_AHC_BROKE;
    typel1 = hcclNslbDp::GetInstance().GetNslbLevel1AlgType(algValue);
    EXPECT_EQ(typel1, hccl::NSLB_ALGO_TYPE_AHC);
}

TEST_F(STnslbdpTest, st_checklevel2_910c)
{
    u8 typel2 = 0;
    AlgTypeLevel2 algValue = AlgTypeLevel2::ALG_LEVEL2_WHOLE_RING;
    typel2 = hcclNslbDp::GetInstance().GetNslbLevel2AlgType(algValue);
    EXPECT_EQ(typel2, NSLB_ALGO_TYPE_NA);

    algValue = AlgTypeLevel2::ALG_LEVEL2_HD;
    typel2 = hcclNslbDp::GetInstance().GetNslbLevel2AlgType(algValue);
    EXPECT_EQ(typel2, NSLB_ALGO_TYPE_HDR);

    algValue = AlgTypeLevel2::ALG_LEVEL2_RING;
    typel2 = hcclNslbDp::GetInstance().GetNslbLevel2AlgType(algValue);
    EXPECT_EQ(typel2, NSLB_ALGO_TYPE_RING);

    algValue = AlgTypeLevel2::ALG_LEVEL2_NHR;
    typel2 = hcclNslbDp::GetInstance().GetNslbLevel2AlgType(algValue);
    EXPECT_EQ(typel2, NSLB_ALGO_TYPE_NHR);

    algValue = AlgTypeLevel2::ALG_LEVEL2_NB;
    typel2 = hcclNslbDp::GetInstance().GetNslbLevel2AlgType(algValue);
    EXPECT_EQ(typel2, NSLB_ALGO_TYPE_NB);
}

TEST_F(STnslbdpTest, st_checkNslbOpType)
{
    u8 type1 = 0;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_BROADCAST;
    bool support = hcclNslbDp::GetInstance().CheckSupportOptype(opType);
    EXPECT_EQ(support, true);
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_BROADCAST);

    opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    support = hcclNslbDp::GetInstance().CheckSupportOptype(opType);
    EXPECT_EQ(support, true);
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_ALLREDUCE);

    opType = HcclCMDType::HCCL_CMD_REDUCE;
    support = hcclNslbDp::GetInstance().CheckSupportOptype(opType);
    EXPECT_EQ(support, true);
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_REDUCE);

    opType = HcclCMDType::HCCL_CMD_SEND;
    support = hcclNslbDp::GetInstance().CheckSupportOptype(opType);
    EXPECT_EQ(support, true);
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_SEND);

    opType = HcclCMDType::HCCL_CMD_RECEIVE;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_RECEIVE);

    opType = HcclCMDType::HCCL_CMD_ALLGATHER;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_ALLGATHER);

    opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_REDUCE_SCATTER);

    opType = HcclCMDType::HCCL_CMD_ALLTOALLV;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_ALLTOALLV);

    opType = HcclCMDType::HCCL_CMD_ALLTOALLVC;
    support = hcclNslbDp::GetInstance().CheckSupportOptype(opType);
    EXPECT_EQ(support, true);
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_ALLTOALLVC);

    opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_ALLTOALL);

    opType = HcclCMDType::HCCL_CMD_GATHER;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_GATHER);

    opType = HcclCMDType::HCCL_CMD_SCATTER;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_SCATTER);

    opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, NSLBDP_CMD_BATCH_SEND_RECV);

    opType = HcclCMDType::HCCL_CMD_INVALID;
    type1 = hcclNslbDp::GetInstance().GetNslbOpType(opType);
    EXPECT_EQ(type1, 0);
}

TEST_F(STnslbdpTest, st_checkfirst4bit)
{
    u8 opType = NSLBDP_CMD_ALLREDUCE;
    u8 algType = 0;
    u64 firstFourBit = hcclNslbDp::GetInstance().GetNslbDpFirstFourBit(opType, algType);
    EXPECT_EQ(firstFourBit, 5);

    opType = NSLBDP_CMD_ALLGATHER;
    firstFourBit = hcclNslbDp::GetInstance().GetNslbDpFirstFourBit(opType, algType);
    EXPECT_EQ(firstFourBit, 2);

    opType = NSLBDP_CMD_REDUCE_SCATTER;
    firstFourBit = hcclNslbDp::GetInstance().GetNslbDpFirstFourBit(opType, algType);
    EXPECT_EQ(firstFourBit, 0);
}

TEST_F(STnslbdpTest, st_checkalgo_match)
{
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLGATHER;
    std::string algName = "AllGatherVMeshOpbaseExecutor";
    bool match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    EXPECT_EQ(match, true);

    opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    algName = "ReduceScatterVMeshOpbaseExecutor";
    match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    EXPECT_EQ(match, false);

    opType = HcclCMDType::HCCL_CMD_BROADCAST;
    algName = "AllGatherVMeshOpbaseExecutor";
    match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    EXPECT_EQ(match, false);

    opType = HcclCMDType::HCCL_CMD_BROADCAST;
    algName = "ReduceScatterVMeshOpbaseExecutor";
    match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    EXPECT_EQ(match, false);
    algName = "BroadCastVMeshOpbaseExecutor";
    match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    EXPECT_EQ(match, true);

    opType = HcclCMDType::HCCL_CMD_SCATTER;
    algName = "ReduceScatterVMeshOpbaseExecutor";
    match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    HCCL_INFO("match 1 = %u", match);

    algName = "AllGatherVMeshOpbaseExecutor";
    match = hcclNslbDp::GetInstance().CheckAlgoConsistency(opType, algName);
    HCCL_INFO("match 1 = %u", match);
}


TEST_F(STnslbdpTest, st_getnslb_l4portid)
{
    u32 rankSize = 8;
    u8 algType = 1;
    u16 l4SPortId = 0;

    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

    rankSize = NSLBDP_COMMINTERVAL_FLAGSIX + 1;
    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

    rankSize = NSLBDP_COMMINTERVAL_FLAGFIV + 1;
    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

    rankSize = NSLBDP_COMMINTERVAL_FLAGFOU + 1;
    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

    rankSize = NSLBDP_COMMINTERVAL_FLAGTHR + 1;
    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

    rankSize = NSLBDP_COMMINTERVAL_FLAGSEC + 1;
    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

    rankSize = NSLBDP_COMMINTERVAL_FLAG + 1;
    hcclNslbDp::GetInstance().GetNslbDpl4SPortId(rankSize, algType, &l4SPortId);
    HCCL_INFO("l4SPortId = %u", l4SPortId);

}

TEST_F(STnslbdpTest, st_collectsend_table4)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    NslbDpGlobalRankInfo tab_f = {};

    tab_f.taskId = 0xC0A800011;
    std::string identifier = "192.168.0.101-8000-8001";

    s32 sRet = memcpy_s(tab_f.commDesc, sizeof(tab_f.commDesc), identifier.c_str(), identifier.size());
    if (sRet != EOK) {
        HCCL_INFO("memcpy_s commDesc fail");
    }

    u64 utime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tab_f.commInitTime = utime;

    tab_f.packetId = 1;
    tab_f.rev = 0;
    tab_f.packetNum = 1;
    tab_f.rev2 = 0;
    tab_f.rev3 = 0;

    for (uint32_t rankIndex = 0; rankIndex < 2; rankIndex++) {
        TableFourRankInfo rankInfoTemp;
        rankInfoTemp.deviceIp = 0xC0A80001 + rankIndex;
        rankInfoTemp.serverIp = 0xC0A80101;
        tab_f.rankInfo.push_back(rankInfoTemp);
    }
    tab_f.rankTotalNum = 2;
    tab_f.rankNum = 2;

    u32 buffer_size = 4096;
    void *tlv_handle = nullptr;
    hcclNslbDp::GetInstance().setHccpInfo(buffer_size, tlv_handle);

    hcclNslbDp::GetInstance().SendRankTableGlobalRank(tab_f);
}

#if 0
TEST_F(STnslbdpTest, st_collect_table2_910b)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclCommunicator impl2;
    MOCKER_CPP_VIRTUAL(impl2, &HcclCommunicator::InitHccp)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    OpParam opParam;
    opParam.outputSize = 100000;
    opParam.All2AllDataDes.sendType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.recvType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.sendCount = 16;

    std::string algName = "ReduceScatterVMeshOpbaseExecutor";  

    std::string identifier_ = "192.168.0.101-8000-8001";
    DevType deviceType_ = DevType::DEV_TYPE_910B;

    AlgType nslbAlgType;
    nslbAlgType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    nslbAlgType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;;

    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
    HcclCommunicator* impl = hcclComm->communicator_.get();

    impl->NslbDp_CollectOperTable(opType, opParam, nslbAlgType, algName);
}


TEST_F(STnslbdpTest, st_collect_table3_910b)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    OpParam opParam;
    opParam.outputSize = 100000;
    opParam.All2AllDataDes.sendType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.recvType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.sendCount = 16;

    std::string algName = "ReduceScatterVMeshOpbaseExecutor";  

    std::string identifier_ = "192.168.0.101-8000-8001";
    DevType deviceType_ = DevType::DEV_TYPE_910B;

    AlgType nslbAlgType;
    nslbAlgType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    nslbAlgType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;

    AdjInfo nslbAdjInfo;
    for(int step = 1; step <= 8; step++) {
        NslbDpAdjInfo nextAdjInfo = {0};
        nextAdjInfo.dstLocalRankId = (step + 1) % 8;
        nextAdjInfo.phaseId = step;
        nextAdjInfo.rev = 0;
        nslbAdjInfo.nsAdjInfo.push_back(nextAdjInfo);
    }
    nslbAdjInfo.dstRankNum = 8;

    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
    HcclCommunicator* impl = hcclComm->communicator_.get();

    impl->NslbDp_CollectSendAdjTable(opType, opParam, nslbAlgType, nslbAdjInfo);
}
#endif

TEST_F(STnslbdpTest, st_Initconfig_nslb)
{
    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    HcclCommunicator impl2;
    MOCKER_CPP_VIRTUAL(impl2, &HcclCommunicator::InitHccp)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    CommConfig commConfig("comm_ID");
    CommConfigInfo configInfo = { sizeof(CommConfigHandle), COMM_CONFIG_MAGIC_WORD, 1, { 0 } };
    CommConfigHandle configHandle = { configInfo, 200, 0 };

    HcclCommConfig config = {0};
    typedef struct {
        size_t size;
        u32 magicWord;
        u32 version;
        u64 reserved;
    } configInfo_t;

    HcclCommConfig *p = &config;
    configInfo_t *info = (configInfo_t *)p;
    info->size = sizeof(HcclCommConfig);
    info->magicWord = HCCL_COMM_CONFIG_MAGIC_WORD;
    info->version = HCCL_COMM_CONFIG_VERSION;
    info->reserved = 0;

    config.hcclBufferSize = HCCL_COMM_BUFFSIZE_CONFIG_NOT_SET;
    config.hcclDeterministic = HCCL_COMM_DETERMINISTIC_CONFIG_NOT_SET;
    config.hcclCommName[0] = '\0';
    config.hcclUdi[0] = '\0';
    config.hcclOpExpansionMode = HCCL_COMM_DEFAULT_OP_EXPANSION_MODE;
    config.hcclRdmaTrafficClass = HCCL_COMM_TRAFFIC_CLASS_CONFIG_NOT_SET;
    config.hcclRdmaServiceLevel = HCCL_COMM_SERVICE_LEVEL_CONFIG_NOT_SET;
    config.hcclWorldRankID = 0;
    config.hcclJobID = 1234567890;

    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    // HcclCommInitClusterInfoConfig(const char *clusterInfo, u32 rank, HcclCommConfig *config, &newcomm)
    HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);

}

#if 0 //执行失败
TEST_F(STnslbdpTest, st_collect_ahc_table2_910C)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    OpParam opParam;
    opParam.outputSize = 100000;
    opParam.All2AllDataDes.sendType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.recvType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.sendCount = 16;

    string algName = "ReduceScatterRingFor91093Executor";  

    string identifier_ = "192.168.0.101_8000_8001";

    AlgType nslbAlgType;
    nslbAlgType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    nslbAlgType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    nslbAlgType.algoLevel2 = AlgTypeLevel2::ALG_LEVEL2_RING;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;

    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
    HcclCommunicator* impl = hcclComm->communicator_.get();
    impl->NslbGetDeviceType();
    impl->NslbGetServerNum();
    MOCKER_CPP(&HcclCommunicator::NslbGetDeviceType)
    .stubs()
    .will(returnValue(DevType::DEV_TYPE_910_93));

    MOCKER_CPP(&HcclCommunicator::NslbGetServerNum)
    .stubs()
    .will(returnValue(8));
    impl->NslbDp_CollectOperTable(opType, opParam, nslbAlgType, algName);
}


TEST_F(STnslbdpTest, st_collect_ahc_table3_910C)
{
    MOCKER(H2DTlvRequest)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(H2DTlvInit)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    OpParam opParam;
    opParam.outputSize = 100000;
    opParam.All2AllDataDes.sendType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.recvType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.sendCount = 16;

    string algName = "ReduceScatterRingFor91093Executor";  

    string identifier_ = "192.168.0.101-8000-8001";
    AlgType nslbAlgType;
    nslbAlgType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_8P_RING;
    nslbAlgType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_RING;
    nslbAlgType.algoLevel2 = AlgTypeLevel2::ALG_LEVEL2_RING;

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;

    AdjInfo nslbAdjInfo;
    for(int step = 1; step <= 8; step++) {
        NslbDpAdjInfo nextAdjInfo = {0};
        nextAdjInfo.dstLocalRankId = (step + 1) % 8;
        nextAdjInfo.phaseId = step;
        nextAdjInfo.rev = 0;
        nslbAdjInfo.nsAdjInfo.push_back(nextAdjInfo);
    }
    nslbAdjInfo.dstRankNum = 8;

    HcclRootInfo id;

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclComm newcomm;
    ret = HcclCommInitRootInfo(1, &id, 0, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::hcclComm* hcclComm = static_cast<hccl::hcclComm *>(newcomm);
    HcclCommunicator* impl = hcclComm->communicator_.get();
    MOCKER_CPP(&HcclCommunicator::NslbGetDeviceType)
    .stubs()
    .will(returnValue(DevType::DEV_TYPE_910_93));

    MOCKER_CPP(&HcclCommunicator::NslbGetServerNum)
    .stubs()
    .will(returnValue(8));
    impl->NslbDp_CollectSendAdjTable(opType, opParam, nslbAlgType, nslbAdjInfo);
}

TEST_F(STnslbdpTest, St_HcclNslb_When_SendTable1_Expect_ReturnIsSUCCESS)
{
    NslbDpCommConfigVal globalTable1{};

    std::string identifier = "192.168.0.101-8000-8001";
    // 获取通信域唯一标识
    s32 ret = strncpy_s(globalTable1.commDesc, COMM_DESC_MAX_LENGTH,  identifier.c_str(), identifier.size());
    if (ret != EOK) {
        HCCL_INFO("strncpy_s globalTable1.commDesc fail");
    }

    std::string times = "8001";
    globalTable1.commInitTime = std::stoull(times);
    globalTable1.commInitTime = globalTable1.commInitTime / NSLBDP_INITIME_MILLISENDS;

    globalTable1.taskId = 0xC0A800011;
    globalTable1.rankTotalNum = 8;
    for(u32 rankIndex = 0; rankIndex < 1040; rankIndex++) {
        NslbDpRankInfo dpRankInfo;
        dpRankInfo.deviceIp = 0xC0A80001 + rankIndex;
        dpRankInfo.serverIp = 0xC0A80101;
        dpRankInfo.podId = 0;
        dpRankInfo.rev = 0;
        globalTable1.rankInfo.push_back(dpRankInfo);
    }
    HcclResult result = hcclNslbDp::GetInstance().SendTableProc(0, globalTable1);
    EXPECT_EQ(result, HCCL_SUCCESS);
    result = hcclNslbDp::GetInstance().SendTableProc(1, globalTable1);
    EXPECT_EQ(result, HCCL_SUCCESS);
    result = hcclNslbDp::GetInstance().SendTableProc(2, globalTable1);
    EXPECT_EQ(result, HCCL_SUCCESS);
}
#endif