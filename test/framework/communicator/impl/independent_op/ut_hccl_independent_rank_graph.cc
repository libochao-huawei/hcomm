/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "hccl_api.h"
#include "hccl_api_base_test.h"
#include "hccl_tbe_task.h"
#include "llt_hccl_stub_GenRankTable.h"
#include "llt_hccl_stub_pub.h"
#include "hccl_communicator.h"
#include "hccl_types.h"
#include "hccl_comm_pub.h"
#include "dlra_function.h"
#include "hcom_common.h"
 
constexpr const char* RANKTABLE_FILE_NAME = "./ut_independent_op_test.json";
class HcclIndependentOpRankGraphTest : public BaseInit {
public:
    void SetUp() override {
        MOCKER(HcclTbeTaskInit)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        BaseInit::SetUp();
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        UT_COMM_CREATE_DEFAULT(comm);
    }
    void TearDown() override {
        Ut_Comm_Destroy(comm);
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclIndependentOpRankGraphTest, Ut_CommGetLinks_When_CommIsNull_Expect_PtrError)
{
    HcclResult ret = CommGetLinks(nullptr, 0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}
TEST_F(HcclIndependentOpRankGraphTest, Ut_A2CommGetRankGraph_When_RightPara_Expect_Success)
{
    //通信域创建
    HcclResult ret = HCCL_SUCCESS;
    HcomInfo hcom_info;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hrtSetDevice(0);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TopoMeta topoMate;
    RankTable_For_LLT::GenTopoMeta(topoMate, 1, 2, 4);//A2 双机
    RankTable_t rankTable_;
    RankTable_For_LLT::GenRankTable(topoMate, rankTable_);
    hcom_info.rankTable = rankTable_;
    u32 rankId = 5;//当前rank是5, server1
    s32 logicDevId;
    DevType deviceType;
    hrtGetDevice(&logicDevId);
    hrtGetDeviceType(deviceType);
    hcom_info.params.totalRanks = rankTable_.rankNum;
    hcom_info.params.rank = rankId;
    hcom_info.params.userRank = rankId;
    hcom_info.params.serverId = rankTable_.rankList[rankId].serverId;
    hcom_info.params.logicDevId = logicDevId;
    hcom_info.params.deviceType = deviceType;
    hcom_info.devId = -1;
 
    u32 rank = hcom_info.params.rank;
    RankInfo_t temp_rankList = hcom_info.rankTable.rankList[rank];
    sal_memset(hcom_info.params.id.internal, HCCL_ROOT_INFO_BYTES, 0, sizeof(hcom_info.params.id.internal));
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
 
    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm(200, 200, HCCL_WORLD_GROUP));
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = hcom_info.pComm->SetIndependentOpConfig(commConfig, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    uint32_t *netlayers = nullptr;
    uint32_t netLayersNum = 0;
    uint32_t rankNum = 0;
    uint32_t rankListNum = 0;
    uint32_t *rankList = nullptr;
    uint32_t *rankSizeList = nullptr;
    CommTopo topoType;
 
    ret = CommGetNetLayers(hcom_info.pComm.get(), &netlayers, &netLayersNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netLayersNum, 2);
    for (int i=0; i<netLayersNum; i++) {
        EXPECT_EQ(netlayers[i], i);
    }
 
    ret = CommGetInstTopoTypeByNetLayer(hcom_info.pComm.get(), 0, &topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, CommTopo::COMM_TOPO_1DMESH);
 
    ret = CommGetInstTopoTypeByNetLayer(hcom_info.pComm.get(), 1, &topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, CommTopo::COMM_TOPO_CLOS);
 
    ret = CommGetInstSizeByNetLayer(hcom_info.pComm.get(), 0, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 4);
 
    ret = CommGetInstSizeByNetLayer(hcom_info.pComm.get(), 1, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 8);
 
    ret = CommGetInstRanksByNetLayer(hcom_info.pComm.get(), 0, &rankList, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 4);
    for (int i=0; i<rankNum; i++) {
        EXPECT_EQ(rankList[i], i+4); //当前rank是5，L0 ranklist应该是 {4, 5, 6, 7}
    }
 
    ret = CommGetInstRanksByNetLayer(hcom_info.pComm.get(), 1, &rankList, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 8);
    for (int i=0; i<rankNum; i++) {
        EXPECT_EQ(rankList[i], i); ////当前rank是5，L1 ranklist应该是 {0, 1, 2, 3, 4, 5, 6, 7}
    }
 
    ret = CommGetInstSizeListByNetLayer(hcom_info.pComm.get(), 0, &rankSizeList, &rankListNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankListNum, 2);
    for (int i=0; i<rankListNum; i++) {
        EXPECT_EQ(rankSizeList[i], 4); //双机，L0 {4, 4}
    }
 
    ret = CommGetInstSizeListByNetLayer(hcom_info.pComm.get(), 1, &rankSizeList, &rankListNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankListNum, 1);
    EXPECT_EQ(rankSizeList[0], 8);  //双机 L1 {8}
}
 
TEST_F(HcclIndependentOpRankGraphTest, Ut_A3CommGetRankGraph_When_RightPara_Expect_Success)
{
    MOCKER(RegisterKernel).stubs().with().will(returnValue(HCCL_SUCCESS));
    setenv("HCCL_OP_RETRY_ENABLE", "L1:0,L2:0", 1);
    InitEnvVarParam();
    //通信域创建
    HcclResult ret = HCCL_SUCCESS;
    HcomInfo hcom_info;
    HcclRootInfo rootInfo;
    ret = hccl::hcclComm::GetUniqueId(&rootInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    hrtSetDevice(0);
    ret = DlRaFunction::GetInstance().DlRaFunctionInit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TopoMeta topoMate;
    RankTable_For_LLT::GenTopoMeta(topoMate, 2, 2, 4);//A3 2个超节点,每个超节点是2server,一个server4卡
    RankTable_t rankTable_;
    RankTable_For_LLT::GenRankTable(topoMate, rankTable_);
    hcom_info.rankTable = rankTable_;
    u32 rankId = 9;//当前rank是9，superod 1 server 0 
    DevType deviceType;
    s32 logicDevId;
    hrtGetDevice(&logicDevId);
    hrtGetDeviceType(deviceType);
    hcom_info.params.totalRanks = rankTable_.rankNum;
    hcom_info.params.rank = rankId;
    hcom_info.params.userRank = rankId;
    hcom_info.params.serverId = rankTable_.rankList[rankId].serverId;
    hcom_info.params.logicDevId = logicDevId;
    hcom_info.params.deviceType = DevType::DEV_TYPE_910_93;
    hcom_info.devId = -1;
 
    u32 rank = hcom_info.params.rank;
    RankInfo_t temp_rankList = hcom_info.rankTable.rankList[rank];
    sal_memset(hcom_info.params.id.internal, HCCL_ROOT_INFO_BYTES, 0, sizeof(hcom_info.params.id.internal));
    sal_memcpy(hcom_info.params.id.internal, sizeof(HcclRootInfo), &rootInfo, sizeof(HcclRootInfo));
 
    hcom_info.pComm.reset(new (std::nothrow) hccl::hcclComm(200, 200, HCCL_WORLD_GROUP));
    rtModel_t model = (void *)1;
    CommConfig commConfig("hccl_world_group");
    ret = hcom_info.pComm->init(hcom_info.params, commConfig, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = hcom_info.pComm->SetIndependentOpConfig(commConfig, hcom_info.rankTable);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    uint32_t *netlayers = nullptr;
    uint32_t netLayersNum = 0;
    uint32_t rankNum = 0;
    uint32_t rankListNum = 0;
    uint32_t *rankList = nullptr;
    uint32_t *rankSizeList = nullptr;
    CommTopo topoType;
    u32 len = 0;
    void *rankgraph = nullptr;
 
    ret = CommGetRankGraph(hcom_info.pComm.get(), RANK_GRAPH_910_93, &rankgraph, &len);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(len, 16 *sizeof(RankInfo_t));
 
    ret = CommGetNetLayers(hcom_info.pComm.get(), &netlayers, &netLayersNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netLayersNum, 3);
    for (int i=0; i<netLayersNum; i++) {
        EXPECT_EQ(netlayers[i], i);
    }
 
    ret = CommGetInstTopoTypeByNetLayer(hcom_info.pComm.get(), 0, &topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, CommTopo::COMM_TOPO_910_93);
 
    ret = CommGetInstTopoTypeByNetLayer(hcom_info.pComm.get(), 1, &topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, CommTopo::COMM_TOPO_CLOS);
 
    ret = CommGetInstTopoTypeByNetLayer(hcom_info.pComm.get(), 2, &topoType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, CommTopo::COMM_TOPO_CLOS);
 
    ret = CommGetInstSizeByNetLayer(hcom_info.pComm.get(), 0, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 4); //rankId 9  server内 L0 {8, 9, 10, 11}
 
    ret = CommGetInstSizeByNetLayer(hcom_info.pComm.get(), 1, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 8);//rankId 9  superPod内 L1 {8, 9, 10, 11, 12, 13, 14, 15}
 
    ret = CommGetInstSizeByNetLayer(hcom_info.pComm.get(), 2, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 16);//rankId 9  所有rank L2 {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
 
    ret = CommGetInstRanksByNetLayer(hcom_info.pComm.get(), 0, &rankList, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 4);
    for (int i=0; i<rankNum; i++) {
        EXPECT_EQ(rankList[i], i+8); //rankId 9  server内 L0 {8, 9, 10, 11}
    }
 
    ret = CommGetInstRanksByNetLayer(hcom_info.pComm.get(), 1, &rankList, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 8);
    for (int i=0; i<rankNum; i++) {
        EXPECT_EQ(rankList[i], i+8); //当前rank是9，L1 ranklist应该是 {8, 9, 10, 11, 12, 13, 14, 15}
    }
 
    ret = CommGetInstRanksByNetLayer(hcom_info.pComm.get(), 2, &rankList, &rankNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankNum, 16);
    for (int i=0; i<rankNum; i++) {
        EXPECT_EQ(rankList[i], i); //rankId 9  所有rank L2 {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
    }
 
    ret = CommGetInstSizeListByNetLayer(hcom_info.pComm.get(), 0, &rankSizeList, &rankListNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankListNum, 4);
    for (int i=0; i<rankListNum; i++) {
        EXPECT_EQ(rankSizeList[i], 4); //L0 {4, 4, 4, 4}
    }
 
    ret = CommGetInstSizeListByNetLayer(hcom_info.pComm.get(), 1, &rankSizeList, &rankListNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankListNum, 2);
    EXPECT_EQ(rankSizeList[0], 8);
    EXPECT_EQ(rankSizeList[1], 8); // L1 {8, 8}
 
    ret = CommGetInstSizeListByNetLayer(hcom_info.pComm.get(), 2, &rankSizeList, &rankListNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankListNum, 1);
    EXPECT_EQ(rankSizeList[0], 16);  //L2 {16}
}