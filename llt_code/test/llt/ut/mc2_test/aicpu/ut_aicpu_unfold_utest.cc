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

#ifndef private
#define private public
#define protected public
#endif
#include "common/aicpu_kfc_def.h"
#include "framework/aicpu_kfc_rpc_server.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_hdc.h"
#include "framework/aicpu_kfc_process.h"
#include "framework/aicpu_kfc_retry_process.h"
#include "aicpu_kfc/aicpu_kfc_interface.h"
#include "hccl_aicpu_interface.h"
#include "hccl_aicpu_transport_interface.h"
#include "algorithm/aicpu_allreduce.h"
#include "algorithm/task_orchestrator.h"
#include "algorithm/aicpu_dmy_cal_allreduce.h"
#include "aicpu_hccl_sqcq.h"
#include "coll_native_executor_base.h"
#include "../stub/llt_hccl_stub_mc2.h"
#include "../stub/llt_hccl_stub.h"
#include "hccl_alg.h"
#include "hccl_impl.h"
#include "dispatcher_aicpu.h"
#include "hccl_communicator.h"
#include "alg_template_base_pub.h"
#include "utils/aicpu_hdc_utils.h"
#include "pub_inc/stream_pub.h"
#include "executor_tracer.h"
#include "aicpu_kfc/common/aicpu_kfc_tiling_utils.h"
#include "common/aicpu_hccl_common.h"
#include "framework/aicpu_kfc_rpc_serverv2.h"
#include "framework/aicpu_kfc_prof.h"
#include "kernel_tiling/kernel_tiling.h"
#include "coll_all_gather_executor.h"
#include "hccl_common.h"
#include "dispatcher_aicpu_pub.h"
#include "profiling_manager_device.h"
#include "hdc_pub.h"
#include "dispatcher.h"
#include "dltrace_function.h"
#include "dlra_function.h"
#include "hccl_network.h"
#include "adapter_rts.h"
#include "adapter_trace.h"
#include "transport_device_ibverbs_pub.h"
#include "aicpu_kfc/common/aicpu_kfc_utils.h"
#include "aicpu_one_side_service.h"
#include "dlhns_function.h"
#include "order_launch.h"
#undef private
#undef protected

using namespace std;
using namespace HcclApi;

constexpr uint32_t BEGINSQEPOS = 10;
constexpr uint32_t ENDSQEPOS = 100;
HcclResult QuerySqStatusByTypeStub1(uint32_t devId, uint32_t sqId, drvSqCqPropType_t type, uint32_t &outVal)
{
    outVal = ENDSQEPOS;
    return HCCL_SUCCESS;
}

HcclResult QuerySqStatusByTypeStub2(uint32_t devId, uint32_t sqId, drvSqCqPropType_t type, uint32_t &outVal)
{
    outVal = BEGINSQEPOS;
    return HCCL_SUCCESS;
}

HcclResult QuerySqStatusByTypeStub3(uint32_t devId, uint32_t sqId, drvSqCqPropType_t type, uint32_t &outVal)
{
    outVal = INVALID_UINT;
    return HCCL_SUCCESS;
}

void CallMC2MaintenanceThreadStub(AicpuComContext *ctx)
{
    return;
}

class AicpuUnfold_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuUnfold_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AicpuUnfold_UT TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        set_board_id(0x0000);
        s32 ndev = 8;
        for (s32 i = 0; i < ndev; i++) {
            set_chip_type_stub(i, static_cast<s32>(DevType::DEV_TYPE_910B));
        }
        g_stubDevType = DevType::DEV_TYPE_910B;
        MOCKER(halGetDeviceInfo).stubs().with(any()).will(invoke(StubhalGetDeviceInfo));
        MOCKER(QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub1));
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "AicpuUnfold_UT Test SetUP" << std::endl;
        MOCKER(AicpuHcclProcess::CallMC2MaintenanceThread).stubs().will(invoke(CallMC2MaintenanceThreadStub));
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "AicpuUnfold_UT Test TearDown" << std::endl;
    }
};
#define TEST_RANK_SIZE 4
#define REMOTE_TAG_LOOP_NUM 3
string g_tag = "hcom_aicpu_unfold";

static void TestConstructParam(HcclCommParams &params, RankTable_t &rankTable)
{
    string commId = "comm ";
    memcpy_s(params.id.internal, HCCL_ROOT_INFO_BYTES, commId.c_str(), commId.length() + 1);
    params.rank = 0;
    params.totalRanks = TEST_RANK_SIZE;
    params.isHeterogComm = false;
    params.logicDevId = 0;
    params.commWorkMode = WorkMode::HCCL_MODE_NORMAL;
    params.deviceType = DevType::DEV_TYPE_910;

    rankTable.collectiveId = "192.168.0.101-8000-8001";
    vector<RankInfo_t> rankVec(TEST_RANK_SIZE);

    rankVec[0].rankId = 0;
    rankVec[0].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr1(1694542016);
    rankVec[0].deviceInfo.deviceIp.push_back(ipAddr1);  // 101.0.168.192
    rankVec[0].serverIdx = 0;
    rankVec[0].serverId = "192.168.0.101";

    rankVec[1].rankId = 1;
    rankVec[1].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr2(1711319232);
    rankVec[1].deviceInfo.deviceIp.push_back(ipAddr2);  // 101.0.168.192
    rankVec[1].serverIdx = 1;
    rankVec[1].serverId = "192.168.0.101";

    rankVec[2].rankId = 2;
    rankVec[2].deviceInfo.devicePhyId = 0;
    HcclIpAddress ipAddr3(1694542017);
    rankVec[2].deviceInfo.deviceIp.push_back(ipAddr3);  // 101.0.168.192
    rankVec[2].serverIdx = 0;
    rankVec[2].serverId = "192.168.0.102";

    rankVec[3].rankId = 3;
    rankVec[3].deviceInfo.devicePhyId = 1;
    HcclIpAddress ipAddr4(1711319233);
    rankVec[3].deviceInfo.deviceIp.push_back(ipAddr4);  // 101.0.168.192
    rankVec[3].serverIdx = 1;
    rankVec[3].serverId = "192.168.0.102";

    rankTable.rankList.assign(rankVec.begin(), rankVec.end());
    rankTable.deviceNum = TEST_RANK_SIZE;
    rankTable.serverNum = TEST_RANK_SIZE / 2;
}

template <typename T>
static HcclResult CopyVectorToDeviceMem(const u64 len, DeviceMem &dstDeviceMem, const std::vector<T> &srcVec)
{
#ifndef CCL_KERNEL_AICPU
    u64 memSize = len;
    dstDeviceMem = DeviceMem::alloc(memSize);
    CHK_RET(hrtMemSet(dstDeviceMem.ptr(), len, len));

    std::shared_ptr<HostMem> srcHostMem;
    HostMem tmpBuffer = HostMem::alloc(len);
    EXECEPTION_CATCH((srcHostMem = std::make_shared<HostMem>(std::move(tmpBuffer))), return HCCL_E_PTR);
    CHK_SMART_PTR_NULL(srcHostMem);
    CHK_SAFETY_FUNC_RET(memset_s(srcHostMem.get()->ptr(), len, 0, len));
    std::copy(srcVec.begin(), srcVec.end(), static_cast<T *>(srcHostMem.get()->ptr()));
    CHK_RET(hrtMemSyncCopy(
        dstDeviceMem.ptr(), len, srcHostMem.get()->ptr(), len, HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE));
#endif
    return HCCL_SUCCESS;
}

static HcclResult BuildHierarchicalAlgOption(const std::string &algName, DeviceMem &dstTlvDeviceMem, u64 &tlvLen)
{
#ifndef CCL_KERNEL_AICPU
    std::map<AHCConcOpType, TemplateType> hierarchicalAlgOption= {
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_NB},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_NB},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_NB},

        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_RING},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_RING},
        {{AHCLevel::AHC_LEVEL_0, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_RING},

        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_NB},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_NB},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTRA, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_NB},

        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_REDUCE_SCATTER}, TemplateType::TEMPLATE_REDUCESCATTER_RING},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLREDUCE}, TemplateType::TEMPLATE_ALL_REDUCE_RING},
        {{AHCLevel::AHC_LEVEL_1, ConcType::CONC_INTER, AHCOpType::AHC_OP_TYPE_ALLGATHER}, TemplateType::TEMPLATE_ALL_GATHER_RING}
    };
    if (dstTlvDeviceMem.ptr() == nullptr) {
        std::vector<u32> hierarchicalAlgOptionVec;
        for (auto it = hierarchicalAlgOption.begin(); it != hierarchicalAlgOption.end(); ++it) {
            HCCL_DEBUG("[HcclCommunicator][BuildHierarchicalAlgOption] Level [%n], ConcType[%n] AHCOpType[%n], TemplateType [%n]",
                it->first.ahcLevel, it->first.concType, it->first.ahcOpType, it->second);
            hierarchicalAlgOptionVec.push_back(static_cast<u32>(it->first.ahcLevel));
            hierarchicalAlgOptionVec.push_back(static_cast<u32>(it->first.concType));
            hierarchicalAlgOptionVec.push_back(static_cast<u32>(it->first.ahcOpType));
            hierarchicalAlgOptionVec.push_back(static_cast<u32>(it->second));
        }
        tlvLen = hierarchicalAlgOptionVec.size();
        CHK_RET(CopyVectorToDeviceMem(tlvLen * sizeof(u32), dstTlvDeviceMem, hierarchicalAlgOptionVec));
    }
#endif
    return HCCL_SUCCESS;
}

static HcclResult BuildOpTopoResVectorTlvParam(const std::string &algName,
    const std::vector<std::vector<std::vector<std::vector<u32>>>> &inputVectorInfo, DeviceMem &dstTlvDeviceMem, u64 &tlvLen)
{
#ifndef CCL_KERNEL_AICPU
    vector<u32> tlv;
    CommonTlv commonTlv;
    for (u16 level0Idx = 0; level0Idx < inputVectorInfo.size(); level0Idx++) {
        for (u16 level1Idx = 0; level1Idx < inputVectorInfo[level0Idx].size(); level1Idx++) {
            for (u16 level2Idx = 0; level2Idx < inputVectorInfo[level0Idx][level1Idx].size(); level2Idx++) {
                commonTlv.type = (((level0Idx << TOP_HIERARCHICAL_COMM_LEVEL0_SHIFT) | level1Idx) << TOP_HIERARCHICAL_COMM_LEVEL1_SHIFT) | level2Idx;
                commonTlv.length = (sizeof(LENGTH_TYPE) + sizeof(TAG_TYPE)) +
                                        inputVectorInfo[level0Idx][level1Idx][level2Idx].size() * sizeof(RANK_TYPE);
                tlv.push_back(commonTlv.type);
                tlv.push_back(commonTlv.length);
                tlv.insert(tlv.end(), inputVectorInfo[level0Idx][level1Idx][level2Idx].begin(),
                        inputVectorInfo[level0Idx][level1Idx][level2Idx].end());
            }
        }
    }
    for (u64 idx = 0; idx < tlv.size(); idx++) {
        HCCL_DEBUG("[HcclCommunicator][BuildOpTopoResVectorTlvParam] idx[%lu] tlv[%lu]", idx, tlv[idx]);
    }
    tlvLen = tlv.size() * sizeof(u32);
    CHK_RET(CopyVectorToDeviceMem(tlvLen, dstTlvDeviceMem, tlv));
#endif
    return HCCL_SUCCESS;
}

static HcclResult BuildOpTopoResTlvParam(const std::string &algName,
    const std::vector<std::vector<std::vector<u32>>> &inputVectorInfo, DeviceMem &dstTlvDeviceMem, u64 &tlvLen,bool errorcommonTlv = false)
{
#ifndef CCL_KERNEL_AICPU
    vector<u32> tlv;
    CommonTlv commonTlv;
    for (u16 level0Idx = 0; level0Idx < inputVectorInfo.size(); level0Idx++) {
        for (u16 level1Idx = 0; level1Idx < inputVectorInfo[level0Idx].size(); level1Idx++) {
            commonTlv.type = (level0Idx << TOP_COMM_LEVEL0_SHIFT | level1Idx);
            commonTlv.length = (sizeof(LENGTH_TYPE) + sizeof(TAG_TYPE)) +
                               inputVectorInfo[level0Idx][level1Idx].size() * sizeof(RANK_TYPE) + errorcommonTlv;
            tlv.push_back(commonTlv.type);
            tlv.push_back(commonTlv.length);
            tlv.insert(
                tlv.end(), inputVectorInfo[level0Idx][level1Idx].begin(), inputVectorInfo[level0Idx][level1Idx].end());
        }
    }
    for (u64 idx = 0; idx < tlv.size(); idx++) {
        HCCL_DEBUG("[HcclCommunicator][BuildOpTopoResTlvParam] idx[%lu] tlv[%lu]", idx, tlv[idx]);
    }
    tlvLen = tlv.size() * sizeof(u32);
    CHK_RET(CopyVectorToDeviceMem(tlvLen, dstTlvDeviceMem, tlv));
#endif
    return HCCL_SUCCESS;
}

static DeviceMem dstTlvDeviceMem;
static DeviceMem dstDeviceMem;
static DeviceMem nicListdstDeviceMem;
static DeviceMem serverAndsuperPodToRankDevice;
static DeviceMem IsUsedRdmaPairVecMem;
static DeviceMem pairLinkCounterVecMem;
static DeviceMem complanSubGroupRankDeviceMem;
static DeviceMem hierarchicalAlgOptionVecDeviceMem;
static std::vector<u32> niclist;
static std::vector<std::vector<std::vector<u32>>> commPlaneRanks = {{
                                                                        {0, 1}
                                                                    },
    {},
    {{0, 2}, {1, 3}},
    {},
    {{0}},
    {{0, 1}},
    {{0, 2}},
    {{0, 1, 3, 2}},
    {{0, 1, 2, 3}}};
static std::vector<std::vector<std::vector<u32>>> serverAndsuperPodToRank = {{
                                                                          {0, 1},
                                                                          {2, 3}},
    {
        {0, 1, 2, 3},
    }};

static std::vector<u32> isUsedRdmaPairVec = {3, 1, 2, 1, 1, 0, 0, 0};


static std::vector<u32> pairLinkCounterVec = {3, 0, 2, 0, 1, 0, 0, 2};
static std::vector<bool> bridgeRank{true, false};
static std::vector<std::vector<std::vector<std::vector<u32>>>> CommPlaneSubGroupVector = {{{{ 0, 1 }, { 2, 3 }}}};

template <typename T>
std::unordered_map<u32, T> PairVecTranstoMap(const std::vector<u32>& PairVec)
{
    std::unordered_map<u32, T> temp;
    for (int i = 0; i< PairVec.size(); i+=2) {
        temp[PairVec[i]] = static_cast<T>(PairVec[i+1]);
    }
    return temp;
}

HcclResult TestConstructHcclOpResParam(HcclOpResParam &paramTask, std::vector<void *> &bufferVec, std::string &hcomId,
    SqCqeContext &sqeCqeContext)
{
    memset_s(&paramTask, sizeof(HcclOpResParam), 0, sizeof(HcclOpResParam));
    paramTask.localUsrRankId = 0;
    paramTask.rankSize = TEST_RANK_SIZE;
    strcpy(paramTask.hcomId, hcomId.c_str());
    paramTask.localRes.signalNum = LOCAL_NOTIFY_MAX_NUM;
    paramTask.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (uint32_t i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        paramTask.localRes.streamParam[i].streamInfo.streamIds = 52 + i;
        paramTask.localRes.streamParam[i].streamInfo.sqIds = 52 + i;
        paramTask.localRes.streamParam[i].streamInfo.cqIds = 52 + i;
        paramTask.localRes.streamParam[i].streamInfo.logicCqids = 52 + i;
    }
    uint64_t resId = 0;
    for (uint32_t i = 0; i < LOCAL_NOTIFY_MAX_NUM; i++) {
        paramTask.localRes.localSignals[i].resId = ++resId;
        paramTask.localRes.localSignals[i].addr = 0x10 + i;
        paramTask.localRes.localSignals[i].devId = 0;
        paramTask.localRes.localSignals[i].tsId = 0;
        paramTask.localRes.localSignals[i].rankId = i;
    }
    for (uint32_t i = 0; i < AICPU_OP_NOTIFY_MAX_NUM; i++) {
        paramTask.localRes.aicpuOpNotify[i].resId = ++resId;
        paramTask.localRes.aicpuOpNotify[i].addr = 0x1000 + i;
        paramTask.localRes.aicpuOpNotify[i].devId = 0;
        paramTask.localRes.aicpuOpNotify[i].tsId = 0;
        paramTask.localRes.aicpuOpNotify[i].rankId = i;
    }
    paramTask.aicpuOrderNotify.resId = ++resId;
    paramTask.aicpuOrderNotify.addr = 0x1000;
    paramTask.aicpuOrderNotify.devId = 0;
    paramTask.aicpuOrderNotify.tsId = 0;
    paramTask.aicpuOrderNotify.rankId = 0;

    paramTask.localRes.mainStreamParam.streamInfo.streamIds = 1111;
    paramTask.localRes.mainStreamParam.streamInfo.sqIds = 1111;
    paramTask.localRes.mainStreamParam.streamInfo.cqIds = 1111;
    paramTask.localRes.mainStreamParam.streamInfo.logicCqids = 1111;

    paramTask.aicpuOrderStreamParam.streamInfo.streamIds = 1112;
    paramTask.aicpuOrderStreamParam.streamInfo.sqIds = 1112;
    paramTask.aicpuOrderStreamParam.streamInfo.cqIds = 1112;
    paramTask.aicpuOrderStreamParam.streamInfo.logicCqids = 1112;
 
    paramTask.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    paramTask.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    for (u32 i = 0; i < paramTask.localRes.streamNum; i++) {
        paramTask.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        paramTask.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }
    paramTask.aicpuOrderStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    paramTask.aicpuOrderStreamParam.sqCqContextSize = sizeof(SqCqeContext);

    ListCommonInit(&paramTask.localRes.nextTagRes, &paramTask.localRes.nextTagRes);

    // 每张卡共有3个tag
    int loopNum = REMOTE_TAG_LOOP_NUM;
    while (loopNum > 0) {
        void *tmpTagBuffer = (void *)calloc(1, sizeof(HccltagLocalResV2));
        bufferVec.push_back(tmpTagBuffer);
        HccltagLocalResV2 *tagBufferPtr = reinterpret_cast<HccltagLocalResV2 *>(tmpTagBuffer);
        tagBufferPtr->ScratchmemSize = 0x1000000;
        tagBufferPtr->Scratchmem = 0x1000000;
        string newTag = g_tag + to_string(loopNum);
        strcpy(tagBufferPtr->tag, newTag.c_str());
        ListCommonInit(&tagBufferPtr->nextTagRes, &tagBufferPtr->nextTagRes);
        ListCommonAddHead(&tagBufferPtr->nextTagRes,
            &tagBufferPtr->nextTagRes,
            &paramTask.localRes.nextTagRes,
            &paramTask.localRes.nextTagRes);
        loopNum--;
    }

    // 共有16个卡
    paramTask.remoteResNum = TEST_RANK_SIZE;
    for (uint32_t i = 0; i < TEST_RANK_SIZE; i++) {
        void *tmpBuffer = (void *)calloc(1, sizeof(HcclRankRelationResV2));
        bufferVec.push_back(tmpBuffer);
        paramTask.remoteRes[i].nextDevicePtr = reinterpret_cast<u64>(tmpBuffer);
        HcclRankRelationResV2 *bufferPtr = reinterpret_cast<HcclRankRelationResV2 *>(tmpBuffer);
        if (bufferPtr == nullptr) {
            HCCL_ERROR("DDDDDDDDDDDDDD [%u]", sizeof(HcclRankRelationResV2));
            return HCCL_SUCCESS;
        }
        bufferPtr->windowsIn = i + 1;
        bufferPtr->windowsOut = i + 1;
        ListCommonInit(&bufferPtr->nextTagRes, &bufferPtr->nextTagRes);

        // 每张卡共有3个tag
        int loopNum = REMOTE_TAG_LOOP_NUM;

        while (loopNum > 0) {
            void *tmpTagBuffer = (void *)calloc(1, sizeof(HccltagRemoteResV2));
            bufferVec.push_back(tmpTagBuffer);
            HccltagRemoteResV2 *tagBufferPtr = reinterpret_cast<HccltagRemoteResV2 *>(tmpTagBuffer);
            string newTag = g_tag + to_string(loopNum);
            strcpy(tagBufferPtr->tag, newTag.c_str());

            for (uint32_t j = 0; j < LINK_P2P_MAX_NUM; j++) {
                tagBufferPtr->linkP2p.localIpcSignal[j].resId = ++resId;
                tagBufferPtr->linkP2p.localIpcSignal[j].addr = 0x2000 + j;
                tagBufferPtr->linkP2p.localIpcSignal[j].devId = 0;
                tagBufferPtr->linkP2p.localIpcSignal[j].tsId = 0;
                tagBufferPtr->linkP2p.localIpcSignal[j].rankId = j;

                tagBufferPtr->linkP2p.remoteIpcSignal[j].resId = ++resId;
                tagBufferPtr->linkP2p.remoteIpcSignal[j].addr = 0x2000 + j;
                tagBufferPtr->linkP2p.remoteIpcSignal[j].devId = 0;
                tagBufferPtr->linkP2p.remoteIpcSignal[j].tsId = 0;
                tagBufferPtr->linkP2p.remoteIpcSignal[j].rankId = j;

                tagBufferPtr->linkP2pSio.localIpcSignal[j].resId = ++resId;
                tagBufferPtr->linkP2pSio.localIpcSignal[j].addr = 0x2000 + j;
                tagBufferPtr->linkP2pSio.localIpcSignal[j].devId = 0;
                tagBufferPtr->linkP2pSio.localIpcSignal[j].tsId = 0;
                tagBufferPtr->linkP2pSio.localIpcSignal[j].rankId = j;

                tagBufferPtr->linkP2pSio.remoteIpcSignal[j].resId = ++resId;
                tagBufferPtr->linkP2pSio.remoteIpcSignal[j].addr = 0x2000 + j;
                tagBufferPtr->linkP2pSio.remoteIpcSignal[j].devId = 0;
                tagBufferPtr->linkP2pSio.remoteIpcSignal[j].tsId = 0;
                tagBufferPtr->linkP2pSio.remoteIpcSignal[j].rankId = j;
            }
            constexpr u32 RDMA_NOTIFY_MIN_NUM = 3;
            constexpr u32 RDMA_NOTIFY_MAX_NUM = 8192;
            std::vector<HcclSignalInfo> localInfo(RDMA_NOTIFY_MAX_NUM);
            std::vector<u64> remoteInfo(RDMA_NOTIFY_MAX_NUM);
            for (uint32_t k = 0; k < 1; k++) {
                tagBufferPtr->linkRoce[k].localMem[0].size = 4;
                tagBufferPtr->linkRoce[k].localMem[0].addr = 0x2000;
                tagBufferPtr->linkRoce[k].localMem[0].key = 0;
                tagBufferPtr->linkRoce[k].localMem[1].size = 4;
                tagBufferPtr->linkRoce[k].localMem[1].addr = 0x2001;
                tagBufferPtr->linkRoce[k].localMem[1].key = 0;
                tagBufferPtr->linkRoce[k].notifyValue = 0x4000;
                tagBufferPtr->linkRoce[k].notifyValueKey = 0;
                tagBufferPtr->linkRoce[k].remoteNotifyKey = 0;
                tagBufferPtr->linkRoce[k].chipId = 0;
                for (uint32_t j = 0; j < RDMA_NOTIFY_MAX_NUM; j++) {
                    localInfo[j].resId = ++resId;
                    localInfo[j].addr = 0x3000 + j;
                    localInfo[j].devId = 0;
                    localInfo[j].tsId = 0;
                    localInfo[j].rankId = j;
                    remoteInfo[j] = 0x5000 + j;
                    tagBufferPtr->linkRoce[k].QpInfo[j / 3].qpPtr = 0x6000;
                    tagBufferPtr->linkRoce[k].QpInfo[j / 3].sqIndex = 1;
                    tagBufferPtr->linkRoce[k].QpInfo[j / 3].dbIndex = 2;
                }
                tagBufferPtr->linkRoce[k].localNotifyList = reinterpret_cast<u64>(localInfo.data());
                tagBufferPtr->linkRoce[k].remoteNotifyList = reinterpret_cast<u64>(remoteInfo.data());
            }

            ListCommonInit(&tagBufferPtr->nextTagRes, &tagBufferPtr->nextTagRes);
            ListCommonAddHead(
                &tagBufferPtr->nextTagRes, &tagBufferPtr->nextTagRes, &bufferPtr->nextTagRes, &bufferPtr->nextTagRes);
            loopNum--;
        }
    }

    {
        HcclResult ret = HCCL_SUCCESS;
        HcclCommParams params;
        RankTable_t rankTable;
        TestConstructParam(params, rankTable);
        params.deviceType = DevType::DEV_TYPE_910;

        std::string serverId;
        {
            for (u32 i = 0; i < rankTable.rankList.size(); i++) {
                if (rankTable.rankList[i].rankId == params.rank) {
                    serverId = rankTable.rankList[i].serverId;
                    break;
                }
            }
            if (serverId.empty()) {
                HCCL_ERROR("[Get][ServerId]GetServerId fail");
                return HCCL_E_PARA;
            }
            ServRankInfo_t servRankInfo;
            // 按server重新组织rank信息，便于后续校验及信息填写
            for (size_t index = 0; index < rankTable.rankList.size(); ++index) {
                const RankInfo_t &rankInfo = rankTable.rankList[index];
                std::string serverId = SalTrim(rankInfo.serverId);
                // 以serverID为索引，将server下的ranks放入vector
                ServRankInfo_t::iterator itr = servRankInfo.find(serverId);
                if (itr != servRankInfo.end()) {
                    itr->second.push_back(rankInfo);
                } else {
                    std::vector<RankInfo_t> rankInfoList;
                    rankInfoList.push_back(rankInfo);
                    std::pair<std::string, std::vector<RankInfo_t>> rankInfoPair(serverId, rankInfoList);
                    servRankInfo.insert(rankInfoPair);
                }
            }
            // 每个server下的rank列表按设备Id从小到大的顺序排序
            for (auto &iter : servRankInfo) {
                std::sort(iter.second.begin(),
                    iter.second.end(),
                    [&](const RankInfo_t &left, const RankInfo_t &right) -> bool {
                        return left.deviceInfo.devicePhyId < right.deviceInfo.devicePhyId;
                    });
            }
            niclist.clear();
            for (auto iter : servRankInfo[serverId]) {
                if (((!iter.hostIp.IsInvalid()) || (!iter.deviceInfo.deviceIp[0].IsInvalid())) &&
                    (iter.deviceInfo.devicePhyId != HOST_DEVICE_ID)) {
                    niclist.push_back(iter.deviceInfo.devicePhyId);
                }
            }
            std::sort(niclist.begin(), niclist.end());
            EXPECT_EQ(2, niclist.size());
            u64 len = niclist.size() * sizeof(u32);
            CopyVectorToDeviceMem(len, nicListdstDeviceMem, niclist);
            paramTask.topoInfo.nicList = reinterpret_cast<u64>(nicListdstDeviceMem.ptr());
            paramTask.topoInfo.nicNum = niclist.size();
        }
        paramTask.topoInfo.userRank = params.rank;
        paramTask.topoInfo.userRankSize = params.totalRanks;
        paramTask.topoInfo.deviceLogicId = params.logicDevId;
        paramTask.topoInfo.isSingleMeshAggregation = false;
        paramTask.topoInfo.deviceNumPerAggregation = TEST_RANK_SIZE;
        paramTask.topoInfo.superPodNum = 0;
        paramTask.topoInfo.devicePhyId = 0;
        paramTask.topoInfo.deviceType = static_cast<u32>(params.deviceType);
        paramTask.topoInfo.topoType = 1;
        {
            u64 size = bridgeRank.size() * sizeof(bool);
            CopyVectorToDeviceMem(size, dstDeviceMem, bridgeRank);
            paramTask.topoInfo.bridgeRank = reinterpret_cast<u64>(dstDeviceMem.ptr());
            paramTask.topoInfo.bridgeRankNum = bridgeRank.size();
            string algName = "allreduce_mesh";
            u64 tlvLen = 0;
            CHK_RET(BuildOpTopoResTlvParam(algName, commPlaneRanks, dstTlvDeviceMem, tlvLen));
            paramTask.topoInfo.complanRank = reinterpret_cast<u64>(dstTlvDeviceMem.ptr());
            paramTask.topoInfo.complanRankLength = tlvLen;

            CHK_RET(BuildOpTopoResTlvParam(algName, serverAndsuperPodToRank, serverAndsuperPodToRankDevice, tlvLen));
            paramTask.topoInfo.serverAndsuperPodRank = reinterpret_cast<u64>(serverAndsuperPodToRankDevice.ptr());
            paramTask.topoInfo.serverAndsuperPodRankLength = tlvLen;

            CHK_RET(BuildOpTopoResVectorTlvParam(algName, CommPlaneSubGroupVector, complanSubGroupRankDeviceMem, tlvLen));
            paramTask.hierarchicalAlgInfo.commplaneSubGroupRank = reinterpret_cast<u64>(complanSubGroupRankDeviceMem.ptr());
            paramTask.hierarchicalAlgInfo.commplaneSubGroupRankLength = tlvLen;

            CHK_RET(BuildHierarchicalAlgOption(algName, hierarchicalAlgOptionVecDeviceMem, tlvLen));
            paramTask.hierarchicalAlgInfo.hierarchicalAlgOptionVec = reinterpret_cast<u64>(hierarchicalAlgOptionVecDeviceMem.ptr());
            paramTask.hierarchicalAlgInfo.hierarchicalAlgOptionNum = tlvLen;

            u64 len = isUsedRdmaPairVec.size() * sizeof(u32);  // key-value，都为u32
            CopyVectorToDeviceMem(len, IsUsedRdmaPairVecMem, isUsedRdmaPairVec);
            paramTask.topoInfo.isUsedRdmaRankPair = reinterpret_cast<u64>(IsUsedRdmaPairVecMem.ptr());
            paramTask.topoInfo.isUsedRdmaRankPairNum = isUsedRdmaPairVec.size();

            len = pairLinkCounterVec.size() * sizeof(u32);  // key-value，都为u32
            CopyVectorToDeviceMem(len, pairLinkCounterVecMem, pairLinkCounterVec);
            paramTask.topoInfo.pairLinkCounter = reinterpret_cast<u64>(pairLinkCounterVecMem.ptr());
            paramTask.topoInfo.pairLinkCounterNum = pairLinkCounterVec.size();
        }
    }

    static char lockBuffer[128];
    paramTask.lockAddr = reinterpret_cast<u64>(lockBuffer);

    for (int i = 0; i < MAX_MODULE_DEVICE_NUM; i++) {
        paramTask.zeroCopyIpcPtrs[i] = 0x0;
    }
    return HCCL_SUCCESS;
}
#define SINGAL_SUB_COMM_NUM 4
#define LEVEL_SUB_COMM_NUM 1
#define OP_COM_NUM 1
HcclResult CalcResRequestStub(HcclCommAicpu *This, const std::string &algName, const OpParam &param,
    std::unique_ptr<CollExecutorBase> &executor, AlgResourceRequest &resourceRequest)
{
    resourceRequest.scratchMemSize = 0;
    resourceRequest.streamNum = LOCAL_STREAM_MAX_NUM;
    resourceRequest.notifyNum = LOCAL_NOTIFY_MAX_NUM;
    resourceRequest.aivBufferRequest = 0;
    resourceRequest.opTransport.resize(OP_COM_NUM);
    static u32 rankId = 0;

    for (int opIdx = 0; opIdx < OP_COM_NUM; opIdx++) {
        LevelNSubCommTransport levelNSubCommTransport;
        levelNSubCommTransport.resize(LEVEL_SUB_COMM_NUM);
        for (int levelIdx = 0; levelIdx < LEVEL_SUB_COMM_NUM; levelIdx++) {
            SingleSubCommTransport singleSubCommTransport;
            singleSubCommTransport.transportRequests.resize(SINGAL_SUB_COMM_NUM);
            for (int i = 0; i < SINGAL_SUB_COMM_NUM; i++) {
                singleSubCommTransport.transportRequests[i].isValid = true;
                singleSubCommTransport.transportRequests[i].remoteUserRank = rankId;
                singleSubCommTransport.transportRequests[i].inputMemType = TransportMemType::CCL_INPUT;
                singleSubCommTransport.transportRequests[i].outputMemType = TransportMemType::CCL_OUTPUT;
                rankId++;
            }
            levelNSubCommTransport[levelIdx] = singleSubCommTransport;
        }
        resourceRequest.opTransport[opIdx] = (levelNSubCommTransport);
    }
    return HCCL_SUCCESS;
}

void GetIpcNotifyToLinkUsedNumStub(HcclCommAicpu *This, const u32 rankId, const std::string &tag,
                                   u32 &localIpcNotifyUseNum, u32 &remoteIpcNotifyUseNum,
                                   std::unordered_map<u32, std::unordered_map<std::string, u32>> localNotifyToLink,
                                   std::unordered_map<u32, std::unordered_map<std::string, u32>> remoteNotifyToLink)
{
    (void)This;
    (void)rankId;
    (void)tag;
    (void)localIpcNotifyUseNum;
    (void)remoteIpcNotifyUseNum;
    (void)localNotifyToLink;
    (void)remoteNotifyToLink;
}

HcclResult GetOpExecCtrlTargetOpStub(
    AicpuHdc *aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, HcclOpIdentifier &opId)
{
    (void)h2dTransfer;
    opId.tag[0] = 't';
    opId.tag[1] = 'e';
    opId.tag[2] = 's';
    opId.tag[3] = 't';
    opId.tag[4] = '_';
    opId.tag[5] = 'B';
    opId.tag[6] = 'S';
    opId.tag[7] = 'R';
    opId.tag[8] = 0;
    opId.srcRank = 0;
    opId.detRank = 1;
    opId.index = 0;
    return HCCL_SUCCESS;
}

TEST_F(AicpuUnfold_UT, UpdateNotifyWaitTimeOut)
{
    SyncMode syncMode = SyncMode::UNLIMITED_TIMEWAITSYNCMODE;
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    auto pDispatcher = new (std::nothrow) DispatcherAiCpu(0);
    hcclCommAicpu->dispatcher_ = pDispatcher;
    EXPECT_TRUE(!hcclCommAicpu->UpdateNotifyWaitTimeOut(syncMode, 68));
    delete hcclCommAicpu;
}
#if 0 // 栈溢出
TEST_F(AicpuUnfold_UT, InitSlaveStreamObjs)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;

    HcclOpResParam paramTask;
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold21";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    paramTask.localRes.streamNum = 41;
    EXPECT_EQ(hcclCommAicpu->InitSlaveStreamObjs(&paramTask),HCCL_E_PARA);

    paramTask.localRes.signalNum  = 99;
    EXPECT_EQ(hcclCommAicpu->InitLocalNotifyObj(&paramTask),HCCL_E_PARA);

    EXPECT_EQ(hcclCommAicpu->ParseTlvToVector(0,1,commPlaneRanks),HCCL_E_PARA);

    EXPECT_EQ(hcclCommAicpu->ParseTlvToVector(paramTask.topoInfo.complanRank,1,commPlaneRanks),HCCL_E_PARA);

    string algName = "allreduce_mesh";
    u64 tlvLen = 0;
    BuildOpTopoResTlvParam(algName, commPlaneRanks, dstTlvDeviceMem, tlvLen,true);
    paramTask.topoInfo.complanRank = reinterpret_cast<u64>(dstTlvDeviceMem.ptr());
    paramTask.topoInfo.complanRankLength = tlvLen;
    EXPECT_EQ(hcclCommAicpu->ParseTlvToVector(paramTask.topoInfo.complanRank,paramTask.topoInfo.complanRankLength,commPlaneRanks),HCCL_E_PARA);

    for(auto buff : bufferVec)
    {
       free(buff);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, InitTopoInfoAndInitResByTag)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    u32 extraNotityNum = 2;

    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold22";
    std::string tag = "hcom_aicpu_unfold9";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    auto isUsedRdmaRankPairNum = paramTask.topoInfo.isUsedRdmaRankPairNum;
    paramTask.topoInfo.isUsedRdmaRankPairNum = 3;
    EXPECT_EQ(hcclCommAicpu->InitTopoInfo(&paramTask),HCCL_E_PARA);
    paramTask.topoInfo.isUsedRdmaRankPairNum = isUsedRdmaRankPairNum;

    auto isUsedRdmaRankPair = paramTask.topoInfo.isUsedRdmaRankPair;
    paramTask.topoInfo.isUsedRdmaRankPair = 0;
    EXPECT_EQ(hcclCommAicpu->InitTopoInfo(&paramTask),HCCL_E_PARA);
    paramTask.topoInfo.isUsedRdmaRankPair = isUsedRdmaRankPair;

    auto pairLinkCounterNum = paramTask.topoInfo.pairLinkCounterNum;
    paramTask.topoInfo.pairLinkCounterNum = 3;
    EXPECT_EQ(hcclCommAicpu->InitTopoInfo(&paramTask),HCCL_E_PARA);
    paramTask.topoInfo.pairLinkCounterNum = pairLinkCounterNum;

    auto pairLinkCounter = paramTask.topoInfo.pairLinkCounter;
    paramTask.topoInfo.pairLinkCounter = 0;
    EXPECT_EQ(hcclCommAicpu->InitTopoInfo(&paramTask),HCCL_E_PARA);
    paramTask.topoInfo.pairLinkCounter = pairLinkCounter;

    auto nicList = paramTask.topoInfo.nicList;
    paramTask.topoInfo.nicList = 0;
    EXPECT_EQ(hcclCommAicpu->InitTopoInfo(&paramTask),HCCL_E_PARA);
    paramTask.topoInfo.nicList = nicList;

    MOCKER_CPP(&HcclCommAicpu::ParseTlvToVector).stubs().will(returnValue(1));
    EXPECT_EQ(hcclCommAicpu->InitTopoInfo(&paramTask),1);
    MOCKER_CPP(&HcclCommAicpu::ParseTlvToVector).stubs().will(returnValue(0));

    for(auto buff : bufferVec)
    {
       free(buff);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
    delete hcclCommAicpu;
}
#endif
TEST_F(AicpuUnfold_UT, InitLocalTagRes)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    ListCommon head;
    ListCommonInit(&head, &head);
    head.nextDevice = 0;
    EXPECT_EQ(hcclCommAicpu->InitLocalTagRes(head),HCCL_E_PARA);
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, InitRemoteTagRes)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    ListCommon head;
    ListCommonInit(&head, &head);
    head.nextDevice = 0;
    u32 rankId = 0;
    u32 extraNotifyNum = 2;
    std::string tag = "hcom_aicpu_unfold10";
    EXPECT_EQ(hcclCommAicpu->InitRemoteTagRes(rankId, head, tag, extraNotifyNum),HCCL_E_PARA);

    ListCommonInit(&head, &head);
    std::vector<void *> bufferVec;
    // 每张卡共有3个tag
    int loopNum = REMOTE_TAG_LOOP_NUM;
    while (loopNum > 0) {
        std::shared_ptr<DeviceMem> tagBuffer;
        void *tmpTagBuffer = (void *)malloc(sizeof(HccltagRemoteResV2));
        bufferVec.push_back(tmpTagBuffer);
        HccltagRemoteResV2 *tagBufferPtr = reinterpret_cast<HccltagRemoteResV2 *>(tmpTagBuffer);
        string newTag = g_tag + to_string(loopNum);
        strcpy(tagBufferPtr->tag, newTag.c_str());
        ListCommonInit(&tagBufferPtr->nextTagRes, &tagBufferPtr->nextTagRes);
        ListCommonAddHead(
            nullptr, &tagBufferPtr->nextTagRes,&head, &head);
        loopNum--;
    }
    MOCKER_CPP(&HcclCommAicpu::InitLinkP2p).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::InitLinkRoce, HcclResult (HcclCommAicpu::*)(HccltagRemoteResV2 *tagRes, HcclLinkRoceV2 *linkRoce, u32 &rankId,
        const std::string &newTag, u32 notifyNum, const bool isBackup, const bool isSecond)).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->InitRemoteTagRes(rankId, head, tag, extraNotifyNum),HCCL_E_PARA);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, GetStreamData)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER(QuerySqStatusByType)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
    MOCKER(QuerySqBaseAddr).stubs().with(any()).will(returnValue(0));
    HcclStreamInfo streamInfo{};
    HcclComStreamInfo comStreamInfo{};
    u32 sqHead = 0;
    u32 sqTail = 0;
    EXPECT_EQ(hcclCommAicpu->GetStreamData(streamInfo, comStreamInfo, sqHead, sqTail),HCCL_E_PARA);
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, InitCclbuffer)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    HcclOpResParam hcclOpResParam;
    hcclOpResParam.localWindowsIn = 0;
    EXPECT_EQ(hcclCommAicpu->InitCclbuffer(&hcclOpResParam),HCCL_SUCCESS);
    delete hcclCommAicpu;
}
#if 0 // 栈溢出
TEST_F(AicpuUnfold_UT, GetSdmaLinksByRankAndTag)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    HcclOpResParam commParam;
    std::string tag = "hcom_aicpu_unfold1";
    std::shared_ptr<Transport> link = make_shared<Transport>(nullptr);
    HcclOpResParam paramTask;
    u32 extraNotifyNum = 2;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold23";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    HcclResult ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    struct SqeRingBuffer sqeRingBuffer;
    sqeRingBuffer.rtsDfxInfo[0].opRingBufferIdx = 0;
    hcclCommAicpu->GetTaskExceptionOpInfo(0, &sqeRingBuffer);
    bool isUseRdma = false;
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_E_PARA));
    EXPECT_EQ(hcclCommAicpu->GetSdmaLinksByRankAndTag(&paramTask, CommTransportsType::SPECIAL, 0, tag,
        link, false, extraNotifyNum), HCCL_E_PARA);
    GlobalMockObject::verify();
    for (u32 rankidx = 0; rankidx < AICPU_MAX_RANK_NUM; rankidx++) {
        hcclCommAicpu->receivedAcks_[rankidx] = false;
    }
    TransportPara para;
    MachinePara machinePara;
    std::shared_ptr<Transport> transport;
    TransportDeviceP2pData transDevP2pData;
    TransportDeviceIbverbsData transDevIbverbsData;
    transport.reset(new (std::nothrow) Transport(
            TransportType::TRANS_TYPE_P2P, para, nullptr, nullptr, machinePara, transDevP2pData, transDevIbverbsData));
    hcclCommAicpu->linkRes_[0][tag] = transport;
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->GetSdmaLinksByRankAndTag(&paramTask, CommTransportsType::SPECIAL, 0, tag,
        link, false, extraNotifyNum), HCCL_SUCCESS);

    MOCKER(AdprofCheckFeatureIsOn).stubs().will(returnValue(1));
    HcclSqeContext sqeContext;
    HcclSqeContext *sqeStr = &sqeContext;
    MOCKER_CPP(&Stream::GetSqeContextPtr)
    .stubs()
    .with(any())
    .will(returnValue(sqeStr));
    EXPECT_EQ(hcclCommAicpu->UpdateProfReportStartSqeIdx(), HCCL_SUCCESS);
    EXPECT_EQ(dfx::ProfilingManager::UpdateStartReportSqeIdx(52, 666), HCCL_SUCCESS);
    EXPECT_EQ(dfx::ProfilingManager::GetStartReportSqeIdx(52), 666);
    dfx::ProfCommInfo profInfo;
    EXPECT_EQ(dfx::ProfilingManager::GetProfInfoByStreamId(52, profInfo), HCCL_SUCCESS);
    EXPECT_EQ(dfx::ProfilingManager::GetProfInfoByStreamId(666, profInfo), HCCL_SUCCESS);

    GlobalMockObject::verify();
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, GetSdmaLinksByRankAndTag_HcclPlusSio)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    MOCKER_CPP(&TransportBase::SignalInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::SetTransportPtpNotify).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Transport::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Transport::SetSupportDataReceivedAck).stubs().will(returnValue(HCCL_SUCCESS));

    for (int k = 0; k < paramTask.remoteResNum; k++) {
        HcclRankRelationResV2 *rankRelationResPtr = reinterpret_cast<HcclRankRelationResV2 *>(paramTask.remoteRes[k].nextDevicePtr);
        ListCommon *curList= reinterpret_cast<ListCommon *>(rankRelationResPtr->nextTagRes.nextDevice);
        HccltagRemoteResV2 *tagRes = nullptr;
        while (curList != &rankRelationResPtr->nextTagRes) {
            tagRes = list_entry(curList, HccltagRemoteResV2, nextTagRes);
            tagRes->linkP2p.remoteMem[INPUT].addr = reinterpret_cast<u64>(inputMem.ptr_);
            tagRes->linkP2p.remoteMem[OUTPUT].addr = reinterpret_cast<u64>(outputMem.ptr_);
            tagRes->linkP2pSio.remoteMem[INPUT].addr = reinterpret_cast<u64>(inputMem.ptr_);
            tagRes->linkP2pSio.remoteMem[OUTPUT].addr = reinterpret_cast<u64>(outputMem.ptr_);
            curList = reinterpret_cast<ListCommon *>(curList->nextDevice);
        }
    }

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportRequest transportRequest_0;
    transportRequest_0.remoteUserRank = 0;
    transportRequest_0.inputMemType = TransportMemType::PARAM_INPUT;
    transportRequest_0.outputMemType = TransportMemType::PARAM_OUTPUT;
    transportRequest_0.isUsedRdma = false;
    transportRequest_0.linkType = TransportLinkType::RESERVED;
    transportRequest_0.notifyNum = 1;
    TransportRequest transportRequest_1;
    transportRequest_1.remoteUserRank = 1;
    transportRequest_1.inputMemType = TransportMemType::PARAM_INPUT;
    transportRequest_1.outputMemType = TransportMemType::PARAM_OUTPUT;
    transportRequest_1.isUsedRdma = false;
    transportRequest_1.linkType = TransportLinkType::HCCS;
    transportRequest_1.notifyNum = 1;
    TransportRequest transportRequest_2;
    transportRequest_2.remoteUserRank = 1;
    transportRequest_2.inputMemType = TransportMemType::CCL_INPUT;
    transportRequest_2.outputMemType = TransportMemType::CCL_OUTPUT;
    transportRequest_2.isUsedRdma = false;
    transportRequest_2.linkType = TransportLinkType::SIO;
    transportRequest_2.notifyNum = 1;

    LINK link0;
    u32 extraNotifyNum = 2;
    std::string newTag = hcomId + "1";
    hcclCommAicpu->receivedAcks_[0] = false;
    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_0, &paramTask, link0, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(hcclCommAicpu->linkRes_.size(), 1U);
    EXPECT_EQ(hcclCommAicpu->linkResSio_.size(), 0U);

    LINK link1;
    hcclCommAicpu->receivedAcks_[1] = false;
    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_1, &paramTask, link1, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(hcclCommAicpu->linkRes_.size(), 2U);
    EXPECT_EQ(hcclCommAicpu->linkResSio_.size(), 0U);

    LINK link2;
    hcclCommAicpu->receivedAcks_[2] = false;
    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_2, &paramTask, link2, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(hcclCommAicpu->linkRes_.size(), 2U);
    EXPECT_EQ(hcclCommAicpu->linkResSio_.size(), 1U);

    std::string tagNotExist = hcomId + "x";
    ret = hcclCommAicpu->CreateLink(tagNotExist, transportRequest_2, &paramTask, link2, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_E_PARA);

    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
    GlobalMockObject::verify();
}
#endif
TEST_F(AicpuUnfold_UT, GetLinksByRankAndTagException)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    std::string tag = "hcom_aicpu_unfold1";
    std::shared_ptr<Transport> link = make_shared<Transport>(nullptr);
    HcclOpResParam paramTask;
    u32 extraNotifyNum = 0;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    MOCKER_CPP(&HcclCommAicpu::RefreshTransportsResForRank).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(
        hcclCommAicpu->GetRdmaLinksByRankAndTag(
            &paramTask, CommTransportsType::SPECIAL, 0, tag, link, false, extraNotifyNum, false),
        HCCL_E_INTERNAL);
    EXPECT_EQ(
        hcclCommAicpu->GetSdmaLinksByRankAndTag(
            &paramTask, CommTransportsType::SPECIAL, 0, tag, link, false, extraNotifyNum),
        HCCL_E_INTERNAL);
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmLaunchProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER_CPP(&HcclCommAicpu::OrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    std::string algName;
    OpParam param;
    AlgResourceResponse algResource;
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;
    std::unique_ptr<CollExecutorBase> executor;
    u32 retryCnt = 0;
    ASSERT_EQ(hcclCommAicpu->HcclOpExecFsmLaunchProcess(
                  algName, param, executor, algResource, state, errorCode, beginSqePos, endSqePos, retryCnt),
        HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmWaitEndProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER_CPP(&HcclCommAicpu::WaitFinishWhileLoop).stubs().with(any()).will(returnValue(HCCL_E_INTERNAL));
    AlgResourceResponse algResource;
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t retryCnt = INVALID_UINT;
    std::string tag = "";
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitEndProcess(param, algResource, state, errorCode, retryCnt, tag, 0), HCCL_E_INTERNAL);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmEndProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    uint32_t retryCnt = INVALID_UINT;
    MOCKER(&AicpuHdc::SetOpExecStatus, HcclResult (AicpuHdc::*)(std::shared_ptr<HDCommunicate> d2hTransfer, HcclOpIdentifier &opId, KfcStatus state, KfcError errorCode, u32 retryCount)).stubs().with(any()).will(returnValue(HCCL_E_PARA));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmEndProcess(retryCnt), HCCL_E_PARA);
    EXPECT_EQ(hcclCommAicpu->dfxExtendInfo_.kfcStatus, DfxKfcStatus::kOneFinished);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

class CollExecutorMock : public CollExecutorBase {
public:
    CollExecutorMock(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher):CollExecutorBase(dispatcher, topoMatcher) {};
    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algRes) {return HCCL_SUCCESS;}
    HcclResult CalcResRequest(const OpParam& param, AlgResourceRequest &resourceRequest) {return HCCL_SUCCESS;}
};

uint8_t g_msgArea[COMM_MAX_WORK_SPACE_SIZE];
uint8_t g_msgAreaBak[COMM_MAX_WORK_SPACE_SIZE];
TEST_F(AicpuUnfold_UT, HcclOpExecFsmLaunchProcessTest)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    AicpuKfcRpcServerV2 rpc;
    memset_s(g_msgArea, sizeof(g_msgArea), 0, sizeof(g_msgArea));
    rpc.Init({reinterpret_cast<u64>(g_msgArea), sizeof(g_msgArea)});
    MOCKER_CPP(&AicpuKfcRpcServerV2::AddCcoreWait).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuKfcRpcServerV2::AddCcoreNotify).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    hcclCommAicpu->SetIsDeviceMode(true);
    hcclCommAicpu->SetAicpuRpcServer(reinterpret_cast<u64>(&rpc));
    MOCKER_CPP(&DispatcherAiCpu::LaunchTask).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommAicpu::LaunchSlaveStreamTask).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    std::string algName;
    OpParam param;
    AlgResourceResponse algResource;
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;
    HcclCommParams params;
    RankTable_t rankTable;
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
    TestConstructParam(params, rankTable);
    implBase->Init(params, rankTable);
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    const HcclDispatcher dispatcher = nullptr;
    std::unique_ptr<CollExecutorBase> executor(new CollExecutorMock(dispatcher, topoMatcher));
    u32 retryCnt = 0;
    hcclCommAicpu->HcclOpExecFsmLaunchProcess(algName, param, executor, algResource, state, errorCode, beginSqePos,
                                              endSqePos, retryCnt);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmLaunchProcessWithStepSize)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    AicpuKfcRpcServerV2 rpc;
    memset_s(g_msgArea, sizeof(g_msgArea), 0, sizeof(g_msgArea));
    rpc.Init({reinterpret_cast<u64>(g_msgArea), sizeof(g_msgArea)});
    rpc.SetStepSize(1);
    MOCKER_CPP(&AicpuKfcRpcServerV2::AddCcoreWait).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuKfcRpcServerV2::AddCcoreNotify).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    hcclCommAicpu->SetIsDeviceMode(true);
    hcclCommAicpu->SetAicpuRpcServer(reinterpret_cast<u64>(&rpc));
    MOCKER_CPP(&DispatcherAiCpu::LaunchTask).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommAicpu::LaunchSlaveStreamTask).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    std::string algName;
    OpParam param;
    AlgResourceResponse algResource;
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;
    HcclCommParams params;
    RankTable_t rankTable;
    params.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
    TestConstructParam(params, rankTable);
    implBase->Init(params, rankTable);
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    const HcclDispatcher dispatcher = nullptr;
    std::unique_ptr<CollExecutorBase> executor(new CollExecutorMock(dispatcher, topoMatcher));
    u32 retryCnt = 0;
    hcclCommAicpu->HcclOpExecFsmLaunchProcess(algName, param, executor, algResource, state, errorCode, beginSqePos,
                                              endSqePos, retryCnt);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

HcclResult GetOpExecCtrlCmdStub1(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kExit;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub2(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kStopExec;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub3(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kNone;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub4(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kRetry;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub5(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kStopLaunch;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub6(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::NsStopLaunch;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub8(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kClear;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub9(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::NsStopLaunch;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub10(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kChangeLink;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub11(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::kReportRetryErr;
    return HCCL_SUCCESS;
}

HcclResult GetOpExecCtrlCmdStub12(AicpuHdc* aicpuHdc, std::shared_ptr<HDCommunicate> h2dTransfer, KfcCommand &cmd)
{
    (void)h2dTransfer;
    cmd = KfcCommand::NsStopLaunch;
    return HCCL_SUCCESS;
}

drvError_t drvQueryProcessHostPidStub(int pid, unsigned int *chip_id, unsigned int *vfid,
    unsigned int *host_pid, unsigned int *cp_type)
{
    if (chip_id != NULL) {
        *chip_id = 0;
    }

    if (vfid != NULL) {
        *vfid = 0;
    }

    if (host_pid != NULL) {
        *host_pid = 0;
    }

    if (cp_type != NULL) {
        *cp_type = 0;
    }

    return DRV_ERROR_NONE;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmStoppingProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;

    OpParam param;
    uint32_t retryCnt;
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(returnValue(HCCL_E_PARA));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt), HCCL_E_PARA);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub1));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExit);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub2));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_STOPPED);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt), HCCL_SUCCESS);
    GlobalMockObject::verify();

    state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub8));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();

    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub5));
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlTargetOp).stubs().with(any()).will(invoke(GetOpExecCtrlTargetOpStub));
    MOCKER_CPP(&HcclCommAicpu::UpdateSuspendStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->retryEnable_ = true;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt), HCCL_SUCCESS);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmStoppedProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(false));
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t retryCnt;
    OpParam param;
    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;
    std::string algName = "test";
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER(&QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub1));
    beginSqePos = BEGINSQEPOS;
    endSqePos = ENDSQEPOS;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER(&QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub2));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER(&QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub3));
    MOCKER_CPP(&HcclCommAicpu::IsTaskExceptionForHccs).stubs().with(any()).will(returnValue(true));
    MOCKER(&AicpuHdcUtils::SetOpExecStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_WAIT_RETRY);
    EXPECT_EQ(errorCode, KfcError::kNone);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpSupportRetry)
{
    std::string algName = "test";
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    OpParam param;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    u8 inplaceSupportRetry;
    u8 retryEnable = 0;
    u8 inPlaceSupportRetryStatus;
    u8 isInplacePreSync;
    u8 isPostSync;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), false);

    retryEnable = 1;
    inplaceSupportRetry = 0;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), false);
    GlobalMockObject::verify();

    inplaceSupportRetry = 1;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();

    inplaceSupportRetry = 1;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(false));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), false);
    GlobalMockObject::verify();

    inplaceSupportRetry = 1;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    param.opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_EIGHT=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_REDUCE;
    param.DataDes.count = 1024;
    param.root = 0;
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================print0=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_RECEIVE;
    param.DataDes.count = 1024;
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_SEVEN=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_SIX=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    hcclCommAicpu->cclbufferSize_ = 1024 * 1024;
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_ONE=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    hcclCommAicpu->cclbufferSize_ = 1024 * 1024;
    algName = "ReduceScatterDeterExecutor";
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_FOUR=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    hcclCommAicpu->cclbufferSize_ = 1024 * 1024;
    algName = "AllReduceRingFor91093Executor";
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_THREE=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    hcclCommAicpu->cclbufferSize_ = 1024 * 1024;
    algName = "AllReduceComm";
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    GlobalMockObject::verify();

    inplaceSupportRetry = 0;
    retryEnable = 0;
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_TWO=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    hcclCommAicpu->cclbufferSize_ = 1024 * 1024;
    algName = "ReduceScatterDeterExecutor";
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), false);
    GlobalMockObject::verify();
    std::cout << "=================case INPLACE_SUPPORT_RETRY_STATUS_FIVE=================" << std::endl;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    param.DataDes.count = 1024;
    hcclCommAicpu->cclbufferSize_ = 1024 * 1024;
    algName = "AllReduceRingFor91093Executor";
    MOCKER(IsInputOutPtrNotNullPtr).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), false);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

static constexpr u32 HCCL_AICPU_WAIT_HOST_BASE_TIME_MS = 200 * 1000;
TEST_F(AicpuUnfold_UT, HcclGetWaitRetryCmdTimeout)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    uint32_t retryCnt = 0;
    hcclCommAicpu->retryHoldTime_ = 10;
    EXPECT_EQ(hcclCommAicpu->HcclGetWaitRetryCmdTimeout(retryCnt), HCCL_AICPU_WAIT_HOST_BASE_TIME_MS + 10);
    hcclCommAicpu->retryIntervalTime_ = 100;
    retryCnt = 1;
    EXPECT_EQ(hcclCommAicpu->HcclGetWaitRetryCmdTimeout(retryCnt), HCCL_AICPU_WAIT_HOST_BASE_TIME_MS + 100);
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmWaitRetryProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    HcclOpExecFSM state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT;
    KfcError errorCode = KfcError::kNone;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    KfcCommand kfcCommand = KfcCommand::kNone;

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub4));
    MOCKER_CPP(&HcclCommAicpu::CleanStream).stubs().with(any()).will(returnValue(HCCL_E_MEMORY));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_E_MEMORY);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kInner);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub1));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExit);
    GlobalMockObject::verify();

    state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT;
    errorCode = KfcError::kNone;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(errorCode, KfcError::kNone);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT);
    GlobalMockObject::verify();

    hcclCommAicpu->endStopLaunch = false;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub6));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_STOP_LAUNCH);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmRetryProcess)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    std::string algName = "test";
    OpParam param;
    AlgResourceResponse algResource;
    HcclOpExecFSM state;
    KfcError errorCode;
    uint32_t retryCnt;
    uint32_t beginSqePos;
    uint32_t endSqePos;
    std::unique_ptr<CollExecutorBase> executor;
    MOCKER_CPP(&HcclCommAicpu::RetryOrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_E_DRV));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_E_DRV);
    GlobalMockObject::verify();
    MOCKER_CPP(&HcclCommAicpu::UpdateOpExecStatus, HcclResult (HcclCommAicpu::*)(HcclOpExecFSM &fsmState, KfcStatus state, KfcError &errorCode, uint32_t retryCnt)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RetryOrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);
    GlobalMockObject::verify();

    MOCKER_CPP(&HcclCommAicpu::RetryOrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(returnValue(HCCL_E_SUSPENDING));
    MOCKER_CPP(&HcclCommAicpu::OrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_E_AGAIN));
    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    hcclCommAicpu->retryEnable_ = true;
    hcclCommAicpu->bsrRetryOp_ = HCCL_SEND;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);
    hcclCommAicpu->bsrRetryOp_ = HCCL_RECV;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, WaitFinishWhileLoop)
{
    MOCKER(QuerySqStatusByType).stubs().will(returnValue(HCCL_SUCCESS));
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = new (std::nothrow) DispatcherAiCpu(0);
    hcclCommAicpu->retryEnable_ = true;
    hcclCommAicpu->debugMode_ = MC2_DEBUG_WAIT_COMM;

    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;


    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    Stream mainStream(streamInfo, true);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    mainStream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);

    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    std::vector<Stream> subStreams;
    for (u32 i = 0; i < subStreams.size(); i++) {
        streamInfo.actualStreamId = i;
        streamInfo.sqId = i;
        subStreams[i] = Stream(streamInfo, false);
    }

    std::string tag = "";
    hcclCommAicpu->dfxExtendInfo_.pollStatus = PollStatus::kStopAsException;
    DispatcherAiCpu * dispatcher = reinterpret_cast<DispatcherAiCpu *>(hcclCommAicpu->dispatcher_);
    dispatcher->dfxTimeOutConfig_.sqeWaitTimeOut = 2;
    MOCKER_CPP(&hccl::HcclCommAicpu::IsTaskExceptionForHccs).stubs().will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->WaitFinishWhileLoop(mainStream,subStreams,tag, 0, param),HCCL_E_SUSPENDING);
    GlobalMockObject::verify();
    MOCKER_CPP(&hccl::HcclCommAicpu::IsTaskExceptionForHccs).stubs().will(returnValue(false));
    EXPECT_EQ(hcclCommAicpu->WaitFinishWhileLoop(mainStream,subStreams,tag, 1, param),HCCL_SUCCESS);
    GlobalMockObject::verify();
    hcclCommAicpu->dfxExtendInfo_.pollStatus = PollStatus::kDefault;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub1));
    EXPECT_EQ(hcclCommAicpu->WaitFinishWhileLoop(mainStream,subStreams,tag, 1, param),HCCL_E_INTERNAL);
    GlobalMockObject::verify();

    u64 curUsec = 961 * static_cast<uint64_t>(NSEC_PER_SEC);
    u64 lastUsec = 0;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(GetCurCpuTimestamp).stubs().will(returnValue(lastUsec)).then(returnValue(curUsec));
    EXPECT_EQ(hcclCommAicpu->WaitFinishWhileLoop(mainStream,subStreams,tag, 1, param),HCCL_E_TIMEOUT);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, IsTaskExceptionForHccs)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    hcclCommAicpu->dfxExtendInfo_.cqeStatus = dfx::CqeStatus::kCqeException;
    hcclCommAicpu->dfxExtendInfo_.cqeException.sqeType = RT_STARS_SQE_TYPE_SDMA;
    hcclCommAicpu->dfxExtendInfo_.cqeException.errorCode = RT_SDMA_COMPDATAERR;
    EXPECT_EQ(hcclCommAicpu->IsTaskExceptionForHccs(),true);
    hcclCommAicpu->dfxExtendInfo_.cqeException.errorCode = RT_SDMA_COMPERR;
    EXPECT_EQ(hcclCommAicpu->IsTaskExceptionForHccs(),true);
    hcclCommAicpu->dfxExtendInfo_.cqeException.sqeType = RT_STARS_SQE_TYPE_FFTS;
    EXPECT_EQ(hcclCommAicpu->IsTaskExceptionForHccs(),false);
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, ResetSqBuff)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;

    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream mainStream(streamInfo, false);
    Stream orderStream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    mainStream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    orderStream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = mainStream;
    hcclCommAicpu->orderStream_ = orderStream;

    Stream slaveStream(streamInfo, false);
    SqCqeContext sqeCqeCtx1;
    sqeCqeCtx1.sqContext.inited = false;
    slaveStream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx1);
    hcclCommAicpu->slaveStreams_ = std::vector<Stream>{slaveStream};
    EXPECT_EQ(hcclCommAicpu->ResetSqBuff(),HCCL_SUCCESS);
    delete hcclCommAicpu;
}
#if 0
TEST_F(AicpuUnfold_UT, init_aicpucom)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold25";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu* hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu,nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    hcclCommAicpu->isZeroCopy_ = false;
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(LOCAL_NOTIFY_MAX_NUM, hcclCommAicpu->localNotifies_.size());
    EXPECT_EQ(2, hcclCommAicpu->opNotifies_.size());
    EXPECT_EQ(LOCAL_STREAM_MAX_NUM, hcclCommAicpu->slaveStreams_.size());
    EXPECT_TRUE(niclist == hcclCommAicpu->topoInfo_.nicList);
    EXPECT_TRUE(commPlaneRanks == hcclCommAicpu->commPlaneVector_);
    EXPECT_TRUE(serverAndsuperPodToRank == hcclCommAicpu->serverAndsuperPodToRank_);
    EXPECT_TRUE(PairVecTranstoMap<bool>(isUsedRdmaPairVec) == hcclCommAicpu->topoInfo_.isUsedRdmaMap);
    EXPECT_TRUE(PairVecTranstoMap<u32>(pairLinkCounterVec) == hcclCommAicpu->topoInfo_.pairLinkCounter);
    EXPECT_TRUE(bridgeRank == hcclCommAicpu->isBridgeVector_);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    void* tempPtr = malloc(sizeof(OpTilingData) + sizeof(OpTilingDataDes));
    OpTilingData* tilingData = static_cast<OpTilingData*>(tempPtr);
    tilingData->isZeroCopy = false;
    tilingData->opType = HCCL_CMD_ALLREDUCE;
    memset_s(&paramTask, sizeof(HcclOpResParam), 0, sizeof(HcclOpResParam));
    strcpy_s(paramTask.hcomId, 128, hcomId.c_str());
    tilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_ALLREDUCE);
    //默认清空内存
    for (u32 i = 0 ; i < TOP_HIERARCHICAL_CONF_SIZE; i++) {
        tilingData->ahcConfInfo[i] = 0;
    }
    std::string tag = "hcom_aicpu_unfold";
    memcpy_s(tilingData->tag, sizeof(tilingData->tag), tag.c_str(), tag.length() + 1);
    std::string algName = "AllReduceMeshExecutor";
    std::string newTag = tag + "_" + algName;
    memcpy_s(tilingData->newTag, sizeof(tilingData->newTag), newTag.c_str(), newTag.length() + 1);
    memcpy_s(tilingData->algName, sizeof(tilingData->algName), algName.c_str(),
        algName.length() + 1);
    u8* dataDesTempPtr = static_cast<u8*>(tempPtr) +  sizeof(OpTilingData);
    OpTilingDataDes* dataDes = reinterpret_cast<OpTilingDataDes*>(dataDesTempPtr);
    dataDes->count = 1024;
    dataDes->dataType = HCCL_DATA_TYPE_FP16;
    KFCTaskComm kfcTask;

    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(tilingData);
    MOCKER_CPP(&DispatcherAiCpu::LaunchTask).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::CalcResRequest).stubs().will(invoke(CalcResRequestStub));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommAicpu::GetSdmaLinksByRankAndTag).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::GetRdmaLinksByRankAndTag).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&kfcTask));
    auto iter = hcclCommAicpu->resMap_.find(newTag);
    EXPECT_NE(iter, hcclCommAicpu->resMap_.end());
    AlgResourceResponse &algResResponse = iter->second;
    EXPECT_EQ(LOCAL_NOTIFY_MAX_NUM, algResResponse.notifiesMain.size() + algResResponse.notifiesAux.size());
    EXPECT_EQ(LOCAL_STREAM_MAX_NUM, algResResponse.slaveStreams.size());
    EXPECT_EQ(inputMem.ptr(), algResResponse.cclInputMem.ptr());
    EXPECT_EQ(outputMem.ptr(), algResResponse.cclOutputMem.ptr());
    for (auto &levelNSubCommTransport : algResResponse.opTransportResponse) {
        for (auto &singleSubCommTransport : levelNSubCommTransport) {
            EXPECT_EQ(SINGAL_SUB_COMM_NUM, singleSubCommTransport.links.size());
        }
    }
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
    free(tempPtr);
}

TEST_F(AicpuUnfold_UT, st_GetAlgResponseRes)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold26";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string algName = "BatchSendRecv";

    AlgResourceResponse *resourceResponse;
    u32 rankId = 0;

    std::unique_ptr<CollExecutorBase> executor;
    OpParam param;
    std::string newTag = "BatchSendRecv_test";

    std::vector<HcclSendRecvItem> itemVec(1);
    itemVec[0].remoteRank = 1;
    itemVec[0].buf = inputMem.ptr();
    itemVec[0].count = 1024;
    itemVec[0].dataType = HCCL_DATA_TYPE_FP32;
    itemVec[0].sendRecvType = HcclSendRecvType::HCCL_SEND;

    OpParam opParam;
    opParam.tag = "test";
    opParam.BatchSendRecvDataDes.sendRecvItemsPtr = itemVec.data();
    opParam.BatchSendRecvDataDes.itemNum = 1;
    opParam.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    MOCKER_CPP(&HcclCommAicpu::CalcResRequest).stubs().will(invoke(CalcResRequestStub));
    MOCKER_CPP(&HcclCommAicpu::AllocAlgResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::GetRdmaLinksByRankAndTag).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::GetSdmaLinksByRankAndTag).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    ret = hcclCommAicpu->GetAlgResponseRes(newTag, algName, opParam, &paramTask, executor, resourceResponse);
    AlgResourceResponse alg;
    alg.opTransportResponse.resize(OP_COM_NUM);
    for (int opIdx = 0; opIdx < OP_COM_NUM; opIdx++) {
        LevelNSubCommTransport levelNSubCommTransport;
        levelNSubCommTransport.resize(LEVEL_SUB_COMM_NUM);
        for (int levelIdx = 0; levelIdx < LEVEL_SUB_COMM_NUM; levelIdx++) {
            SingleSubCommTransport singleSubCommTransport;
            singleSubCommTransport.transportRequests.resize(SINGAL_SUB_COMM_NUM);
            singleSubCommTransport.links.resize(SINGAL_SUB_COMM_NUM);
            for (int i = 0; i < SINGAL_SUB_COMM_NUM; i++) {
                singleSubCommTransport.transportRequests[i].isValid = true;
                singleSubCommTransport.transportRequests[i].remoteUserRank = rankId;
                singleSubCommTransport.transportRequests[i].inputMemType = TransportMemType::CCL_INPUT;
                singleSubCommTransport.transportRequests[i].outputMemType = TransportMemType::CCL_OUTPUT;
                singleSubCommTransport.links[i] = std::make_shared<Transport>(nullptr);
                rankId++;
            }
            levelNSubCommTransport[levelIdx] = singleSubCommTransport;
        }
        alg.opTransportResponse[opIdx] = (levelNSubCommTransport);
    }
    hcclCommAicpu->resMap_["BatchSendRecv_test"] = alg;
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclCommAicpu->GetAlgResponseRes(newTag, algName, opParam, &paramTask, executor, resourceResponse);
    ret = hcclCommAicpu->GetAlgResponseRes(newTag, algName, opParam, &paramTask, executor, resourceResponse);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    TransportRequest transportRequest_0;
    transportRequest_0.inputMemType = TransportMemType::PARAM_INPUT;
    transportRequest_0.outputMemType = TransportMemType::PARAM_OUTPUT;
    transportRequest_0.isUsedRdma = true;
    TransportRequest transportRequest_1;
    transportRequest_1.inputMemType = TransportMemType::PARAM_INPUT;
    transportRequest_1.outputMemType = TransportMemType::PARAM_OUTPUT;
    transportRequest_1.isUsedRdma = false;
    TransportRequest transportRequest_2;
    transportRequest_2.inputMemType = TransportMemType::CCL_INPUT;
    transportRequest_2.outputMemType = TransportMemType::CCL_OUTPUT;
    transportRequest_2.isUsedRdma = true;
    TransportRequest transportRequest_3;
    transportRequest_3.inputMemType = TransportMemType::CCL_INPUT;
    transportRequest_3.outputMemType = TransportMemType::CCL_OUTPUT;
    transportRequest_3.isUsedRdma = false;
    LINK link;
    u32 extraNotifyNum = 2;
    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_0, &paramTask, link, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_1, &paramTask, link, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_2, &paramTask, link, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = hcclCommAicpu->CreateLink(newTag, transportRequest_3, &paramTask, link, extraNotifyNum, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}

TEST_F(AicpuUnfold_UT, calcResRequest)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    u32 extraNotifyNum = 2;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold";
    std::string tag = "hcom_aicpu_unfold1";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string algName = "AllGatherRingFor91093Executor";

    AlgResourceRequest resourceRequest;
    std::unique_ptr<CollExecutorBase> executor;
    OpParam param;
    ret = hcclCommAicpu->CalcResRequest(algName, param, executor, resourceRequest);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}

TEST_F(AicpuUnfold_UT, orchestrate)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold1";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string algName = "AllGatherRingFor91093Executor";

    MOCKER(AicpuHdcUtils::InitOpExecStatus).stubs().will(returnValue(HCCL_E_INTERNAL));
    AlgResourceResponse algResource;
    std::unique_ptr<CollExecutorBase> executor;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;

    ret = hcclCommAicpu->Orchestrate("newTag", algName, param, executor, algResource, &paramTask);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}

TEST_F(AicpuUnfold_UT, orchestrate_RunAlltoAllVTwoLevelPipeline_a2a)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold2";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string algName = "RunAlltoAllVTwoLevelPipeline";

    MOCKER(AicpuHdcUtils::InitOpExecStatus).stubs().will(returnValue(HCCL_E_INTERNAL));
    AlgResourceResponse algResource;
    std::unique_ptr<CollExecutorBase> executor;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLTOALL;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;

    param.All2AllDataDes.sendCount = 16;
    std::vector<u64> sendCountMatrix(paramTask.rankSize * paramTask.rankSize, 16);
    param.All2AllDataDes.sendCountMatrix = sendCountMatrix.data();

    ret = hcclCommAicpu->Orchestrate("newTag", algName, param, executor, algResource, &paramTask);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }

    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}

TEST_F(AicpuUnfold_UT, orchestrate_RunAlltoAllVTwoLevelPipeline_a2avc)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold3";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string algName = "RunAlltoAllVTwoLevelPipeline";

    MOCKER(AicpuHdcUtils::InitOpExecStatus).stubs().will(returnValue(HCCL_E_INTERNAL));
    AlgResourceResponse algResource;
    std::unique_ptr<CollExecutorBase> executor;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLTOALLVC;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;

    param.All2AllDataDes.sendCount = 16;
    std::vector<u64> sendCountMatrix(paramTask.rankSize * paramTask.rankSize, 16);
    param.All2AllDataDes.sendCountMatrix = sendCountMatrix.data();

    ret = hcclCommAicpu->Orchestrate("newTag", algName, param, executor, algResource, &paramTask);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }

    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}

TEST_F(AicpuUnfold_UT, orchestrate_RunAlltoAllVTwoLevelPipeline_a2av)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    dlog_setlevel(HCCL, DLOG_DEBUG, 1);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold4";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu *hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string algName = "RunAlltoAllVTwoLevelPipeline";

    MOCKER(AicpuHdcUtils::InitOpExecStatus).stubs().will(returnValue(HCCL_E_INTERNAL));
    AlgResourceResponse algResource;
    std::unique_ptr<CollExecutorBase> executor;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_ALLTOALLV;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_INT8;

    u32 rankSize = paramTask.rankSize;
    // 生成 alltoallv 入参
    u64 count = 16;
    std::vector<u64> sendCounts(rankSize * sizeof(u64), 0);
    std::vector<u64> sdispls(rankSize * sizeof(u64), 0);
    std::vector<u64> recvCounts(rankSize * sizeof(u64), 0);
    std::vector<u64> rdispls(rankSize * sizeof(u64), 0);

    HcclDataType sendDataType = HCCL_DATA_TYPE_INT8;
    HcclDataType recvDataType = HCCL_DATA_TYPE_INT8;

    /** 初始化输入输出缓存 */
    for (u32 i = 0; i < rankSize; i++ ) {
        sendCounts[i] = count;
        sdispls[i] = i * count;
        recvCounts[i] = count;
        rdispls[i] = i * count;
    }

    param.All2AllDataDes.sendType = sendDataType;
    param.All2AllDataDes.recvType = recvDataType;
    param.All2AllDataDes.sendCounts = static_cast<void *>(sendCounts.data());
    param.All2AllDataDes.recvCounts = static_cast<void *>(recvCounts.data());
    param.All2AllDataDes.sdispls = static_cast<void *>(recvCounts.data());
    param.All2AllDataDes.rdispls = static_cast<void *>(recvCounts.data());

    std::vector<u64> sendRecvInfos(rankSize * sizeof(u64) * rankSize * 4, 0);
    hcclCommAicpu->SetSendRecvInfoPtr(sendRecvInfos.data());

    ret = hcclCommAicpu->Orchestrate("newTag", algName, param, executor, algResource, &paramTask);
    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }

    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}

TEST_F(AicpuUnfold_UT, test_RunAicpuRpcSrvLaunchV2_dynamicData)
{
    HcclResult ret = HCCL_SUCCESS;
    hccl::HcclCommAicpu* hcclCommAicpu = new hccl::HcclCommAicpu();

    MOCKER_CPP(&HcclCommAicpu::CalcResRequest).stubs().will(invoke(CalcResRequestStub));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().will(returnValue(0));
    MOCKER(AicpuHcclProcess::AicpuGetCommbyGroup).stubs().will(returnValue(hcclCommAicpu));
    MOCKER_CPP(&HcclCommAicpu::AllocAlgResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateNotifyWaitTimeOut).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuShareDataManager::RecordOpInfo).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoMatcher::SetAHCAlgOption).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    u32 itemNum = 1;
    HcclSendRecvItem item;
    u32 dynamicDataSize = sizeof(struct OpTilingBatchSendRecvDataDes) + itemNum * sizeof(HcclSendRecvItem);
    void* tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    OpTilingData* opTilingData = static_cast<OpTilingData*>(tempPtr);

    u8* dynamicDataPtr = static_cast<u8*>(tempPtr) + sizeof(struct OpTilingData);
    struct OpTilingBatchSendRecvDataDes* batchSendRecvDataPtr =
            reinterpret_cast<struct OpTilingBatchSendRecvDataDes*>(dynamicDataPtr);
    batchSendRecvDataPtr->itemNum = itemNum;
    for (u32 i = 0; i < batchSendRecvDataPtr->itemNum; i++) {
        batchSendRecvDataPtr->batchSendRecvItem[i] = item;
    }
    std::string hcomId = "test";
    HcclOpResParam paramTask;
    memset_s(&paramTask, sizeof(HcclOpResParam), 0, sizeof(HcclOpResParam));
    strcpy_s(paramTask.hcomId, 128, hcomId.c_str());
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_SEND_RECV);
    opTilingData->isZeroCopy = 0;
    //默认清空内存
    for (u32 i = 0 ; i < TOP_HIERARCHICAL_CONF_SIZE; i++) {
        opTilingData->ahcConfInfo[i] = 0;
    }
    std::string tag1 = "hcom_aicpu_unfold";
    memcpy_s(opTilingData->tag, sizeof(opTilingData->tag), tag1.c_str(), tag1.size());
    std::string algName1 = "BatchSendrecv";
    tag1 = reinterpret_cast<char *>(opTilingData->tag);
    std::string newTag1 = tag1 + "_" + algName1;
    memcpy_s(opTilingData->newTag, sizeof(opTilingData->newTag), newTag1.c_str(), newTag1.length() + 1);
    memcpy_s(opTilingData->algName, sizeof(opTilingData->algName), algName1.c_str(),
        algName1.length() + 1);
    KFCTaskComm kfcTask1;
    kfcTask1.context = uint64_t(&paramTask);
    kfcTask1.tilingData = uint64_t(opTilingData);
    paramTask.utraceStatusFlag = 1;
    ret = hcclCommAicpu->InitUtraceInfo(&paramTask);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);
    SqCqeContext sqeCqeContext;
    paramTask.localRes.mainStreamParam.sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
    paramTask.localRes.mainStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    paramTask.localRes.streamNum = LOCAL_STREAM_MAX_NUM;
    for (u32 i = 0; i < LOCAL_STREAM_MAX_NUM; i++) {
        paramTask.localRes.streamParam[i].sqCqContextAddr = reinterpret_cast<u64>(&sqeCqeContext);
        paramTask.localRes.streamParam[i].sqCqContextSize = sizeof(SqCqeContext);
    }
    ret = hcclCommAicpu->FlushUtraceInfo();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&kfcTask1));

    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_ALLTOALL);
    kfcTask1.tilingData = uint64_t(opTilingData);
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&kfcTask1));

    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_ALLTOALLV);
    kfcTask1.tilingData = uint64_t(opTilingData);
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&kfcTask1));

    delete(hcclCommAicpu);
    free(tempPtr);
    aicpuCustomDev.free();
}


TEST_F(AicpuUnfold_UT, test_RunAicpuRpcSrvLaunchV2_ReportSdmaErr)
{
    HcclResult ret = HCCL_SUCCESS;
    hccl::HcclCommAicpu* hcclCommAicpu = new hccl::HcclCommAicpu();

    MOCKER_CPP(&HcclCommAicpu::CalcResRequest).stubs().will(invoke(CalcResRequestStub));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().will(returnValue(0));
    MOCKER(AicpuHcclProcess::AicpuGetCommbyGroup).stubs().will(returnValue(hcclCommAicpu));
    MOCKER_CPP(&HcclCommAicpu::AllocAlgResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateNotifyWaitTimeOut).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuShareDataManager::RecordOpInfo).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoMatcher::SetAHCAlgOption).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    const u32 rankSize = 8;
    u64 vDateLen = 2 * rankSize * sizeof(u64);
    u32 dynamicDataSize = sizeof(struct OpTilingVDataDes) + vDateLen;
    void* tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    OpTilingData* opTilingData = static_cast<OpTilingData*>(tempPtr);

    u8* dynamicDataPtr = static_cast<u8*>(tempPtr) + sizeof(struct OpTilingData);
    struct OpTilingVDataDes* vDataPtr = reinterpret_cast<struct OpTilingVDataDes*>(dynamicDataPtr);
    vDataPtr->vDataLen = vDateLen;
    vDataPtr->dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    for (u32 i = 0; i < rankSize; i++) {
        vDataPtr->vData[i] = i; // counts
        vDataPtr->vData[i + rankSize] = i;  // displs
    }
    std::string hcomId = "test";
    HcclOpResParam paramTask;
    memset_s(&paramTask, sizeof(HcclOpResParam), 0, sizeof(HcclOpResParam));
    strcpy_s(paramTask.hcomId, 128, hcomId.c_str());
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_ALLGATHER_V);
    opTilingData->srcRank = 0;
    opTilingData->isZeroCopy = 0;
    //默认清空内存
    for (u32 i = 0 ; i < TOP_HIERARCHICAL_CONF_SIZE; i++) {
        opTilingData->ahcConfInfo[i] = 0;
    }
    std::string tag = "hcom_agv_aicpu_unfold";
    memcpy_s(opTilingData->tag, sizeof(opTilingData->tag), tag.c_str(), tag.size() + 1);
    std::string algName = "AllGatherV";
    tag = reinterpret_cast<char *>(opTilingData->tag);
    std::string newTag = tag + "_" + algName;
    memcpy_s(opTilingData->newTag, sizeof(opTilingData->newTag), newTag.c_str(), newTag.length() + 1);
    memcpy_s(opTilingData->algName, sizeof(opTilingData->algName), algName.c_str(),
        algName.length() + 1);
    KFCTaskComm kfcTask;
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(opTilingData);

    // 打桩故障
    DfxExtendInfo dfxInfo;
    dfxInfo.cqeException.sqeType = RT_STARS_SQE_TYPE_SDMA;
    MOCKER_CPP(&HcclCommAicpu::GetDfxExtendInfo).stubs().will(returnValue(&dfxInfo));
    EXPECT_EQ(1001, RunAicpuRpcSrvLaunchV2(&kfcTask));

    dfxInfo.pollStatus = PollStatus::kStopAsException;
    EXPECT_EQ(1001, RunAicpuRpcSrvLaunchV2(&kfcTask));

    delete(hcclCommAicpu);
    free(tempPtr);
}

TEST_F(AicpuUnfold_UT, test_aicpuHcclProcess_HandleOneSideService)
{
    HcclResult ret;
    constexpr u32 itemNum = 2;
    std::string commId = "one_side_st_test";
    u32 dynamicDataSize = sizeof(OpTilingOneSideCommDataDes) + itemNum * sizeof(HcclOneSideOpDescParam);
    void *tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    uint8_t *tilingPtr = static_cast<uint8_t *>(tempPtr);
    OpTilingData* opTilingData = reinterpret_cast<OpTilingData*>(tilingPtr);
    memcpy(opTilingData->tag, commId.data(), commId.length() + 1);
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_PUT);
    auto *oneSideCommDataDes = reinterpret_cast<OpTilingOneSideCommDataDes*>(tilingPtr + sizeof(OpTilingData));
    oneSideCommDataDes->commResParaSize = sizeof(HcclOneSideCommResParam);
    DeviceMem commResParaDevice = DeviceMem::alloc(sizeof(HcclOneSideCommResParam));
    oneSideCommDataDes->commResParaAddr = reinterpret_cast<u64>(commResParaDevice.ptr());
    auto *oneSideCommResParam = reinterpret_cast<HcclOneSideCommResParam *>(commResParaDevice.ptr());
    oneSideCommResParam->execStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    DeviceMem execStreamContext = DeviceMem::alloc(sizeof(SqCqeContext));
    oneSideCommResParam->execStreamParam.sqCqContextAddr =  reinterpret_cast<u64>(execStreamContext.ptr());
    oneSideCommDataDes->transportDataSize = sizeof(TransportDeviceNormalData);
    DeviceMem transportDataDevice = DeviceMem::alloc(sizeof(TransportDeviceNormalData));
    oneSideCommDataDes->transportDataAddr = reinterpret_cast<u64>(transportDataDevice.ptr());
    auto *transportData = reinterpret_cast<TransportDeviceNormalData *>(transportDataDevice.ptr());
    for (u32 i = 0; i < AICPU_OP_NOTIFY_MAX_NUM; i++) {
        oneSideCommResParam->aicpuOpNotify[i].resId = i;
    }
    struct ibv_qp qp;
    qp.state = ibv_qp_state::IBV_QPS_RTR;
	qp.qp_type = ibv_qp_type::IBV_QPT_RC;
    transportData->qpInfo.qpPtr = reinterpret_cast<u64>(&qp);
    oneSideCommDataDes->linkTimeout = 10;
    oneSideCommDataDes->linkType = static_cast<u8>(LinkType::LINK_ROCE);
    oneSideCommDataDes->finalize = false;
    oneSideCommDataDes->descNum = itemNum;
    oneSideCommDataDes->descDataLen = oneSideCommDataDes->descNum * sizeof(HcclOneSideOpDescParam);
    auto *item = reinterpret_cast<HcclOneSideOpDescParam*>(tilingPtr + sizeof(OpTilingData) +
        sizeof(OpTilingOneSideCommDataDes));
    for (u32 i = 0; i < itemNum; i++) {
        item[i].localAddr = 1;
        item[i].remoteAddr = 2;
        item[i].lkey = 3;
        item[i].rkey = 4;
        item[i].count = 1;
        item[i].dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    }

    MOCKER(hrtDrvGetLocalDevIDByHostDevID).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Init, HcclResult (LocalNotify::*)(const HcclSignalInfo &, const NotifyLoadType));
    MOCKER(hrtHalGetDeviceType)
        .stubs()
        .with(any(), outBound(DevType::DEV_TYPE_910_93))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(RegisterLoadTaskCallBack).stubs().will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&LocalNotify::Wait,
        HcclResult (*)(Stream &, HcclDispatcher, const std::shared_ptr<LocalNotify> &, s32, u32))
        .stubs().will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DispatcherAiCpu::LaunchTask).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DlHnsFunction::DlHnsFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));

    auto postSend = DlHnsFunction::GetInstance().dlHnsIbvExpPostSend;
    DlHnsFunction::GetInstance().dlHnsIbvExpPostSend = [](
        struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr, struct WrExpRsp *expRsp) { return 0; };

    KFCTaskComm args;
    args.tilingData = reinterpret_cast<u64>(opTilingData);
    args.context = 0;
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&args));
    ASSERT_FALSE(HcclOneSideServiceAicpu::services_.empty());

    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_GET);
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // HCCL_E_AGAIN <- HrtHnsIbvExpPostSend: ENOENT/EAGAIN/ENOMEM
    DlHnsFunction::GetInstance().dlHnsIbvExpPostSend = [](
        struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr, struct WrExpRsp *expRsp) {
            static u32 count = 0;
            if (++count < 10) {
                return -EAGAIN;
            }
            if (++count < 20) {
                return -ENOMEM;
            }
            if (++count < 30) {
                return -ENOENT;
            }
            return 0;
        };
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);   // retry success

    auto service = HcclOneSideServiceAicpu::services_.begin()->second;
    service->linkTimeout_ = INVALID_UINT;
    service->rdmaLinks_.clear();    // recreate transport
    DlHnsFunction::GetInstance().dlHnsIbvExpPostSend = [](
        struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr, struct WrExpRsp *expRsp) {
            static u32 count = 0;
            if (++count < 10) {
                return -EAGAIN;
            }
            return 0;
        };
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_E_AGAIN);   // no retry with invalid timeout

    service->linkTimeout_ = 1;      // 1s
    service->rdmaLinks_.clear();    // recreate transport
    DlHnsFunction::GetInstance().dlHnsIbvExpPostSend = [](
        struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr, struct WrExpRsp *expRsp) {
            static u32 count = 0;
            if (++count < 1000) {
                return -EAGAIN;
            }
            if (++count < 2000) {
                return -ENOMEM;
            }
            if (++count < 3000) {
                return -ENOENT;
            }
            return 0;
        };
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_E_AGAIN);   // retry timeout

    DlHnsFunction::GetInstance().dlHnsIbvExpPostSend = [](
        struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **badWr, struct WrExpRsp *expRsp) {
            return -EPERM;
        };
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_E_NETWORK);

    oneSideCommDataDes->finalize = true;
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(HcclOneSideServiceAicpu::services_.empty());

    DlHnsFunction::GetInstance().dlHnsIbvExpPostSend = postSend;
    free(tempPtr);
}

TEST_F(AicpuUnfold_UT, test_hcclCommAicpu_alltoall)
{
    hccl::HcclCommAicpu* hcclCommAicpu = new hccl::HcclCommAicpu();
    hcclCommAicpu->topoInfo_.userRankSize = 2;
    OpParam opParam;
    opParam.All2AllDataDes.sendType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.All2AllDataDes.recvType = HcclDataType::HCCL_DATA_TYPE_FP32;
    void* tmpMem = malloc(1024);
    opParam.inputPtr = tmpMem;
    opParam.outputPtr = tmpMem;
    std::vector<u64> sendCountMatrix (4, 16);
    opParam.All2AllDataDes.sendCountMatrix = static_cast<void *>(sendCountMatrix.data());
    AlgResourceResponse algResource;
    HcclResult ret1 = hcclCommAicpu->SetAlltoAllInputAndOutPutMem(opParam, algResource);
    HcclResult ret2 = hcclCommAicpu->GetAlltoAllvcSendRecvInfo(opParam.All2AllDataDes.sendCountMatrix, opParam.All2AllDataDes.sendType,
                opParam.All2AllDataDes.recvType);
    EXPECT_EQ(HCCL_SUCCESS, ret1);
    EXPECT_EQ(HCCL_SUCCESS, ret2);

    delete(hcclCommAicpu);
    free(tmpMem);
}
#endif
struct TestTilingData{
    uint32_t version;
    uint32_t commCnt;
    Mc2ServerCfg serverCfg;
    Mc2HcommCfg cfg1;
    Mc2HcommCfg cfg2;
};

struct ArgsInput {
    uint64_t inputDesc;
    void *context1;
    void *context2;
    void *workspace;
    void *tilingData;
};

class ExecutorTracer_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "ExecutorTracer_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "ExecutorTracer_ST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "ExecutorTracer_ST Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "ExecutorTracer_ST Test TearDown" << std::endl;
    }
};

CqeStatus CqReportRecvStub(const CqeQueryInput &cqeQueryInput, rtLogicCqReport_t &cqeException) {
    cqeException.sqeType = cqeException.sqeType = RT_STARS_SQE_TYPE_SDMA;
    cqeException.errorCode == RT_SDMA_COMPDATAERR;
    return dfx::CqeStatus::kCqeException;
}
#if 1
TEST_F(ExecutorTracer_ST, HandleCqeException)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    Stream stream;
    MOCKER(&CqReportRecv).stubs().will(returnValue(dfx::CqeStatus::kCqeException));
    MOCKER(&drvQueryProcessHostPid).stubs().will(invoke(drvQueryProcessHostPidStub));
    MOCKER(&getpid).stubs().will(returnValue(0));
    hcclCommAicpu->HandleCqeException(stream, false);
    GlobalMockObject::verify();

    MOCKER(&CqReportRecv).stubs().will(invoke(CqReportRecvStub));
    MOCKER(&drvQueryProcessHostPid).stubs().will(invoke(drvQueryProcessHostPidStub));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().will(returnValue(HCCL_E_DRV));
    MOCKER(&getpid).stubs().will(returnValue(0));
    hcclCommAicpu->HandleCqeException(stream, false);

    delete hcclCommAicpu;
}
#endif

TEST_F(ExecutorTracer_ST, ReportCqeExceptionSdma)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());

    MOCKER(&drvQueryProcessHostPid).stubs().will(invoke(drvQueryProcessHostPidStub));
    MOCKER(&getpid).stubs().will(returnValue(0));
    MOCKER_CPP(&LocalNotify::GetNotifyData).stubs().will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->SendTaskExceptionByMBox(RT_SDMA_COMPDATAERR);
    hcclCommAicpu->SendTaskExceptionByMBox(RT_SDMA_COMPERR);
    hcclCommAicpu->SendTaskExceptionByMBox(RT_SDMA_DATAERR);

    hcclCommAicpu->SendTaskExceptionByMBox(0);
    hcclCommAicpu->userStreamId_ = -1;
    hcclCommAicpu->SendTaskExceptionByMBox(0);

    MOCKER(&halEschedSubmitEvent).stubs().will(returnValue(1));
    hcclCommAicpu->SendTaskExceptionByMBox(0);
    GlobalMockObject::verify();

    MOCKER(&drvQueryProcessHostPid).stubs().will(returnValue(1));
    MOCKER(&getpid).stubs().will(returnValue(0));
    MOCKER_CPP(&LocalNotify::GetNotifyData).stubs().will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->SendTaskExceptionByMBox(0);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, InitSendRecvOpId_HcclUpdateOpIndex)
{
    OpParam param;
    HcclOpIdentifier Identify;
    hccl::HcclCommAicpu hcclCommAicpu;

    param.opType = HcclCMDType::HCCL_CMD_SEND;
    hcclCommAicpu.InitSendRecvOpId(param, Identify);
    param.opType = HcclCMDType::HCCL_CMD_RECEIVE;
    hcclCommAicpu.InitSendRecvOpId(param, Identify);
}

TEST_F(AicpuUnfold_UT, PrintTaskExceptionByTaskId)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    struct rtLogicCqReport_t rtLogicCqReport;
    // 测试初始化是否成功
    hccl::Stream hcclStream(stream);
    u8 sqeType = 0;
    u16 taskId = 1;
    EXPECT_EQ(hcclCommAicpu->PrintTaskExceptionByTaskId(sqeType, taskId, hcclStream, 1), HCCL_SUCCESS);
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, PrintTaskException)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = stream;
    MOCKER(QuerySqStatus).stubs().with(any(),any(),outBound(sqHead),outBound(sqTail)).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->commOpenStatus = true;
    MOCKER_CPP(&HcclTraceInfo::Flush).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->PrintTaskExceptionAllStreams();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, ut_PrintTaskException_ExecOp_fail)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = stream;
    hcclCommAicpu->retryEnable_ = true;
    hcclCommAicpu->printTaskExceptionForErr_ = true;
    hcclCommAicpu->identifier_ = "1";
    MOCKER(QuerySqStatusByType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    hccl::HcclCommAicpu *hcclCommAicpu2 = new hccl::HcclCommAicpu;
    hcclCommAicpu2->mainStream_ = stream;
    hcclCommAicpu2->retryEnable_ = true;
    hcclCommAicpu2->printTaskExceptionForErr_ = true;
    hcclCommAicpu2->identifier_ = "2";

    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> aicpuCommInfo;
    aicpuCommInfo.push_back({hcclCommAicpu->identifier_, hcclCommAicpu});
    aicpuCommInfo.push_back({hcclCommAicpu2->identifier_, hcclCommAicpu2});

    MOCKER_CPP(&HcclCommAicpu::GetAlgResponseRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateOpRingBufferIdx).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    MOCKER(&AicpuHcclProcess::AicpuGetCommAll).stubs().with(outBound(aicpuCommInfo)).will(returnValue(HCCL_SUCCESS));

    std::string newTag = "tag_test_taskException";
    std::string algName = "algName_test_taskException";
    OpParam opParam;
    HcclOpResParam commParam;
    commParam.localUsrRankId = 0;
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);

    hcclCommAicpu->isZeroCopy_ = true;
    MOCKER_CPP(&HcclCommAicpu::PrepareZeroCopyExchanger).stubs().with(any()).will(returnValue(HCCL_E_PARA));
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);

    hcclCommAicpu->isZeroCopy_ = true;
    MOCKER_CPP(&AicpuZeroCopyExchanger::BatchSetLocalAddrToRemote).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuZeroCopyExchanger::GetRemoteAddr).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);
    delete hcclCommAicpu;
    delete hcclCommAicpu2;
}

TEST_F(AicpuUnfold_UT, ut_HcclOneSideServiceAicpuProcess_When_SdmaAicpuUnflod_Expect_Success)
{
    HcclResult ret;
    constexpr u32 itemNum = 2;
    std::string commId = "one_side_ut_test";
    u32 dynamicDataSize = sizeof(OpTilingOneSideCommDataDes) + itemNum * sizeof(HcclOneSideOpDescParam);
    void *tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    uint8_t *tilingPtr = static_cast<uint8_t *>(tempPtr);
    OpTilingData* opTilingData = reinterpret_cast<OpTilingData*>(tilingPtr);
    memcpy(opTilingData->tag, commId.data(), commId.length() + 1);
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_GET);
    auto *oneSideCommDataDes = reinterpret_cast<OpTilingOneSideCommDataDes*>(tilingPtr + sizeof(OpTilingData));
    oneSideCommDataDes->commResParaSize = sizeof(HcclOneSideCommResParam);
    DeviceMem commResParaDevice = DeviceMem::alloc(sizeof(HcclOneSideCommResParam));
    oneSideCommDataDes->commResParaAddr = reinterpret_cast<u64>(commResParaDevice.ptr());
    auto *oneSideCommResParam = reinterpret_cast<HcclOneSideCommResParam *>(commResParaDevice.ptr());
    for (u32 i = 0; i < AICPU_OP_NOTIFY_MAX_NUM; i++) {
        oneSideCommResParam->aicpuOpNotify[i].resId = i;
    }
    oneSideCommResParam->execStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    DeviceMem execStreamContext = DeviceMem::alloc(sizeof(SqCqeContext));
    oneSideCommResParam->execStreamParam.sqCqContextAddr =  reinterpret_cast<u64>(execStreamContext.ptr());
    oneSideCommDataDes->transportDataSize = sizeof(TransportDeviceNormalData);
    DeviceMem transportDataDevice = DeviceMem::alloc(sizeof(TransportDeviceNormalData));
    oneSideCommDataDes->transportDataAddr = reinterpret_cast<u64>(transportDataDevice.ptr());
    auto *transportData = reinterpret_cast<TransportDeviceNormalData *>(transportDataDevice.ptr());
    struct ibv_qp qp;
    qp.state = ibv_qp_state::IBV_QPS_RTR;
	qp.qp_type = ibv_qp_type::IBV_QPT_RC;
    transportData->qpInfo.qpPtr = reinterpret_cast<u64>(&qp);
    oneSideCommDataDes->linkTimeout = 10;
    oneSideCommDataDes->linkType = static_cast<u8>(LinkType::LINK_HCCS);
    oneSideCommDataDes->finalize = false;
    oneSideCommDataDes->descNum = itemNum;
    oneSideCommDataDes->descDataLen = oneSideCommDataDes->descNum * sizeof(HcclOneSideOpDescParam);
    auto *item = reinterpret_cast<HcclOneSideOpDescParam*>(tilingPtr + sizeof(OpTilingData) +
        sizeof(OpTilingOneSideCommDataDes));
    for (u32 i = 0; i < itemNum; i++) {
        item[i].localAddr = 1;
        item[i].remoteAddr = 2;
        item[i].lkey = 3;
        item[i].rkey = 4;
        item[i].count = 1;
        item[i].dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    }
 
    MOCKER(hrtDrvGetLocalDevIDByHostDevID).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtHalGetDeviceType)
        .stubs()
        .with(any(), outBound(DevType::DEV_TYPE_910_93))
        .will(returnValue(HCCL_SUCCESS));
 
    MOCKER(RegisterLoadTaskCallBack).stubs().will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&LocalNotify::Init, HcclResult (LocalNotify::*)(const HcclSignalInfo &, const NotifyLoadType))
        .stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalNotify::Post,
        HcclResult (*)(Stream &, HcclDispatcher, const std::shared_ptr<LocalNotify> &, s32))
        .stubs().will(returnValue(HCCL_SUCCESS));
 
    MOCKER(HcclD2DMemcpyAsync).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DispatcherAiCpu::LaunchTask).stubs().will(returnValue(HCCL_SUCCESS));
 
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_PUT);
    ret = HcclOneSideServiceAicpu::Process(opTilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    oneSideCommDataDes->finalize = true;
    ret = AicpuHcclProcess::HandleOneSideService(opTilingData);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(HcclOneSideServiceAicpu::services_.empty());
 
    free(tempPtr);
}
 
TEST_F(AicpuUnfold_UT, ut_HcclOneSideServiceAicpuDoProcess_When_linkTypeNotSupport_Expect_Failed)
{
    HcclResult ret;
    constexpr u32 itemNum = 2;
    std::string commId = "one_side_ut_test";
    u32 dynamicDataSize = sizeof(OpTilingOneSideCommDataDes) + itemNum * sizeof(HcclOneSideOpDescParam);
    void *tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    uint8_t *tilingPtr = static_cast<uint8_t *>(tempPtr);
    OpTilingData* opTilingData = reinterpret_cast<OpTilingData*>(tilingPtr);
    memcpy(opTilingData->tag, commId.data(), commId.length() + 1);
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_GET);
    auto *oneSideCommDataDes = reinterpret_cast<OpTilingOneSideCommDataDes*>(tilingPtr + sizeof(OpTilingData));
    oneSideCommDataDes->commResParaSize = sizeof(HcclOneSideCommResParam);
    DeviceMem commResParaDevice = DeviceMem::alloc(sizeof(HcclOneSideCommResParam));
    oneSideCommDataDes->commResParaAddr = reinterpret_cast<u64>(commResParaDevice.ptr());
    auto *oneSideCommResParam = reinterpret_cast<HcclOneSideCommResParam *>(commResParaDevice.ptr());
    for (u32 i = 0; i < AICPU_OP_NOTIFY_MAX_NUM; i++) {
        oneSideCommResParam->aicpuOpNotify[i].resId = i;
    }
    oneSideCommResParam->execStreamParam.sqCqContextSize = sizeof(SqCqeContext);
    DeviceMem execStreamContext = DeviceMem::alloc(sizeof(SqCqeContext));
    oneSideCommResParam->execStreamParam.sqCqContextAddr =  reinterpret_cast<u64>(execStreamContext.ptr());
    oneSideCommDataDes->transportDataSize = sizeof(TransportDeviceNormalData);
    DeviceMem transportDataDevice = DeviceMem::alloc(sizeof(TransportDeviceNormalData));
    oneSideCommDataDes->transportDataAddr = reinterpret_cast<u64>(transportDataDevice.ptr());
    auto *transportData = reinterpret_cast<TransportDeviceNormalData *>(transportDataDevice.ptr());
    struct ibv_qp qp;
    qp.state = ibv_qp_state::IBV_QPS_RTR;
	qp.qp_type = ibv_qp_type::IBV_QPT_RC;
    transportData->qpInfo.qpPtr = reinterpret_cast<u64>(&qp);
    oneSideCommDataDes->linkTimeout = 10;
    oneSideCommDataDes->linkType = static_cast<u8>(LinkType::LINK_PCIE);
    oneSideCommDataDes->finalize = false;
 
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_BATCH_PUT);
 
    HcclOneSideServiceAicpu service;
    ret = service.DoProcess(opTilingData->tag, opTilingData);
    EXPECT_EQ(ret, HCCL_E_PARA);
 
    free(tempPtr);
}
 
TEST_F(AicpuUnfold_UT, ut_HcclOneSideServiceAicpuDoSdmaProcess_When_D2dCopyFailed_Expect_Failed)
{
    HcclOneSideOpDescParam opDescs;
    opDescs.localAddr = 1;
    opDescs.remoteAddr = 1;
    opDescs.lkey = 3;
    opDescs.rkey = 4;
    opDescs.count = 1;
    opDescs.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
 
    MOCKER(HcclD2DMemcpyAsync).stubs().will(returnValue(HCCL_E_PTR));
    HcclOneSideServiceAicpu service;
    HcclResult ret = service.DoSdmaProcess(HcclCMDType::HCCL_CMD_BATCH_PUT, 1, nullptr, &opDescs, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
 
    ret = service.DoSdmaProcess(HcclCMDType::HCCL_CMD_BATCH_GET, 1, nullptr, &opDescs, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(AicpuUnfold_UT, ut_aicpu_PrepareZeroCopyExchanger)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = stream;
    hcclCommAicpu->retryEnable_ = true;
    hcclCommAicpu->printTaskExceptionForErr_ = true;
    hcclCommAicpu->identifier_ = "1";
    MOCKER(QuerySqStatusByType)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    hccl::HcclCommAicpu *hcclCommAicpu2 = new hccl::HcclCommAicpu;
    hcclCommAicpu2->mainStream_ = stream;
    hcclCommAicpu2->retryEnable_ = true;
    hcclCommAicpu2->printTaskExceptionForErr_ = true;
    hcclCommAicpu2->identifier_ = "2";

    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> aicpuCommInfo;
    aicpuCommInfo.push_back({hcclCommAicpu->identifier_, hcclCommAicpu});
    aicpuCommInfo.push_back({hcclCommAicpu2->identifier_, hcclCommAicpu2});

    MOCKER_CPP(&HcclCommAicpu::GetAlgResponseRes).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateOpRingBufferIdx).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    MOCKER(&AicpuHcclProcess::AicpuGetCommAll).stubs().with(outBound(aicpuCommInfo)).will(returnValue(HCCL_SUCCESS));

    std::string newTag = "tag_test_taskException";
    std::string algName = "algName_test_taskException";
    OpParam opParam;
    HcclOpResParam commParam;
    commParam.localUsrRankId = 0;
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);

    hcclCommAicpu->isZeroCopy_ = true;
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);

    hcclCommAicpu->isZeroCopy_ = true;
    MOCKER_CPP(&AicpuZeroCopyExchanger::BatchSetLocalAddrToRemote).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuZeroCopyExchanger::GetRemoteAddr).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    hcclCommAicpu->ExecOp(newTag, algName, opParam, &commParam);
    delete hcclCommAicpu;
    delete hcclCommAicpu2;
}
#if 0 // 栈溢出
TEST_F(AicpuUnfold_UT, aicpu_prof_test_init)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    MOCKER(AdprofCheckFeatureIsOn).stubs().will(returnValue(1));
    bool isL1On = dfx::ProfilingManager::IsProfL1On();
    bool isL0On = dfx::ProfilingManager::IsProfL0On();
    EXPECT_EQ(isL1On, true);
    EXPECT_EQ(isL0On, true);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold13";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu* hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu,nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    OpParam param;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    ret = hcclCommAicpu->CombineReportOpInfo(param, true, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}
#endif
TEST_F(AicpuUnfold_UT, st_ReportFlagTask_test)
{
    hccl::Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    uint16_t taskId = 1;
    uint16_t type = 2;
    auto ret = dfx::ProfilingManager::ReportMainStreamTask(stream, taskId, type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MsporfAicpuFlipTask flipTaskInfo;
    s32 streamId;
    uint32_t flipNum;
    ret = dfx::ProfilingManager::ReportFilpTask(streamId, taskId, flipNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(AdprofCheckFeatureIsOn).stubs().will(returnValue(1));
    bool isL0On = dfx::ProfilingManager::IsProfL0On();
    EXPECT_EQ(isL0On, true);
    ret = dfx::ProfilingManager::ReportMainStreamTask(stream, taskId, type);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::ProfilingManager::ReportFilpTask(streamId, taskId, flipNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
class CollExecutorMock2 : public CollExecutorBase {
public:
    CollExecutorMock2(std::unique_ptr<TopoMatcher> &topoMatcher):CollExecutorBase(nullptr, topoMatcher) {};
    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algRes) {return HCCL_SUCCESS;}
    HcclResult CalcResRequest(const OpParam& param, AlgResourceRequest &resourceRequest) {return HCCL_SUCCESS;}
};

TEST_F(AicpuUnfold_UT, HcclOpExecFsmLaunchProcess_bsr_retry)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER(QuerySqStatusByType).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::NotifyWait).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::NotifyPost).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(LaunchTask).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::LaunchSlaveStreamTask).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::CheckOpExecStatus).stubs().with(any()).will(returnValue(HCCL_E_SUSPENDING));

    std::string algName;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    param.tag = "hccl_llt_batch_send_recv_test";
    param.BatchSendRecvDataDes.curIterNum = 0;

    hcclCommAicpu->retryEnable_ = true;

    HcclSendRecvItem sendItem = {HcclSendRecvType::HCCL_SEND, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    HcclSendRecvItem recvItem = {HcclSendRecvType::HCCL_RECV, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    std::vector<HcclSendRecvItem *> temp;
    temp.push_back(&sendItem);
    temp.push_back(&recvItem);
    hcclCommAicpu->bsrSendRecvPairs_.push_back(temp);

    AlgResourceResponse algResource;
    HcclComStreamInfo comStreamInfo = {0};
    algResource.slaveStreams.emplace_back(Stream(comStreamInfo));
    algResource.slaveStreams.emplace_back(Stream(comStreamInfo));

    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;

    std::unique_ptr<TopoMatcher> topoMatcher;
    std::unique_ptr<CollExecutorBase> executor(new CollExecutorMock2(topoMatcher));
    u32 retryCnt = 0;
    ASSERT_EQ(hcclCommAicpu->HcclOpExecFsmLaunchProcess(algName, param, executor, algResource, state, errorCode, beginSqePos,
                                                        endSqePos, retryCnt),
              HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}


TEST_F(AicpuUnfold_UT, HcclOpExecFsmStoppedProcess_bsr_retry)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(false));
    HcclOpExecFSM state;
    KfcError errorCode = KfcError::kNone;
    uint32_t retryCnt;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    param.tag = "hccl_llt_batch_send_recv_test";
    param.BatchSendRecvDataDes.curIterNum = 0;

    hcclCommAicpu->retryEnable_ = true;

    HcclSendRecvItem sendItem = {HcclSendRecvType::HCCL_SEND, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    HcclSendRecvItem recvItem = {HcclSendRecvType::HCCL_RECV, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    std::vector<HcclSendRecvItem *> temp;
    temp.push_back(&sendItem);
    temp.push_back(&recvItem);
    hcclCommAicpu->bsrSendRecvPairs_.push_back(temp);

    AlgResourceResponse algResource;
    HcclComStreamInfo comStreamInfo = {0};
    algResource.slaveStreams.emplace_back(Stream(comStreamInfo));
    algResource.slaveStreams.emplace_back(Stream(comStreamInfo));

    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;
    std::string algName = "test";
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER(&QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub1));
    beginSqePos = BEGINSQEPOS;
    endSqePos = ENDSQEPOS;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER(&QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub2));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExec);
    GlobalMockObject::verify();

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER(&QuerySqStatusByType).stubs().with(any()).will(invoke(QuerySqStatusByTypeStub3));
    MOCKER_CPP(&HcclCommAicpu::IsTaskExceptionForHccs).stubs().with(any()).will(returnValue(true));
    MOCKER(&AicpuHdcUtils::SetOpExecStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    hcclCommAicpu->bsrSendOpBeginSqePos_ = 10;
    hcclCommAicpu->bsrRecvOpBeginSqePos_ = 10;
    hcclCommAicpu->bsrSendOpEndSqePos_ = 100;
    hcclCommAicpu->bsrRecvOpEndSqePos_ = 100;

    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_WAIT_RETRY);
    EXPECT_EQ(errorCode, KfcError::kNone);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}


TEST_F(AicpuUnfold_UT, HcclOpExecFsmWaitRetryProcess_bsr_retry)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    HcclOpExecFSM state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT;
    KfcError errorCode = KfcError::kNone;
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    param.tag = "hccl_llt_batch_send_recv_test";
    param.BatchSendRecvDataDes.curIterNum = 0;
    KfcCommand kfcCommand = KfcCommand::kNone;

    hcclCommAicpu->retryEnable_ = true;

    HcclSendRecvItem sendItem = {HcclSendRecvType::HCCL_SEND, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    HcclSendRecvItem recvItem = {HcclSendRecvType::HCCL_RECV, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    std::vector<HcclSendRecvItem *> temp;
    temp.push_back(&sendItem);
    temp.push_back(&recvItem);
    hcclCommAicpu->bsrSendRecvPairs_.push_back(temp);

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub4));
    MOCKER_CPP(&Stream::ClearLocalBuff).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateSqStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    hcclCommAicpu->bsrRetryOp_ = HCCL_SEND;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_RETRY);
    EXPECT_EQ(errorCode, KfcError::kNone);

    hcclCommAicpu->bsrRetryOp_ = HCCL_RECV;
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_RETRY);
    EXPECT_EQ(errorCode, KfcError::kNone);
    GlobalMockObject::verify();

    MOCKER_CPP(&Stream::ClearLocalBuff).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateSqStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub1));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExit);
    GlobalMockObject::verify();

    state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT;
    errorCode = KfcError::kNone;
    MOCKER_CPP(&Stream::ClearLocalBuff).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateSqStatus).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, kfcCommand), HCCL_SUCCESS);
    EXPECT_EQ(errorCode, KfcError::kNone);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_INIT);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExecFsmRetryProcess_bsr_retry)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    std::string algName = "test";
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    param.tag = "test_BSR";
    param.BatchSendRecvDataDes.curIterNum = 0;

    hcclCommAicpu->retryEnable_ = true;

    HcclSendRecvItem sendItem = {HcclSendRecvType::HCCL_SEND, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    HcclSendRecvItem recvItem = {HcclSendRecvType::HCCL_RECV, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    std::vector<HcclSendRecvItem *> temp;
    temp.push_back(&sendItem);
    temp.push_back(&recvItem);
    hcclCommAicpu->bsrSendRecvPairs_.push_back(temp);


    AlgResourceResponse algResource;
    HcclComStreamInfo comStreamInfo = {0};
    algResource.slaveStreams.emplace_back(Stream(comStreamInfo));
    algResource.slaveStreams.emplace_back(Stream(comStreamInfo));

    HcclOpExecFSM state;
    KfcError errorCode;
    uint32_t retryCnt;
    uint32_t beginSqePos;
    uint32_t endSqePos;
    std::unique_ptr<CollExecutorBase> executor;
    MOCKER_CPP(&HcclCommAicpu::RetryOrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_E_DRV));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_E_DRV);
    GlobalMockObject::verify();


    MOCKER_CPP(&HDCommunicate::Put).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateOpExecStatus, HcclResult (HcclCommAicpu::*)(HcclOpExecFSM &fsmState, KfcStatus state, KfcError &errorCode, uint32_t retryCnt)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RetryOrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);

    MOCKER_CPP(&HcclCommAicpu::CheckOpExecStatus).stubs().with(any()).will(returnValue(HCCL_E_SUSPENDING));

    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);

    hcclCommAicpu->bsrSendOpId_.tag[0] = 0;
    hcclCommAicpu->bsrSendOpId_.srcRank = 0;
    hcclCommAicpu->bsrSendOpId_.detRank = 1;
    hcclCommAicpu->bsrRecvOpId_.tag[0] = 0;
    hcclCommAicpu->bsrRecvOpId_.srcRank = 7;
    hcclCommAicpu->bsrRecvOpId_.detRank = 0;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlTargetOp).stubs().with(any()).will(invoke(GetOpExecCtrlTargetOpStub));
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub5));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);
    GlobalMockObject::verify();


    MOCKER_CPP(&HDCommunicate::Put).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateOpExecStatus, HcclResult (HcclCommAicpu::*)(HcclOpExecFSM &fsmState, KfcStatus state, KfcError &errorCode, uint32_t retryCnt)).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RetryOrchestrateHcclOp).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);

    MOCKER_CPP(&HcclCommAicpu::CheckOpExecStatus).stubs().with(any()).will(returnValue(HCCL_E_SUSPENDING));

    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);

    hcclCommAicpu->bsrSendOpId_.tag[0] = 0;
    hcclCommAicpu->bsrSendOpId_.srcRank = 0;
    hcclCommAicpu->bsrSendOpId_.detRank = 1;
    hcclCommAicpu->bsrRecvOpId_.tag[0] = 0;
    hcclCommAicpu->bsrRecvOpId_.srcRank = 7;
    hcclCommAicpu->bsrRecvOpId_.detRank = 0;
    hcclCommAicpu->bsrTargetOpId_.index = 1;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlTargetOp).stubs().with(any()).will(invoke(GetOpExecCtrlTargetOpStub));
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub2));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmRetryProcess(
                  algName, param, executor, algResource, state, errorCode, retryCnt, beginSqePos, endSqePos),
        HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}
#if 0 //栈溢出
TEST_F(AicpuUnfold_UT, aicpu_prof_test_init_false)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    MOCKER(AdprofCheckFeatureIsOn).stubs().will(returnValue(0));
    bool isL1On = dfx::ProfilingManager::IsProfL1On();
    bool isL0On = dfx::ProfilingManager::IsProfL0On();
    EXPECT_EQ(isL1On, false);
    EXPECT_EQ(isL0On, false);
    HcclResult ret = HCCL_SUCCESS;
    HcclOpResParam paramTask;
    memset(&paramTask, 0, sizeof(HcclOpResParam));
    std::vector<void *> bufferVec;
    std::string hcomId = "hcom_aicpu_unfold14";
    SqCqeContext sqeCqeContext;
    TestConstructHcclOpResParam(paramTask, bufferVec, hcomId, sqeCqeContext);
    paramTask.winSize = 4096;
    DeviceMem inputMem = DeviceMem::alloc(4096);
    DeviceMem outputMem = DeviceMem::alloc(4096);
    paramTask.localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
    paramTask.localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
    DeviceMem hostStateInfo = DeviceMem::alloc(4096);
    DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
    paramTask.hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
    paramTask.aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
    DeviceMem aicpuCustomDev = DeviceMem::alloc(sizeof(AicpuCustomParam));
    paramTask.aicpuCustomParamAddr = reinterpret_cast<u64>(aicpuCustomDev.ptr());
    paramTask.aicpuCustomParamSize = sizeof(AicpuCustomParam);

    KFCResInitTask initTask;
    initTask.context = uint64_t(&paramTask);
    std::string group = paramTask.hcomId;
    hccl::HcclCommAicpu* hcclCommAicpu;
    ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu,nullptr);
    hcclCommAicpu->SetDumpDebug(false);
    ret = hcclCommAicpu->Init(&paramTask, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    OpParam param;
    param.DataDes.dataType = HCCL_DATA_TYPE_INT8;
    ret = hcclCommAicpu->CombineReportOpInfo(param, true, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    for (auto ptr : bufferVec) {
        free(ptr);
    }
    inputMem.free();
    outputMem.free();
    hostStateInfo.free();
    aicpuStateInfo.free();
    aicpuCustomDev.free();
}
#endif
class CollExecutorMock1 : public CollExecutorBase {
public:
    CollExecutorMock1(const HcclDispatcher dispatcher, std::unique_ptr<TopoMatcher> &topoMatcher):CollExecutorBase(dispatcher, topoMatcher) {};
    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algRes) {return HCCL_SUCCESS;}
    HcclResult CalcResRequest(const OpParam& param, AlgResourceRequest &resourceRequest) {return HCCL_SUCCESS;}
    HcclResult CreatePairWiseList(HcclSendRecvItem *sendRecvInfo, u32 itemNum) {return HCCL_SUCCESS;}
    HcclResult GetPairWiseList(std::vector<std::vector<HcclSendRecvItem*>> &sendRecvPairList) {
        std::vector<HcclSendRecvItem *> temp;
        temp.push_back(&sendItem);
        temp.push_back(&recvItem);
        sendRecvPairList.push_back(temp);

        return HCCL_SUCCESS;
    }
    HcclSendRecvItem sendItem = {HcclSendRecvType::HCCL_SEND, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
    HcclSendRecvItem recvItem = {HcclSendRecvType::HCCL_RECV, nullptr, 0, HCCL_DATA_TYPE_INT8, 0};
};

TEST_F(AicpuUnfold_UT, HcclOpExec_initexecloop)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    OpParam opParam;
    opParam.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    opParam.tag = "hccl_llt_batch_send_recv_test";
    opParam.BatchSendRecvDataDes.curIterNum = 0;
    hcclCommAicpu->retryEnable_ = true;

    HcclCommParams param;
    RankTable_t rankTable;
    param.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());
    TestConstructParam(param, rankTable);
    implBase->Init(param, rankTable);
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    const HcclDispatcher dispatcher = nullptr;

    std::unique_ptr<CollExecutorBase> executor(new CollExecutorMock1(dispatcher, topoMatcher));
     u32 loopNum = 0;
    hcclCommAicpu->InitExecLoop(opParam, executor, loopNum);

    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, HcclOpExec_ResetBSRRetryCnt_bsr_retry)
{

    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;

    hcclCommAicpu->bsrRetryOp_ = HCCL_SEND;
    hcclCommAicpu->ResetBSRRetryCnt();
    EXPECT_EQ(hcclCommAicpu->bsrSendRetryCnt_, 0);

    hcclCommAicpu->bsrRetryOp_ = HCCL_RECV;
    hcclCommAicpu->ResetBSRRetryCnt();
    EXPECT_EQ(hcclCommAicpu->bsrRecvRetryCnt_, 0);

    GlobalMockObject::verify();
    delete hcclCommAicpu;
}


TEST_F(AicpuUnfold_UT, HcclOpExec_aicpuHdc)
{
    AicpuHdc aicpuHdc;

    MOCKER_CPP(&HDCommunicate::Get)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HDCommunicate::Put)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    std::shared_ptr<HDCommunicate> h2dTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, sizeof(KfcExecControl)));

    HcclOpIdentifier opId;
    KfcStatus state;
    KfcError errorCode;
    u32 retryCount;
    HcclResult ret = aicpuHdc.SetOpExecStatus(h2dTransfer, opId, state, errorCode, retryCount);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = aicpuHdc.GetOpExecCtrlTargetOp(h2dTransfer, opId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ChangeLinkInfo changeLinkInfo;
    ret = aicpuHdc.GetOpExecChangeLink(h2dTransfer, changeLinkInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    GlobalMockObject::verify();
}

TEST_F(AicpuUnfold_UT, HcclOpExec_HcclOpExecFsmInitProcess)
{

    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    std::string newTag = "newTag";
    OpParam param;
    param.opType = HcclCMDType::HCCL_CMD_SEND;
    AlgResourceResponse algResource;
    HcclOpExecFSM fsmState;
    KfcError errorCode;
    HcclResult result = hcclCommAicpu->HcclOpExecFsmInitProcess(newTag, param, algResource, fsmState, errorCode);
    EXPECT_EQ(result, HCCL_SUCCESS);
    // Arrange
    std::string newTag1 = "newTag";
    OpParam param1;
    param1.opType = HcclCMDType::HCCL_CMD_BATCH_SEND_RECV;
    param1.BatchSendRecvDataDes.curIterNum = 0;
    AlgResourceResponse algResource1;
    HcclOpExecFSM fsmState1;
    KfcError errorCode1;
    result = hcclCommAicpu->HcclOpExecFsmInitProcess(newTag1, param1, algResource1, fsmState1, errorCode1);
    EXPECT_EQ(result, HCCL_SUCCESS);
    std::string newTag2 = "newTag";
    OpParam param2;
    param.opType = HcclCMDType::HCCL_CMD_SEND;
    AlgResourceResponse algResource2;
    HcclOpExecFSM fsmState2;
    KfcError errorCode2;
    result = hcclCommAicpu->HcclOpExecFsmInitProcess(newTag2, param2, algResource2, fsmState2, errorCode2);
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(fsmState2, HcclOpExecFSM::HCCL_OP_EXEC_FSM_LAUNCH);

    std::string newTag3 = "newTag";
    OpParam param3;
    param.opType = HcclCMDType::HCCL_CMD_SEND;
    AlgResourceResponse algResource3;
    HcclOpExecFSM fsmState3;
    KfcError errorCode3;
    result = hcclCommAicpu->HcclOpExecFsmInitProcess(newTag3, param3, algResource3, fsmState3, errorCode3);
    EXPECT_EQ(result, HCCL_SUCCESS);
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, InitBatchSendRecvOpId)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    // std::vector<std::vector<HcclSendRecvItem*>>
    HcclSendRecvItem recv_item;
    recv_item.sendRecvType = HcclSendRecvType::HCCL_SEND;
    HcclSendRecvItem recv_item1;
    recv_item1.sendRecvType = HcclSendRecvType::HCCL_RECV;
    std::vector<HcclSendRecvItem *> recv_item_vec{&recv_item};
    std::vector<HcclSendRecvItem *> recv_item_vec1{&recv_item1};
    std::vector<std::vector<HcclSendRecvItem *>> recv_item_vec_2d{recv_item_vec, recv_item_vec1};
    hcclCommAicpu->bsrSendRecvPairs_ = recv_item_vec_2d;
    auto BSR_RETRY_STREAM_NUM = 2;
    MOCKER_CPP(&HcclCommAicpu::GetBsrTransportQpn).stubs().will(returnValue(HCCL_SUCCESS));
    {
        OpParam param;
        AlgResourceResponse algResource;
        param.tag = "test_tag";
        HcclSendRecvItem sendrecvPair;
        sendrecvPair.sendRecvType = HcclSendRecvType::HCCL_SEND;
        sendrecvPair.remoteRank = 1;
        HcclOpIdentifier opId;
        HcclResult result = hcclCommAicpu->InitBatchSendRecvOpId(param, &sendrecvPair, opId, 0, algResource);
        EXPECT_EQ(result, HCCL_SUCCESS);
        EXPECT_EQ(opId.srcRank, hcclCommAicpu->topoInfo_.userRank);
        EXPECT_EQ(opId.detRank, sendrecvPair.remoteRank);
        EXPECT_EQ(opId.isSendRecv, true);
    }

    {
        OpParam param;
        AlgResourceResponse algResource;
        param.tag = "test_tag";
        HcclSendRecvItem sendrecvPair;
        sendrecvPair.sendRecvType = HcclSendRecvType::HCCL_RECV;
        sendrecvPair.remoteRank = 1;
        HcclOpIdentifier opId;
        HcclResult result = hcclCommAicpu->InitBatchSendRecvOpId(param, &sendrecvPair, opId, 0, algResource);
        EXPECT_EQ(result, HCCL_SUCCESS);
        EXPECT_EQ(opId.srcRank, sendrecvPair.remoteRank);
        EXPECT_EQ(opId.detRank, hcclCommAicpu->topoInfo_.userRank);
        EXPECT_EQ(opId.isSendRecv, true);
    }

    {
        OpParam param;
        param.BatchSendRecvDataDes.curIterNum = 1;
        AlgResourceResponse algResource;
        algResource.slaveStreams.resize(BSR_RETRY_STREAM_NUM);
        EXPECT_EQ(hcclCommAicpu->InitBatchSendRecvOpId(param, algResource), HCCL_SUCCESS);
    }

    {
        OpParam param;
        AlgResourceResponse algResource;
        param.BatchSendRecvDataDes.curIterNum = 2;
        algResource.slaveStreams.resize(BSR_RETRY_STREAM_NUM - 1);
        EXPECT_EQ(hcclCommAicpu->InitBatchSendRecvOpId(param, algResource), HCCL_E_INTERNAL);
    }

    {
        OpParam param;
        param.BatchSendRecvDataDes.curIterNum = 1;
        AlgResourceResponse algResource;
        algResource.slaveStreams.resize(BSR_RETRY_STREAM_NUM);
        EXPECT_EQ(hcclCommAicpu->InitBatchSendRecvOpId(param, algResource), HCCL_SUCCESS);
    }

    {
        OpParam param;
        param.BatchSendRecvDataDes.curIterNum = 0;
        AlgResourceResponse algResource;
        algResource.slaveStreams.resize(BSR_RETRY_STREAM_NUM);
        EXPECT_EQ(hcclCommAicpu->InitBatchSendRecvOpId(param, algResource), HCCL_SUCCESS);
    }
    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, st_GetBsrTransportQpn)
{
    MOCKER_CPP(&Transport::GetTransportId).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportManager::Alloc).stubs().will(returnValue(HCCL_SUCCESS));

    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    HcclSendRecvItem recv_item;
    recv_item.remoteRank = 0;
    recv_item.sendRecvType = HcclSendRecvType::HCCL_SEND;
    hcclCommAicpu->topoInfo_.userRank = 1;

    struct TransportRequest transportRequest;
    transportRequest.localUserRank = 1;
    transportRequest.remoteUserRank = 0;
    struct SingleSubCommTransport singleSubCommTransport;
    singleSubCommTransport.transportRequests.push_back(transportRequest);
    std::shared_ptr<Transport> link = nullptr;
    auto type = TransportType::TRANS_TYPE_IBV_EXP;
    HcclDispatcher dispatcher;
    TransportPara para{};
    const std::unique_ptr<NotifyPool> notifyPool_;
    MachinePara machinePara;
    link.reset(new (std::nothrow) Transport(type, para, dispatcher, notifyPool_, machinePara));
    singleSubCommTransport.links.push_back(link);
    singleSubCommTransport.userRank2subCommRank.insert({0, 0});

    struct SingleSubCommTransport singleSubCommTransport1;
    struct TransportRequest transportRequest1;
    transportRequest1.localUserRank = 1;
    transportRequest1.remoteUserRank = 0;
    singleSubCommTransport1.transportRequests.push_back(transportRequest1);
    std::shared_ptr<Transport> link1 = nullptr;
    type = TransportType::TRANS_TYPE_IBV_EXP;
    link.reset(new (std::nothrow) Transport(type, para, dispatcher, notifyPool_, machinePara));
    singleSubCommTransport1.links.push_back(link1);
    singleSubCommTransport1.userRank2subCommRank.insert({0, 0});

    hccl::AlgResourceResponse algRes;
    algRes.opTransportResponse.resize(CommPlane::COMM_LEVEL_RESERVED);
    algRes.opTransportResponse[CommPlane::COMM_COMBINE_ORDER].push_back(singleSubCommTransport);
    algRes.opTransportResponse[CommPlane::COMM_COMBINE_ORDER].push_back(singleSubCommTransport1);
    u32 qpn = 0 ;
    EXPECT_EQ(hcclCommAicpu->GetBsrTransportQpn(&recv_item, algRes, qpn), HCCL_SUCCESS);

    GlobalMockObject::verify();
    delete hcclCommAicpu;
}

static int loopCntV2 = 0;
static int handleIdCntV2 = 0;
static bool ReadAddrMsgStubV2(AicpuKfcRpcServerV2 *tmp, HcclApi::HcclMsg *tmpMsg, HcclApi::HcclMsg *msgList, u32 queueIdx, uint32_t msgPos)
{
    HcclMsgForTest *hcclMsg = (HcclMsgForTest *)tmpMsg;
    if (loopCntV2 >= 1) {
        hcclMsg->commType = HCCL_CMD_FINALIZE;
        hcclMsg->version = 1;
    } else {
        HcclMsgV1ForTest *hcclMsgV1 = reinterpret_cast<HcclMsgV1ForTest *>(hcclMsg);
        hcclMsgV1->commType = HCCL_CMD_ALLTOALLV;
        hcclMsgV1->opType = HCCL_REDUCE_RESERVED;
        hcclMsgV1->dataCnt = 2048;
        hcclMsgV1->strideCount = 0;
        hcclMsgV1->hcclDataType = HCCL_DATA_TYPE_FP16;
        hcclMsgV1->repeatCnt = 1;
        hcclMsgV1->selfHandleID = handleIdCntV2;
        hcclMsgV1->seqNum = 0;
        hcclMsgV1->version = 1;
        handleIdCntV2++;
    }
    loopCntV2++;
    return true;
}

TEST_F(AicpuUnfold_UT, st_DispatcherProfilingRdmaSend)
{
    MOCKER(AdprofCheckFeatureIsOn).stubs().will(returnValue(1));
    bool isL1On = dfx::ProfilingManager::IsProfL1On();
    EXPECT_EQ(isL1On, true);

    uint32_t sqHead = 0;
    uint32_t sqTail = 5;
    u32 dbIndex = 0;
    u64 dbInfo = 0;
    u32 userRank;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;

    Stream stream(streamInfo, true);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    std::unique_ptr<DispatcherAiCpu> dispatcherAiCpu = std::unique_ptr<DispatcherAiCpu>(new (std::nothrow) DispatcherAiCpu(1));
    dispatcherAiCpu->aicpuInfo_.devType = DevType::DEV_TYPE_910B;
    dispatcherAiCpu->aicpuInfo_.devId = 1;
    dispatcherAiCpu->aicpuInfo_.ssid = 1;

    RdmaTaskInfo taskInfo;
    taskInfo.remoteRank = userRank;
    dispatcherAiCpu->Init();
    auto ret = dispatcherAiCpu->RdmaSend(dbIndex, dbInfo, stream, taskInfo);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    ret = dfx::ProfilingManager::ReportTaskInfo(stream.id(), stream.GetSqeContextPtr());
    GlobalMockObject::verify();
}


TEST_F(AicpuUnfold_UT, TaskInfo2Addition_ShouldSetCorrectFields_WhenMemcpySucceeds) {
    // Arrange
    const char testData[] = "test data";
    int testLen = sizeof(testData);
    MsprofAdditionalInfo reporterData;
    memset(&reporterData, 0, sizeof(reporterData));

    // Act
    HcclResult result = dfx::ProfilingManager::TaskInfo2Addition(testData, testLen, reporterData);

    // Assert
    EXPECT_EQ(result, HCCL_SUCCESS);
    EXPECT_EQ(reporterData.level, MSPROF_REPORT_AICPU_LEVEL);
    EXPECT_EQ(reporterData.type, MSPROF_REPORT_AICPU_MC2_BATCH_HCCL_INFO);
    EXPECT_EQ(reporterData.threadId, SalGetTid());
    EXPECT_EQ(reporterData.dataLen, testLen);
    EXPECT_EQ(reporterData.timeStamp, 0);
    EXPECT_EQ(memcmp(reporterData.data, testData, testLen), 0);
}

TEST_F(AicpuUnfold_UT, TaskInfo2Addition_ShouldReturnError_WhenMemcpyFails) {
    // Arrange
    const char* testData = nullptr; // Invalid data pointer
    int testLen = 10; // Arbitrary length
    MsprofAdditionalInfo reporterData;
    memset(&reporterData, 0, sizeof(reporterData));

    // Act
    HcclResult result = dfx::ProfilingManager::TaskInfo2Addition(testData, testLen, reporterData);

    // Assert
    EXPECT_EQ(result, HCCL_E_MEMORY);
}

TEST_F(AicpuUnfold_UT, st_GenTaskExceptionInfo)
{
    HcclResult ret = HCCL_SUCCESS;
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;

    MOCKER_CPP(&HDCommunicate::Put).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 100;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, true);
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);

    rtLogicCqReport_t cqeException;
    cqeException.sqeType = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
    hcclCommAicpu->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, 10));
    u8 sqeType = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
    ret = hcclCommAicpu->GenTaskExceptionInfo(sqeType, stream, 1);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    delete hcclCommAicpu;
    GlobalMockObject::verify();
}
TEST_F(AicpuUnfold_UT, PrintTaskException_NoTask_st)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 1;
    uint32_t sqTail = 1;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 1;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = stream;
    MOCKER(QuerySqStatus).stubs().with(any(),any(),outBound(sqHead),outBound(sqTail)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclTraceInfo::Flush).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->commOpenStatus = true;
    hcclCommAicpu->PrintTaskExceptionAllStreams();
    delete hcclCommAicpu;
}
TEST_F(AicpuUnfold_UT, PrintTaskException_noActive_st)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    uint32_t sqHead = 0;
    uint32_t sqTail = 100;
    HcclComStreamInfo streamInfo;
    streamInfo.actualStreamId = 1;
    streamInfo.sqId = 1;
    streamInfo.sqDepth = 10;
    streamInfo.sqBaseAddr = &streamInfo;
    streamInfo.logicCqId = 1;
    Stream stream(streamInfo, false);
    SqCqeContext sqeCqeCtx;
    sqeCqeCtx.sqContext.inited = false;
    stream.InitSqAndCqeContext(sqHead, sqTail, &sqeCqeCtx);
    hcclCommAicpu->mainStream_ = stream;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_[0]->notifyId_=0x123;
    HCCL_ERROR("FOR TEST out0  address[%p]", hcclCommAicpu->opNotifies_[0].get());

    hcclCommAicpu->mainStream_.GetSqeContextPtr()->buffer.rtsqSqeType[0] = NOTIFY_SQE_V2;
    rtStarsNotifySqeV2_t sqeInfo;
    rtStarsNotifySqeV2_t * sqeInfoPtr = &sqeInfo;
    sqeInfoPtr->header.type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
    sqeInfoPtr->notify_id = 0x123;
    memcpy_s(&(hcclCommAicpu->mainStream_.GetSqeContextPtr()->buffer.rtsMirrorBuffer[0]), HCCL_SQE_SIZE, &sqeInfo, HCCL_SQE_SIZE);
    MOCKER(QuerySqStatus).stubs().with(any(),any(),outBound(sqHead),outBound(sqTail)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclTraceInfo::Flush).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->commOpenStatus = true;
    hcclCommAicpu->PrintTaskExceptionAllStreams();
    delete hcclCommAicpu;
}

std::queue<std::vector<u8>> dataQueue;
HcclResult Send(HcclSocket *socket, const void *data, u64 size)
{
    u8 *bytePtr = static_cast<u8 *>(const_cast<void *>(data));
    dataQueue.push(std::vector<u8>(bytePtr, bytePtr + size));
    return HCCL_SUCCESS;
};
HcclResult Recv(HcclSocket *socket, void *recvBuf, u32 recvBufLen)
{
    auto data = dataQueue.front();
    dataQueue.pop();
    std::memcpy(recvBuf, data.data(), recvBufLen);
    return HCCL_SUCCESS;
};

HcclResult GetNotifyMrInfo(u32 phy_id, RdmaHandle handle, struct mr_info *mrInfo)
{
    static int i = 0;
    static char allocs[2 * 1024 * 1024];
    mrInfo->addr = reinterpret_cast<void *>(&allocs[i * 8]);
    mrInfo->size = 4;
    mrInfo->access = RA_ACCESS_REMOTE_WRITE | RA_ACCESS_LOCAL_WRITE | RA_ACCESS_REMOTE_READ;
    mrInfo->lkey = i;
    mrInfo->rkey = i;
    i++;
    return HCCL_SUCCESS;
}

static HcclResult stub_hrtMemSyncCopy(void *dst, size_t dstMax, const void *src, size_t count, HcclRtMemcpyKind kind)
{
    memcpy(dst, src, count);
    return HCCL_SUCCESS;
}

static HcclResult stub_hrtMemSet(void *dst, size_t dstMax, size_t count)
{
    memset(dst, 0, count);
    return HCCL_SUCCESS;
}

static HcclResult stub_hrtMalloc(void **devPtr, u64 size, bool level2Address)
{
    *devPtr = malloc(size);

    return *devPtr == nullptr ? HCCL_E_INTERNAL : HCCL_SUCCESS;
}

static HcclResult stub_hrtFree(void *devPtr)
{
    if (devPtr != nullptr) {
       free(devPtr);
    }

    return HCCL_SUCCESS;
}

HcclResult TxSendWrlistExtStub(struct wr_info wrList[], u32 sendNum, struct send_wr_rsp opRsp[],
    unsigned int *completeNum, u32 multiQpIndex = RDMA_INVALID_QP_INDEX)
{
    *completeNum = 2;
    return HCCL_SUCCESS;
}

struct StubQpInfo {
    u32 qpn = 0;
};
HcclResult stub_hrtRaQpCreate(RdmaHandle rdmaHandle, int flag, int qpMode, QpHandle &qpHandle)
{
    static u32 qpn = 0;
    StubQpInfo *info = new StubQpInfo();
    info->qpn = qpn++;
    qpHandle = (void *)info;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaAiQpCreate(
    u32 phy_id, RdmaHandle rdmaHandle, struct qp_ext_attrs *attrs, struct ai_qp_info *info1, QpHandle &qpHandle)
{
    static u32 qpn = 0;
    StubQpInfo *info = new StubQpInfo();
    info->qpn = qpn++;
    qpHandle = (void *)info;
    info1->ai_qp_addr = reinterpret_cast<u64>(info);
    info1->db_index = info1->ai_qp_addr;
    info1->sq_index = info1->ai_qp_addr;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaQpCreateWithAttrs(RdmaHandle rdmaHandle, struct qp_ext_attrs *attrs, QpHandle &qpHandle)
{
    static u32 qpn = 0;
    StubQpInfo *info = new StubQpInfo();
    info->qpn = qpn++;
    qpHandle = (void *)info;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaQpDestroy(QpHandle handle)
{
    delete (StubQpInfo *)handle;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaGetQpAttr(QpHandle qpHandle, struct qp_attr *attr)
{
    static u32 qpn = 0;
    attr->qpn = ((StubQpInfo *)qpHandle)->qpn;
    return HCCL_SUCCESS;
}

HcclResult stub_CreateNotifyBuffer(TransportIbverbs *, std::shared_ptr<LocalIpcNotify> &localNotify, MemType notifyType,
    u8 *&exchangeDataPtr, u64 &exchangeDataBlankSize, NotifyLoadType notifyLoadType)
{
    exchangeDataPtr += sizeof(MemMsg);
    exchangeDataBlankSize -= sizeof(MemMsg);
    return HCCL_SUCCESS;
}
#if 0 // 栈溢出
TEST_F(AicpuUnfold_UT, st_MultiQp_useSingleQp)
{
    DlRaFunction::GetInstance().DlRaFunctionInit();
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    std::string path = "sss";
    u32 multiQpThreshold = 1024;
    MOCKER(GetExternalInputQpSrcPortConfigPath).stubs().will(returnValue(path));
    MOCKER(GetExternalInputMultiQpThreshold).stubs().will(returnValue(multiQpThreshold));
    MOCKER_CPP(&HcclCommunicator::BuildOpRetryParam).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommunicator::BuildZeroCopyParam).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Close).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NetworkManager::StopHostNet).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NetDevContext::Deinit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclNetCloseDev).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtMemAsyncCopy).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaMrDereg).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::DeInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaMrReg).stubs().will(returnValue(HCCL_SUCCESS));
    struct qp_ext_attrs;
    ai_qp_info aiQpInfo;
    char aiapaddr[4];
    aiQpInfo.ai_qp_addr = reinterpret_cast<std::uintptr_t>(&aiapaddr);
    aiQpInfo.db_index = 1;
    aiQpInfo.sq_index = 1;
    char aphandle[4];
    QpHandle qpHandle = &aphandle;
    MOCKER(HrtRaQpCreate).stubs().will(invoke(stub_hrtRaQpCreate));
    MOCKER(hrtRaAiQpCreate).stubs().will(invoke(stub_hrtRaAiQpCreate));
    MOCKER(hrtRaQpCreateWithAttrs).stubs().will(invoke(stub_hrtRaQpCreateWithAttrs));
    MOCKER(hrtRaGetQpAttr).stubs().will(invoke(stub_hrtRaGetQpAttr));
    MOCKER(HrtRaQpDestroy).stubs().will(invoke(stub_hrtRaQpDestroy));
    MOCKER(hrtRaSetQpAttrQos).stubs().will(returnValue(HCCL_SUCCESS));
    s32 qpStatus = 1;
    MOCKER(hrtGetRaQpStatus).stubs().with(any(), outBoundP(&qpStatus)).will(returnValue(0));
    MOCKER(HrtRaQpConnectAsync).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSetQpAttrRetryCnt).stubs().will(returnValue(HCCL_SUCCESS));
    // todo stub 不然检查不出来错误 、、已知问题 linkRoce->remoteNotifyKey = notifyAddrKey[0].key
    // HcclCommAicpu 检查roceQpNumSum等
    MOCKER_CPP(&LocalIpcNotify::Grant).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalIpcNotify::GetNotifyOffset).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(ra_qp_connect_async).stubs().with(any()).will(returnValue(EOK));
    MOCKER_CPP(&HcclSocket::DeInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Close).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64)).stubs().with(any()).will(invoke(Send));
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32)).stubs().with(any()).will(invoke(Recv));
    MOCKER(hrtRaDeRegGlobalMr).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaRegGlobalMr).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaGetNotifyMrInfo).stubs().with(any()).will(invoke(GetNotifyMrInfo));
    MOCKER(hcclStreamSynchronize).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    MOCKER(hrtAicpuKernelLaunchExWithArgs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportDeviceIbverbs::SendWrlistExt).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    void *dispatcherPtr = nullptr;
    EXPECT_EQ(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr), HCCL_SUCCESS);
    ASSERT_NE(dispatcherPtr, nullptr);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    DispatcherPub *dispatcher = reinterpret_cast<DispatcherPub *>(dispatcherPtr);
    EXPECT_EQ(dispatcher->AddRetryPreamble(stream), HCCL_SUCCESS);
    // init notifyPool
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    std::string commTag = "Dev_Aicpu_multiQp";
    EXPECT_EQ(notifyPool->Init(0), HCCL_SUCCESS);
    EXPECT_EQ(notifyPool->RegisterOp(commTag), HCCL_SUCCESS);

    RaResourceInfo raResourceInfo;
    IpSocket ipSocket;
    u64 nicSocketHandle = 0;
    rdevInfo_t nicRdmaHandle = {0};
    ipSocket.nicSocketHandle = reinterpret_cast<void *>(&nicSocketHandle);
    ipSocket.nicRdmaHandle = reinterpret_cast<void *>(&nicRdmaHandle);
    NicType socketType = NicType::DEVICE_NIC_TYPE;
    HcclSocketRole localRole = HcclSocketRole::SOCKET_ROLE_SERVER;
    HcclIpAddress localIp;
    EXPECT_EQ(localIp.SetReadableAddress("127.0.0.1"), HCCL_SUCCESS);
    raResourceInfo.nicSocketMap[localIp] = ipSocket;

    MOCKER_CPP(&NetworkManager::GetRaResourceInfo)
        .stubs()
        .with(outBound(raResourceInfo))
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    std::string algName = "Dev_Aicpu_multiQp";
    AlgResourceResponse algResource;
    memset_s(&algResource, sizeof(AlgResourceResponse), 0, sizeof(AlgResourceResponse));

    char machineParaMem[2 * 1024 * 1024];
    hccl::NetDevContext nicPortCtx;
    nicPortCtx.Init(socketType, 0, 0, localIp);
    std::list<DeviceMem> ibverbsRemoteNotify_;
    std::list<DeviceMem> ibverbsLocalNotify_;
    std::string newTag = "Dev_Aicpu_multiQp";
    rtStream_t aicpuStream;
    EXPECT_EQ(0, rtStreamCreate(&aicpuStream, 5));

    auto makeAlgResourceResponse = [&](OpCommTransport &algResourceResponse) {
        int rank = 0;

        LevelNSubCommTransport level0Transport(1);
        SingleSubCommTransport &singleSubCommTransport = level0Transport[0];
        singleSubCommTransport.links.resize(2);
        singleSubCommTransport.status.resize(1, TransportStatus::READY);
        singleSubCommTransport.taskNum = 1;
        singleSubCommTransport.supportDataReceivedAck = true;
        singleSubCommTransport.linkMode = LinkMode::LINK_DUPLEX_MODE;
        singleSubCommTransport.enableUseOneDoorbell = true;
        singleSubCommTransport.needVirtualLink = false;
        singleSubCommTransport.transportRequests.resize(2);

        TransportRequest &transportRequest = singleSubCommTransport.transportRequests[0];
        transportRequest.isValid = true;
        transportRequest.localUserRank = 0;
        transportRequest.remoteUserRank = 1;
        transportRequest.inputMemType = TransportMemType::SCRATCH;
        transportRequest.outputMemType = TransportMemType::CCL_OUTPUT;
        transportRequest.isUsedRdma = true;
        transportRequest.notifyNum = 0;

        TransportRequest &transportRequest1 = singleSubCommTransport.transportRequests[1];
        transportRequest1.isValid = true;
        transportRequest1.localUserRank = 0;
        transportRequest1.remoteUserRank = 2;
        transportRequest1.inputMemType = TransportMemType::SCRATCH;
        transportRequest1.outputMemType = TransportMemType::CCL_OUTPUT;
        transportRequest1.isUsedRdma = true;
        transportRequest1.notifyNum = 0;

        MachinePara machinePara_;
        machinePara_.inputMem.ptr_ = &machineParaMem;
        machinePara_.inputMem.size_ = 2 * 1024 * 1024;
        machinePara_.inputMem.owner_ = false;
        machinePara_.outputMem.ptr_ = &machineParaMem;
        machinePara_.outputMem.size_ = 2 * 1024 * 1024;
        machinePara_.outputMem.owner_ = false;
        machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
        machinePara_.linkMode = LinkMode::LINK_DUPLEX_MODE;
        machinePara_.collectiveId = "11111";
        machinePara_.tag = "Dev_Aicpu_multiQp";
        machinePara_.serverId = "172.10.3.69";
        machinePara_.localIpAddr = HcclIpAddress("192.168.0.11");
        machinePara_.remoteIpAddr = HcclIpAddress("192.168.0.12");
        machinePara_.localSocketPort = 60000;
        machinePara_.remoteSocketPort = 60000;
        machinePara_.localDeviceId = 0;
        machinePara_.remoteDeviceId = 1;
        machinePara_.deviceLogicId = 1;
        machinePara_.localUserrank = 0;
        machinePara_.remoteUserrank = 1;
        machinePara_.localWorldRank = 0;
        machinePara_.remoteWorldRank = 1;
        machinePara_.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
        machinePara_.deviceType = DevType::DEV_TYPE_910_93;
        machinePara_.sockets.resize(2);
        // init socket
        std::shared_ptr<HcclSocket> &tempSocket = machinePara_.sockets[0];
        tempSocket.reset(new (std::nothrow) HcclSocket(reinterpret_cast<HcclNetDevCtx>(&nicPortCtx), 6000));
        tempSocket->Init();
        tempSocket->localRole_ = HcclSocketRole::SOCKET_ROLE_SERVER;
        tempSocket->tag_ = commTag;
        tempSocket->fdHandle_ = (void *)0x01;
        tempSocket->status_ = HcclSocketStatus::SOCKET_OK;
        machinePara_.sockets[1] = tempSocket;

        machinePara_.exchangeInfo.resize(MAX_EXCHANGE_DATA_LEN);
        machinePara_.supportDataReceivedAck = true;
        machinePara_.isAicpuModeEn = true;
        machinePara_.srcPorts = {60000};
        machinePara_.notifyNum = 0;
        machinePara_.qpMode = QPMode::INVALID;

        hrtSetDevice(rank);
        ;
        TransportPara para;
        para.timeout = std::chrono::seconds(300);
        TransportDeviceP2pData transDevP2pData;
        TransportDeviceIbverbsData transDevIbverbsData;
        LINK &link = singleSubCommTransport.links[0];
        link.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP,
            para,
            dispatcher,
            notifyPool,
            machinePara_,
            transDevP2pData,
            transDevIbverbsData));
        // todo

        ASSERT_EQ(HCCL_SUCCESS, link->Init());
        LINK &link1 = singleSubCommTransport.links[1];
        link1.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP,
            para,
            dispatcher,
            notifyPool,
            machinePara_,
            transDevP2pData,
            transDevIbverbsData));
        ASSERT_EQ(HCCL_SUCCESS, link1->Init());
        // todo
        algResourceResponse.push_back(level0Transport);
    };
    auto makeRankInfos = [&](std::vector<RankInfo> &rank_vector) {
        RankInfo tmp_para_0;

        tmp_para_0.userRank = 0;
        tmp_para_0.devicePhyId = 0;
        tmp_para_0.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_0.serverIdx = 0;
        tmp_para_0.serverId = "10.0.0.10";
        tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
        tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        RankInfo tmp_para_1;

        tmp_para_1.userRank = 1;
        tmp_para_1.devicePhyId = 1;
        tmp_para_1.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_1.serverIdx = 0;
        tmp_para_1.serverId = "10.0.0.10";
        tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
        tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        RankInfo tmp_para_2;

        tmp_para_2.userRank = 2;
        tmp_para_2.devicePhyId = 2;
        tmp_para_2.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_2.serverIdx = 0;
        tmp_para_2.serverId = "10.0.0.10";
        tmp_para_2.nicIp.push_back(HcclIpAddress("192.168.0.13"));
        tmp_para_2.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        RankInfo tmp_para_3;

        tmp_para_3.userRank = 3;
        tmp_para_3.devicePhyId = 3;
        tmp_para_3.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_3.serverIdx = 0;
        tmp_para_3.serverId = "10.0.0.10";
        tmp_para_3.nicIp.push_back(HcclIpAddress("192.168.0.14"));
        tmp_para_3.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        rank_vector.push_back(tmp_para_0);
        rank_vector.push_back(tmp_para_1);
        rank_vector.push_back(tmp_para_2);
        rank_vector.push_back(tmp_para_3);
    };
    auto getTransportLinkRoce = [&](const std::shared_ptr<Transport> &link, HcclLinkRoceV2 *linkRoce) {
        // localMem & remoteMem
        CHK_RET(link->GetLocalMemDetails(UserMemType::INPUT_MEM, (linkRoce->localMem)[INPUT]));
        CHK_RET(link->GetLocalMemDetails(UserMemType::OUTPUT_MEM, (linkRoce->localMem)[OUTPUT]));
        void *inbufferPtr = nullptr;
        void *outbufferPtr = nullptr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &inbufferPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &outbufferPtr));
        HCCL_DEBUG("[%s]inbufferPtr[%p], outbufferPtr[%p]", __func__, inbufferPtr, outbufferPtr);
        if (inbufferPtr == nullptr || outbufferPtr == nullptr) {
            HCCL_ERROR("[%s]inbufferPtr[%p], outbufferPtr[%p]", __func__, inbufferPtr, outbufferPtr);
            return HCCL_E_INTERNAL;
        }
        (linkRoce->remoteMem)[INPUT].addr = reinterpret_cast<u64>(inbufferPtr);
        (linkRoce->remoteMem)[OUTPUT].addr = reinterpret_cast<u64>(outbufferPtr);
        CHK_RET(link->GetRemoteMemKey(UserMemType::INPUT_MEM, &((linkRoce->remoteMem)[INPUT].key)));
        CHK_RET(link->GetRemoteMemKey(UserMemType::OUTPUT_MEM, &((linkRoce->remoteMem)[OUTPUT].key)));
        CHK_RET(link->GetRemoteMemSize(UserMemType::INPUT_MEM, (linkRoce->remoteMem)[INPUT].size));
        CHK_RET(link->GetRemoteMemSize(UserMemType::OUTPUT_MEM, (linkRoce->remoteMem)[OUTPUT].size));
        HCCL_DEBUG("[%s] finish set localMem & remoteMem info", __func__);
        // notifyValue & Key
        std::vector<AddrKey> notifyValueAddrKey;
        CHK_RET(link->GetLocalNotifyValueAddrKey(notifyValueAddrKey));
        linkRoce->notifyValue = notifyValueAddrKey[0].addr;
        linkRoce->notifyValueKey = notifyValueAddrKey[0].key;
        // chipId
        CHK_RET(link->GetChipId(linkRoce->chipId));
        // QPInfo
        std::vector<HcclQpInfoV2> aiQpInfos;
        CHK_RET(link->GetAiQpInfo(aiQpInfos));
        u32 qpNum = aiQpInfos.size();
        if (qpNum > RDMA_QP_MAX_NUM || qpNum < 1) {
            return HCCL_E_INTERNAL;
        }
        std::copy_n(aiQpInfos.begin(), qpNum, linkRoce->QpInfo);
        linkRoce->qpsPerConnection = qpNum - static_cast<u32>(qpNum > 1);  // 多QP数量或单QP模式

        // localnotify & remotenotify
        std::vector<AddrKey> notifyAddrKey;
        std::vector<HcclSignalInfo> signalInfos;
        CHK_RET(link->GetLocalRdmaNotify(signalInfos));
        CHK_RET(link->GetRemoteRdmaNotifyAddrKey(notifyAddrKey));
        constexpr u32 RDMA_NOTIFY_MIN_NUM = 3;
        constexpr u32 RDMA_NOTIFY_MAX_NUM = 8192;
        if ((signalInfos.size() != notifyAddrKey.size()) || (signalInfos.size() < RDMA_NOTIFY_MIN_NUM) ||
            (signalInfos.size() > RDMA_NOTIFY_MAX_NUM) || (notifyAddrKey.size() < RDMA_NOTIFY_MIN_NUM) ||
            (notifyAddrKey.size() > RDMA_NOTIFY_MAX_NUM) ||
            ((signalInfos.size() - RDMA_NOTIFY_MIN_NUM) % linkRoce->qpsPerConnection) ||
            ((notifyAddrKey.size() - RDMA_NOTIFY_MIN_NUM) % linkRoce->qpsPerConnection)) {
            return HCCL_E_INTERNAL;
        }
        u64 notifyNum = (notifyAddrKey.size() - RDMA_NOTIFY_MIN_NUM) / linkRoce->qpsPerConnection -
                        static_cast<u32>(linkRoce->qpsPerConnection > 1);
        linkRoce->singleQPNotifyNum = notifyNum;

        u64 len = signalInfos.size() * sizeof(HcclSignalInfo);
        DeviceMem localListPtr;
        CHK_RET(CopyVectorToDeviceMem(len, localListPtr, signalInfos));
        linkRoce->localNotifyList = reinterpret_cast<u64>(localListPtr.ptr());
        ibverbsRemoteNotify_.emplace_back(std::move(localListPtr));

        std::vector<u64> notifyAddr;
        for (const auto &iter : notifyAddrKey) {
            notifyAddr.push_back(iter.addr);
        }
        len = notifyAddr.size() * sizeof(u64);
        DeviceMem remoteListPtr;
        CHK_RET(CopyVectorToDeviceMem(len, remoteListPtr, notifyAddr));
        linkRoce->remoteNotifyList = reinterpret_cast<u64>(remoteListPtr.ptr());
        ibverbsRemoteNotify_.emplace_back(std::move(remoteListPtr));
        linkRoce->remoteNotifyKey = notifyAddrKey[0].key;  // 只需要刷新一次，remote notify共用
        return HCCL_SUCCESS;
    };
    auto isHcclLinkRoceV2EQOTHHcclLinkRoce = [&](const HcclLinkRoceV2 &linkRoce1,
                                                 const HcclLinkRoceV2 &linkRoce2) -> void {
        EXPECT_EQ(linkRoce1.localMem[INPUT].addr, linkRoce2.localMem[INPUT].addr);
        EXPECT_EQ(linkRoce1.localMem[OUTPUT].addr, linkRoce2.localMem[OUTPUT].addr);
        EXPECT_EQ(linkRoce1.remoteMem[INPUT].addr, linkRoce2.remoteMem[INPUT].addr);
        EXPECT_EQ(linkRoce1.remoteMem[OUTPUT].addr, linkRoce2.remoteMem[OUTPUT].addr);
        EXPECT_EQ(linkRoce1.localMem[INPUT].size, linkRoce2.localMem[INPUT].size);
        EXPECT_EQ(linkRoce1.localMem[OUTPUT].size, linkRoce2.localMem[OUTPUT].size);
        EXPECT_EQ(linkRoce1.localMem[INPUT].key, linkRoce2.localMem[INPUT].key);
        EXPECT_EQ(linkRoce1.localMem[OUTPUT].key, linkRoce2.localMem[OUTPUT].key);
        EXPECT_EQ(linkRoce1.remoteMem[INPUT].key, linkRoce2.remoteMem[INPUT].key);
        EXPECT_EQ(linkRoce1.remoteMem[OUTPUT].key, linkRoce2.remoteMem[OUTPUT].key);
        EXPECT_EQ(linkRoce1.notifyValue, linkRoce2.notifyValue);
        EXPECT_EQ(linkRoce1.notifyValueKey, linkRoce2.notifyValueKey);
        ASSERT_EQ(linkRoce1.singleQPNotifyNum, linkRoce2.singleQPNotifyNum);
        EXPECT_EQ(linkRoce1.remoteNotifyKey, linkRoce2.remoteNotifyKey);
        ASSERT_EQ(linkRoce1.qpsPerConnection, linkRoce2.qpsPerConnection);
        for (u32 i = 0; i < linkRoce1.qpsPerConnection + static_cast<u32>(linkRoce1.qpsPerConnection > 1); i++) {
            EXPECT_NE(linkRoce1.QpInfo[i].qpPtr, 0);
            EXPECT_NE(linkRoce2.QpInfo[i].qpPtr, 0);
            EXPECT_EQ(linkRoce1.QpInfo[i].dbIndex, linkRoce2.QpInfo[i].dbIndex);
            EXPECT_EQ(linkRoce1.QpInfo[i].sqIndex, linkRoce2.QpInfo[i].sqIndex);
            EXPECT_EQ(linkRoce1.QpInfo[i].qpPtr, linkRoce2.QpInfo[i].qpPtr);
        }

        // localNotifyList & remoteNotifyList
        HcclSignalInfo *localNotifyList1 = reinterpret_cast<HcclSignalInfo *>(linkRoce1.localNotifyList);
        HcclSignalInfo *localNotifyList2 = reinterpret_cast<HcclSignalInfo *>(linkRoce2.localNotifyList);
        u64 *remoteNotifyList1 = reinterpret_cast<u64 *>(linkRoce1.remoteNotifyList);
        u64 *remoteNotifyList2 = reinterpret_cast<u64 *>(linkRoce2.remoteNotifyList);
        for (int i = 0; i < 3 + linkRoce1.qpsPerConnection * (linkRoce1.singleQPNotifyNum + 1); ++i) {
            // ptr() //notifyId_
            EXPECT_EQ(localNotifyList1[i].addr, localNotifyList2[i].addr);
            EXPECT_EQ(localNotifyList1[i].resId, localNotifyList2[i].resId);
            EXPECT_EQ(remoteNotifyList1[i], remoteNotifyList2[i]);
        }
    };
    {
        char malloc_mem[2 * 1024 * 1024];
        void *tinySendRecvMem = reinterpret_cast<void *>(malloc_mem);
        HcclCommParams commparam;
        HcclCMDType opType = HcclCMDType::HCCL_CMD_GATHER;
        RankTable_t rankTable;
        commparam.deviceType = DevType::DEV_TYPE_910_93;
        std::unique_ptr<HcclCommunicator> impl(new (std::nothrow) HcclCommunicator());
        TestConstructParam(commparam, rankTable);
        EXPECT_EQ(HCCL_SUCCESS, impl->Init(commparam, rankTable));
        impl->hostDeviceLock_->devMem_.ptr_ = tinySendRecvMem;
        impl->hostDeviceLock_->devMem_.size_ = 2 * 1024 * 1024;
        impl->hostDeviceLock_->devMem_.owner_ = false;
        makeAlgResourceResponse(algResource.opTransportResponse);
        EXPECT_EQ(HCCL_SUCCESS, impl->AicpuResourceInit(algName, algResource, newTag, aicpuStream, opType));

        HcclResult ret = HCCL_SUCCESS;
        HcclOpResParam *paramTask = new HcclOpResParam();
        memset(paramTask, 0, sizeof(HcclOpResParam));
        std::vector<void *> bufferVec;
        std::string hcomId = "Dev_Aicpu_multiQp";
        SqCqeContext sqeCqeContext;
        TestConstructHcclOpResParam(*paramTask, bufferVec, hcomId, sqeCqeContext);
        paramTask->winSize = 4096;
        DeviceMem inputMem = DeviceMem::alloc(4096);
        DeviceMem outputMem = DeviceMem::alloc(4096);
        paramTask->localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
        paramTask->localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
        DeviceMem hostStateInfo = DeviceMem::alloc(4096);
        DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
        paramTask->hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
        paramTask->aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
        KFCResInitTask initTask;
        initTask.context = uint64_t(paramTask);
        std::string group = paramTask->hcomId;
        hccl::HcclCommAicpu *hcclCommAicpu;
        ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
        DispatcherAiCpu *pDispatcher = new (std::nothrow) DispatcherAiCpu(0);
        pDispatcher->Init();
        hcclCommAicpu->dispatcher_ = pDispatcher;
        MOCKER_CPP_VIRTUAL(*pDispatcher, &DispatcherAiCpu::RdmaSend, HcclResult(DispatcherAiCpu::*)(u32, u64,
            hccl::Stream &, RdmaTaskInfo &)).stubs().will(returnValue(HCCL_SUCCESS));
        EXPECT_NE(hcclCommAicpu, nullptr);
        hcclCommAicpu->SetDumpDebug(false);
        hcclCommAicpu->isZeroCopy_ = false;
        std::string newTag = group;
        std::string algName = "Dev_Aicpu_multiQp";
        {
                EXPECT_EQ(HCCL_SUCCESS,
                    hcclCommAicpu->InitConfigInfo(reinterpret_cast<HcclOpResParam *>(impl->opResDevicePara_.ptr())));

                HcclOpResParam *opResDeviceParaPtr = reinterpret_cast<HcclOpResParam *>(impl->opResDevicePara_.ptr());
                for (u32 k = 0; k < AICPU_MAX_RANK_NUM; k++) {
                    EXPECT_EQ(HCCL_SUCCESS,
                        hcclCommAicpu->RefreshTransportsResForRank(opResDeviceParaPtr, k, newTag, 0));
                }

                std::array<HcclLinkRoceV2, 4> linkRoces;
                EXPECT_EQ(
                    getTransportLinkRoce(algResource.opTransportResponse[0][0].links[0], &linkRoces[0]), HCCL_SUCCESS);
                EXPECT_EQ(
                    getTransportLinkRoce(algResource.opTransportResponse[0][0].links[1], &linkRoces[1]), HCCL_SUCCESS);
                EXPECT_EQ(
                    getTransportLinkRoce(hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0], &linkRoces[2]), HCCL_SUCCESS);
                EXPECT_EQ(
                    getTransportLinkRoce(hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0], &linkRoces[3]), HCCL_SUCCESS);
                isHcclLinkRoceV2EQOTHHcclLinkRoce(linkRoces[0], linkRoces[2]);
                isHcclLinkRoceV2EQOTHHcclLinkRoce(linkRoces[1], linkRoces[3]);
                std::vector<TxMemoryInfo> txMems;
                txMems.emplace_back(TxMemoryInfo{UserMemType::INPUT_MEM, 0, &machineParaMem, 2 * 1024 * 1024});
                EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
                EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
                EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->TxWaitDone(stream));

                EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
                EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
                EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->TxWaitDone(stream));
                s32 mem_size = 2 * 1024 * 1024;
                struct Transport::Buffer remoteBuf(&machineParaMem, mem_size);
                struct Transport::Buffer localBuf(&machineParaMem, mem_size);
                EXPECT_EQ(HCCL_SUCCESS,
                    hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->WriteAsync(remoteBuf, localBuf, stream));
                EXPECT_EQ(HCCL_SUCCESS,
                    hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->WriteAsync(remoteBuf, localBuf, stream));
                EXPECT_EQ(
                    dynamic_cast<TransportDeviceIbverbs *>(hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->pimpl_)
                        ->multiQpThreshold_,
                    1024);
                EXPECT_EQ(
                    dynamic_cast<TransportDeviceIbverbs *>(hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->pimpl_)
                        ->multiQpThreshold_,
                    1024);
        }
        AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
        AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
        for (auto ptr : bufferVec) {
                free(ptr);
        }
        inputMem.free();
        outputMem.free();
        hostStateInfo.free();
        aicpuStateInfo.free();
        delete paramTask;
    }

    if (dispatcherPtr != nullptr) {
       ret = HcclDispatcherDestroy(dispatcherPtr);
       EXPECT_EQ(ret, HCCL_SUCCESS);
       dispatcherPtr = nullptr;
    }
    EXPECT_EQ(0, rtStreamDestroy(aicpuStream));
    notifyPool->UnregisterOp(commTag);
    notifyPool->Destroy();
}

TEST_F(AicpuUnfold_UT, st_MultiQp_32Qp_3Notify)
{
    DlRaFunction::GetInstance().DlRaFunctionInit();
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    std::string path = "sss";
    u32 multiQpThreshold = 1024;
    MOCKER(GetExternalInputQpSrcPortConfigPath).stubs().will(returnValue(path));
    MOCKER(GetExternalInputMultiQpThreshold).stubs().will(returnValue(multiQpThreshold));
    MOCKER_CPP(&HcclCommunicator::BuildOpRetryParam).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommunicator::BuildZeroCopyParam).stubs().will(returnValue(0));
    MOCKER_CPP(&HcclCommunicator::InitRaResource).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Close).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NetworkManager::StopHostNet).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&NetDevContext::Deinit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclNetCloseDev).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtMemAsyncCopy).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaMrDereg).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::DeInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportIbverbs::GetNicHandle, HcclResult(TransportIbverbs::*)())
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaMrReg).stubs().will(returnValue(HCCL_SUCCESS));
    struct qp_ext_attrs;
    ai_qp_info aiQpInfo;
    char aiapaddr[4];
    aiQpInfo.ai_qp_addr = reinterpret_cast<std::uintptr_t>(&aiapaddr);
    aiQpInfo.db_index = 1;
    aiQpInfo.sq_index = 1;
    char aphandle[4];
    QpHandle qpHandle = &aphandle;
    MOCKER(HrtRaQpCreate).stubs().will(invoke(stub_hrtRaQpCreate));
    MOCKER(hrtRaAiQpCreate).stubs().will(invoke(stub_hrtRaAiQpCreate));
    MOCKER(hrtRaQpCreateWithAttrs).stubs().will(invoke(stub_hrtRaQpCreateWithAttrs));
    MOCKER(hrtRaGetQpAttr).stubs().will(invoke(stub_hrtRaGetQpAttr));
    MOCKER(HrtRaQpDestroy).stubs().will(invoke(stub_hrtRaQpDestroy));
    MOCKER(hrtRaSetQpAttrQos).stubs().will(returnValue(HCCL_SUCCESS));
    s32 qpStatus = 1;
    MOCKER(hrtGetRaQpStatus).stubs().with(any(), outBoundP(&qpStatus)).will(returnValue(0));
    MOCKER(HrtRaQpConnectAsync).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaSetQpAttrRetryCnt).stubs().will(returnValue(HCCL_SUCCESS));
    // todo stub 不然检查不出来错误 、、已知问题 linkRoce->remoteNotifyKey = notifyAddrKey[0].key
    // HcclCommAicpu 检查roceQpNumSum等
    MOCKER_CPP(&LocalIpcNotify::Grant).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&LocalIpcNotify::GetNotifyOffset).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(ra_qp_connect_async).stubs().with(any()).will(returnValue(EOK));
    MOCKER_CPP(&HcclSocket::DeInit).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Close).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclSocket::Send, HcclResult(HcclSocket::*)(const void *, u64)).stubs().with(any()).will(invoke(Send));
    MOCKER_CPP(&HcclSocket::Recv, HcclResult(HcclSocket::*)(void *, u32)).stubs().with(any()).will(invoke(Recv));
    MOCKER(hrtRaDeRegGlobalMr).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaRegGlobalMr).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER(HrtRaGetNotifyMrInfo).stubs().with(any()).will(invoke(GetNotifyMrInfo));
    MOCKER(hcclStreamSynchronize).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    SetWorkflowMode(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE);
    MOCKER(hrtAicpuKernelLaunchExWithArgs).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportDeviceIbverbs::SendWrlistExt).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    void *dispatcherPtr = nullptr;
    EXPECT_EQ(HcclDispatcherInit(DispatcherType::DISPATCHER_NORMAL, 0, &dispatcherPtr), HCCL_SUCCESS);
    ASSERT_NE(dispatcherPtr, nullptr);
    Stream stream(StreamType::STREAM_TYPE_OFFLINE);
    DispatcherPub *dispatcher = reinterpret_cast<DispatcherPub *>(dispatcherPtr);
    EXPECT_EQ(dispatcher->AddRetryPreamble(stream), HCCL_SUCCESS);
    // init notifyPool
    std::unique_ptr<NotifyPool> notifyPool = nullptr;
    notifyPool.reset(new (std::nothrow) NotifyPool());
    EXPECT_NE(notifyPool, nullptr);
    std::string commTag = "Dev_Aicpu_multiQp";
    EXPECT_EQ(notifyPool->Init(0), HCCL_SUCCESS);
    EXPECT_EQ(notifyPool->RegisterOp(commTag), HCCL_SUCCESS);

    RaResourceInfo raResourceInfo;
    IpSocket ipSocket;
    u64 nicSocketHandle = 0;
    rdevInfo_t nicRdmaHandle = {0};
    ipSocket.nicSocketHandle = reinterpret_cast<void *>(&nicSocketHandle);
    ipSocket.nicRdmaHandle = reinterpret_cast<void *>(&nicRdmaHandle);
    NicType socketType = NicType::DEVICE_NIC_TYPE;
    HcclSocketRole localRole = HcclSocketRole::SOCKET_ROLE_SERVER;
    HcclIpAddress localIp;
    EXPECT_EQ(localIp.SetReadableAddress("127.0.0.1"), HCCL_SUCCESS);
    raResourceInfo.nicSocketMap[localIp] = ipSocket;

    MOCKER_CPP(&NetworkManager::GetRaResourceInfo)
        .stubs()
        .with(outBound(raResourceInfo))
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HCCL_SUCCESS;
    std::string algName = "Dev_Aicpu_multiQp";
    AlgResourceResponse algResource;
    memset_s(&algResource, sizeof(AlgResourceResponse), 0, sizeof(AlgResourceResponse));

    char machineParaMem[2 * 1024 * 1024];
    hccl::NetDevContext nicPortCtx;
    nicPortCtx.Init(socketType, 0, 0, localIp);
    std::list<DeviceMem> ibverbsRemoteNotify_;
    std::list<DeviceMem> ibverbsLocalNotify_;
    std::string newTag = "Dev_Aicpu_multiQp";
    rtStream_t aicpuStream;
    EXPECT_EQ(0, rtStreamCreate(&aicpuStream, 5));
    auto makeAlgResourceResponse = [&](OpCommTransport &algResourceResponse) {
        int rank = 0;

        LevelNSubCommTransport level0Transport(1);
        SingleSubCommTransport &singleSubCommTransport = level0Transport[0];
        singleSubCommTransport.links.resize(2);
        singleSubCommTransport.status.resize(1, TransportStatus::READY);
        singleSubCommTransport.taskNum = 1;
        singleSubCommTransport.supportDataReceivedAck = true;
        singleSubCommTransport.linkMode = LinkMode::LINK_DUPLEX_MODE;
        singleSubCommTransport.enableUseOneDoorbell = true;
        singleSubCommTransport.needVirtualLink = false;
        singleSubCommTransport.transportRequests.resize(2);

        TransportRequest &transportRequest = singleSubCommTransport.transportRequests[0];
        transportRequest.isValid = true;
        transportRequest.localUserRank = 0;
        transportRequest.remoteUserRank = 1;
        transportRequest.inputMemType = TransportMemType::SCRATCH;
        transportRequest.outputMemType = TransportMemType::CCL_OUTPUT;
        transportRequest.isUsedRdma = true;
        transportRequest.notifyNum = 3;

        TransportRequest &transportRequest1 = singleSubCommTransport.transportRequests[1];
        transportRequest1.isValid = true;
        transportRequest1.localUserRank = 0;
        transportRequest1.remoteUserRank = 2;
        transportRequest1.inputMemType = TransportMemType::SCRATCH;
        transportRequest1.outputMemType = TransportMemType::CCL_OUTPUT;
        transportRequest1.isUsedRdma = true;
        transportRequest1.notifyNum = 0;

        MachinePara machinePara_;
        machinePara_.inputMem.ptr_ = &machineParaMem;
        machinePara_.inputMem.size_ = 2 * 1024 * 1024;
        machinePara_.inputMem.owner_ = false;
        machinePara_.outputMem.ptr_ = &machineParaMem;
        machinePara_.outputMem.size_ = 2 * 1024 * 1024;
        machinePara_.outputMem.owner_ = false;
        machinePara_.machineType = MachineType::MACHINE_SERVER_TYPE;
        machinePara_.linkMode = LinkMode::LINK_DUPLEX_MODE;
        machinePara_.collectiveId = "11111";
        machinePara_.tag = "Dev_Aicpu_multiQp";
        machinePara_.serverId = "172.10.3.69";
        machinePara_.localIpAddr = HcclIpAddress("192.168.0.11");
        machinePara_.remoteIpAddr = HcclIpAddress("192.168.0.12");
        machinePara_.localSocketPort = 60000;
        machinePara_.remoteSocketPort = 60000;
        machinePara_.localDeviceId = 0;
        machinePara_.remoteDeviceId = 1;
        machinePara_.deviceLogicId = 1;
        machinePara_.localUserrank = 0;
        machinePara_.remoteUserrank = 1;
        machinePara_.localWorldRank = 0;
        machinePara_.remoteWorldRank = 1;
        machinePara_.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
        machinePara_.deviceType = DevType::DEV_TYPE_910_93;
        machinePara_.sockets.resize(2);
        // init socket
        std::shared_ptr<HcclSocket> &tempSocket = machinePara_.sockets[0];
        tempSocket.reset(new (std::nothrow) HcclSocket(reinterpret_cast<HcclNetDevCtx>(&nicPortCtx), 6000));
        tempSocket->Init();
        tempSocket->localRole_ = HcclSocketRole::SOCKET_ROLE_SERVER;
        tempSocket->tag_ = commTag;
        tempSocket->fdHandle_ = (void *)0x01;
        tempSocket->status_ = HcclSocketStatus::SOCKET_OK;
        machinePara_.sockets[1] = tempSocket;

        machinePara_.exchangeInfo.resize(MAX_EXCHANGE_DATA_LEN);
        machinePara_.supportDataReceivedAck = true;
        machinePara_.isAicpuModeEn = true;
        machinePara_.srcPorts.resize(32);
        for (u32 i = 0; i < 32; i++) {
            machinePara_.srcPorts[i] = 60000 + i;
        }
        machinePara_.notifyNum = 3;
        machinePara_.qpMode = QPMode::INVALID;

        hrtSetDevice(rank);
        ;
        TransportPara para;
        para.timeout = std::chrono::seconds(300);
        TransportDeviceP2pData transDevP2pData;
        TransportDeviceIbverbsData transDevIbverbsData;
        LINK &link = singleSubCommTransport.links[0];
        link.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP,
            para,
            dispatcher,
            notifyPool,
            machinePara_,
            transDevP2pData,
            transDevIbverbsData));
        // todo

        ASSERT_EQ(HCCL_SUCCESS, link->Init());
        LINK &link1 = singleSubCommTransport.links[1];
        link1.reset(new Transport(TransportType::TRANS_TYPE_IBV_EXP,
            para,
            dispatcher,
            notifyPool,
            machinePara_,
            transDevP2pData,
            transDevIbverbsData));
        ASSERT_EQ(HCCL_SUCCESS, link1->Init());
        // todo
        algResourceResponse.push_back(level0Transport);
    };
    auto makeRankInfos = [&](std::vector<RankInfo> &rank_vector) {
        RankInfo tmp_para_0;

        tmp_para_0.userRank = 0;
        tmp_para_0.devicePhyId = 0;
        tmp_para_0.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_0.serverIdx = 0;
        tmp_para_0.serverId = "10.0.0.10";
        tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
        tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        RankInfo tmp_para_1;

        tmp_para_1.userRank = 1;
        tmp_para_1.devicePhyId = 1;
        tmp_para_1.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_1.serverIdx = 0;
        tmp_para_1.serverId = "10.0.0.10";
        tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
        tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        RankInfo tmp_para_2;

        tmp_para_2.userRank = 2;
        tmp_para_2.devicePhyId = 2;
        tmp_para_2.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_2.serverIdx = 0;
        tmp_para_2.serverId = "10.0.0.10";
        tmp_para_2.nicIp.push_back(HcclIpAddress("192.168.0.13"));
        tmp_para_2.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        RankInfo tmp_para_3;

        tmp_para_3.userRank = 3;
        tmp_para_3.devicePhyId = 3;
        tmp_para_3.deviceType = DevType::DEV_TYPE_910_93;
        tmp_para_3.serverIdx = 0;
        tmp_para_3.serverId = "10.0.0.10";
        tmp_para_3.nicIp.push_back(HcclIpAddress("192.168.0.14"));
        tmp_para_3.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

        rank_vector.push_back(tmp_para_0);
        rank_vector.push_back(tmp_para_1);
        rank_vector.push_back(tmp_para_2);
        rank_vector.push_back(tmp_para_3);
    };
    auto getTransportLinkRoce = [&](const std::shared_ptr<Transport> &link, HcclLinkRoceV2 *linkRoce) {
        // localMem & remoteMem
        CHK_RET(link->GetLocalMemDetails(UserMemType::INPUT_MEM, (linkRoce->localMem)[INPUT]));
        CHK_RET(link->GetLocalMemDetails(UserMemType::OUTPUT_MEM, (linkRoce->localMem)[OUTPUT]));
        void *inbufferPtr = nullptr;
        void *outbufferPtr = nullptr;
        CHK_RET(link->GetRemoteMem(UserMemType::INPUT_MEM, &inbufferPtr));
        CHK_RET(link->GetRemoteMem(UserMemType::OUTPUT_MEM, &outbufferPtr));
        HCCL_DEBUG("[%s]inbufferPtr[%p], outbufferPtr[%p]", __func__, inbufferPtr, outbufferPtr);
        if (inbufferPtr == nullptr || outbufferPtr == nullptr) {
            HCCL_ERROR("[%s]inbufferPtr[%p], outbufferPtr[%p]", __func__, inbufferPtr, outbufferPtr);
            return HCCL_E_INTERNAL;
        }
        (linkRoce->remoteMem)[INPUT].addr = reinterpret_cast<u64>(inbufferPtr);
        (linkRoce->remoteMem)[OUTPUT].addr = reinterpret_cast<u64>(outbufferPtr);
        CHK_RET(link->GetRemoteMemKey(UserMemType::INPUT_MEM, &((linkRoce->remoteMem)[INPUT].key)));
        CHK_RET(link->GetRemoteMemKey(UserMemType::OUTPUT_MEM, &((linkRoce->remoteMem)[OUTPUT].key)));
        CHK_RET(link->GetRemoteMemSize(UserMemType::INPUT_MEM, (linkRoce->remoteMem)[INPUT].size));
        CHK_RET(link->GetRemoteMemSize(UserMemType::OUTPUT_MEM, (linkRoce->remoteMem)[OUTPUT].size));
        HCCL_DEBUG("[%s] finish set localMem & remoteMem info", __func__);
        // notifyValue & Key
        std::vector<AddrKey> notifyValueAddrKey;
        CHK_RET(link->GetLocalNotifyValueAddrKey(notifyValueAddrKey));
        linkRoce->notifyValue = notifyValueAddrKey[0].addr;
        linkRoce->notifyValueKey = notifyValueAddrKey[0].key;
        // chipId
        CHK_RET(link->GetChipId(linkRoce->chipId));
        // QPInfo
        std::vector<HcclQpInfoV2> aiQpInfos;
        CHK_RET(link->GetAiQpInfo(aiQpInfos));
        u32 qpNum = aiQpInfos.size();
        if (qpNum > RDMA_QP_MAX_NUM || qpNum < 1) {
            return HCCL_E_INTERNAL;
        }
        std::copy_n(aiQpInfos.begin(), qpNum, linkRoce->QpInfo);
        linkRoce->qpsPerConnection = qpNum - static_cast<u32>(qpNum > 1);  // 多QP数量或单QP模式

        // localnotify & remotenotify
        std::vector<AddrKey> notifyAddrKey;
        std::vector<HcclSignalInfo> signalInfos;
        CHK_RET(link->GetLocalRdmaNotify(signalInfos));
        CHK_RET(link->GetRemoteRdmaNotifyAddrKey(notifyAddrKey));
        constexpr u32 RDMA_NOTIFY_MIN_NUM = 3;
        constexpr u32 RDMA_NOTIFY_MAX_NUM = 8192;
        if ((signalInfos.size() != notifyAddrKey.size()) || (signalInfos.size() < RDMA_NOTIFY_MIN_NUM) ||
            (signalInfos.size() > RDMA_NOTIFY_MAX_NUM) || (notifyAddrKey.size() < RDMA_NOTIFY_MIN_NUM) ||
            (notifyAddrKey.size() > RDMA_NOTIFY_MAX_NUM) ||
            ((signalInfos.size() - RDMA_NOTIFY_MIN_NUM) % linkRoce->qpsPerConnection) ||
            ((notifyAddrKey.size() - RDMA_NOTIFY_MIN_NUM) % linkRoce->qpsPerConnection)) {
            return HCCL_E_INTERNAL;
        }
        u64 notifyNum = (notifyAddrKey.size() - RDMA_NOTIFY_MIN_NUM) / linkRoce->qpsPerConnection -
                        static_cast<u32>(linkRoce->qpsPerConnection > 1);
        linkRoce->singleQPNotifyNum = notifyNum;
        DeviceMem localMem;
        u64 len = signalInfos.size() * sizeof(HcclSignalInfo);
        CHK_RET(CopyVectorToDeviceMem(len, localMem, signalInfos));
        linkRoce->localNotifyList = reinterpret_cast<u64>(localMem.ptr());
        ibverbsLocalNotify_.emplace_back(std::move(localMem));
        std::vector<u64> notifyAddr;
        for (const auto &iter : notifyAddrKey) {
            notifyAddr.push_back(iter.addr);
        }
        len = notifyAddr.size() * sizeof(u64);
        DeviceMem remotemem;
        CHK_RET(CopyVectorToDeviceMem(len, remotemem, notifyAddr));
        linkRoce->remoteNotifyList = reinterpret_cast<u64>(remotemem.ptr());
        ibverbsRemoteNotify_.emplace_back(std::move(remotemem));
        linkRoce->remoteNotifyKey = notifyAddrKey[0].key;  // 只需要刷新一次，remote notify共用
        return HCCL_SUCCESS;
    };
    auto isHcclLinkRoceV2EQOTHHcclLinkRoce = [&](const HcclLinkRoceV2 &linkRoce1,
                                                 const HcclLinkRoceV2 &linkRoce2) -> void {
        EXPECT_EQ(linkRoce1.localMem[INPUT].addr, linkRoce2.localMem[INPUT].addr);
        EXPECT_EQ(linkRoce1.localMem[OUTPUT].addr, linkRoce2.localMem[OUTPUT].addr);
        EXPECT_EQ(linkRoce1.remoteMem[INPUT].addr, linkRoce2.remoteMem[INPUT].addr);
        EXPECT_EQ(linkRoce1.remoteMem[OUTPUT].addr, linkRoce2.remoteMem[OUTPUT].addr);
        EXPECT_EQ(linkRoce1.localMem[INPUT].size, linkRoce2.localMem[INPUT].size);
        EXPECT_EQ(linkRoce1.localMem[OUTPUT].size, linkRoce2.localMem[OUTPUT].size);
        EXPECT_EQ(linkRoce1.localMem[INPUT].key, linkRoce2.localMem[INPUT].key);
        EXPECT_EQ(linkRoce1.localMem[OUTPUT].key, linkRoce2.localMem[OUTPUT].key);
        EXPECT_EQ(linkRoce1.remoteMem[INPUT].key, linkRoce2.remoteMem[INPUT].key);
        EXPECT_EQ(linkRoce1.remoteMem[OUTPUT].key, linkRoce2.remoteMem[OUTPUT].key);
        EXPECT_EQ(linkRoce1.notifyValue, linkRoce2.notifyValue);
        EXPECT_EQ(linkRoce1.notifyValueKey, linkRoce2.notifyValueKey);
        ASSERT_EQ(linkRoce1.singleQPNotifyNum, linkRoce2.singleQPNotifyNum);
        EXPECT_EQ(linkRoce1.remoteNotifyKey, linkRoce2.remoteNotifyKey);
        ASSERT_EQ(linkRoce1.qpsPerConnection, linkRoce2.qpsPerConnection);
        for (u32 i = 0; i < linkRoce1.qpsPerConnection + static_cast<u32>(linkRoce1.qpsPerConnection > 1); i++) {
            EXPECT_NE(linkRoce1.QpInfo[i].qpPtr, 0);
            EXPECT_NE(linkRoce2.QpInfo[i].qpPtr, 0);
            EXPECT_EQ(linkRoce1.QpInfo[i].dbIndex, linkRoce2.QpInfo[i].dbIndex);
            EXPECT_EQ(linkRoce1.QpInfo[i].sqIndex, linkRoce2.QpInfo[i].sqIndex);
            EXPECT_EQ(linkRoce1.QpInfo[i].qpPtr, linkRoce2.QpInfo[i].qpPtr);
        }

        // localNotifyList & remoteNotifyList
        HcclSignalInfo *localNotifyList1 = reinterpret_cast<HcclSignalInfo *>(linkRoce1.localNotifyList);
        HcclSignalInfo *localNotifyList2 = reinterpret_cast<HcclSignalInfo *>(linkRoce2.localNotifyList);
        u64 *remoteNotifyList1 = reinterpret_cast<u64 *>(linkRoce1.remoteNotifyList);
        u64 *remoteNotifyList2 = reinterpret_cast<u64 *>(linkRoce2.remoteNotifyList);
        for (int i = 0; i < 3 + linkRoce1.qpsPerConnection *
                                    (linkRoce1.singleQPNotifyNum + static_cast<u32>(linkRoce1.qpsPerConnection != 1));
             ++i) {
            // ptr() //notifyId_
            EXPECT_EQ(localNotifyList1[i].addr, localNotifyList2[i].addr);
            EXPECT_EQ(localNotifyList1[i].resId, localNotifyList2[i].resId);
            EXPECT_EQ(remoteNotifyList1[i], remoteNotifyList2[i]);
        }
    };
    {
       char malloc_mem[2 * 1024 * 1024];
       void *tinySendRecvMem = reinterpret_cast<void *>(malloc_mem);
       HcclCommParams commparam;
       HcclCMDType opType = HcclCMDType::HCCL_CMD_GATHER;
       RankTable_t rankTable;
       commparam.deviceType = DevType::DEV_TYPE_910_93;
       std::unique_ptr<HcclCommunicator> impl(new (std::nothrow) HcclCommunicator());
       TestConstructParam(commparam, rankTable);
       EXPECT_EQ(HCCL_SUCCESS, impl->Init(commparam, rankTable));
       impl->hostDeviceLock_->devMem_.ptr_ = tinySendRecvMem;
       impl->hostDeviceLock_->devMem_.size_ = 2 * 1024 * 1024;
       impl->hostDeviceLock_->devMem_.owner_ = false;
       makeAlgResourceResponse(algResource.opTransportResponse);
       EXPECT_EQ(HCCL_SUCCESS, impl->AicpuResourceInit(algName, algResource, newTag, aicpuStream, opType));

       HcclResult ret = HCCL_SUCCESS;
       HcclOpResParam *paramTask = new HcclOpResParam();
       memset(paramTask, 0, sizeof(HcclOpResParam));
       std::vector<void *> bufferVec;
       std::string hcomId = "Dev_Aicpu_multiQp";
       SqCqeContext sqeCqeContext;
       TestConstructHcclOpResParam(*paramTask, bufferVec, hcomId, sqeCqeContext);
       paramTask->winSize = 4096;
       DeviceMem inputMem = DeviceMem::alloc(4096);
       DeviceMem outputMem = DeviceMem::alloc(4096);
       paramTask->localWindowsIn = reinterpret_cast<u64>(inputMem.ptr());
       paramTask->localWindowsOut = reinterpret_cast<u64>(outputMem.ptr());
       DeviceMem hostStateInfo = DeviceMem::alloc(4096);
       DeviceMem aicpuStateInfo = DeviceMem::alloc(4096);
       paramTask->hostStateInfo = reinterpret_cast<u64>(hostStateInfo.ptr());
       paramTask->aicpuStateInfo = reinterpret_cast<u64>(aicpuStateInfo.ptr());
       KFCResInitTask initTask;
       initTask.context = uint64_t(paramTask);
       std::string group = paramTask->hcomId;
       hccl::HcclCommAicpu *hcclCommAicpu;
       ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
       EXPECT_EQ(ret, HCCL_SUCCESS);
       hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
       DispatcherAiCpu *pDispatcher = new (std::nothrow) DispatcherAiCpu(0);
       MOCKER_CPP_VIRTUAL(*pDispatcher, &DispatcherAiCpu::RdmaSend, HcclResult(DispatcherAiCpu::*)(u32, u64,
            hccl::Stream &, RdmaTaskInfo &)).stubs().will(returnValue(HCCL_SUCCESS));

       pDispatcher->Init();
       hcclCommAicpu->dispatcher_ = pDispatcher;
       EXPECT_NE(hcclCommAicpu, nullptr);
       hcclCommAicpu->SetDumpDebug(false);
       hcclCommAicpu->isZeroCopy_ = false;
       std::string newTag = group;
       std::string algName = "Dev_Aicpu_multiQp";
       {
            EXPECT_EQ(HCCL_SUCCESS,
                hcclCommAicpu->InitConfigInfo(reinterpret_cast<HcclOpResParam *>(impl->opResDevicePara_.ptr())));

            HcclOpResParam *opResDeviceParaPtr = reinterpret_cast<HcclOpResParam *>(impl->opResDevicePara_.ptr());
            for (u32 k = 0; k < AICPU_MAX_RANK_NUM; k++) {
                EXPECT_EQ(HCCL_SUCCESS,
                    hcclCommAicpu->RefreshTransportsResForRank(opResDeviceParaPtr, k, newTag, 3));
            }

            std::array<HcclLinkRoceV2, 4> linkRoces;
            EXPECT_EQ(
                getTransportLinkRoce(algResource.opTransportResponse[0][0].links[0], &linkRoces[0]), HCCL_SUCCESS);
            EXPECT_EQ(
                getTransportLinkRoce(algResource.opTransportResponse[0][0].links[1], &linkRoces[1]), HCCL_SUCCESS);
            EXPECT_EQ(
                getTransportLinkRoce(hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0], &linkRoces[2]), HCCL_SUCCESS);
            EXPECT_EQ(
                getTransportLinkRoce(hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0], &linkRoces[3]), HCCL_SUCCESS);
            isHcclLinkRoceV2EQOTHHcclLinkRoce(linkRoces[0], linkRoces[2]);
            isHcclLinkRoceV2EQOTHHcclLinkRoce(linkRoces[1], linkRoces[3]);
            std::vector<TxMemoryInfo> txMems;
            txMems.emplace_back(TxMemoryInfo{UserMemType::INPUT_MEM, 0, &machineParaMem, 2 * 1024 * 1024});
            EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
            EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
            EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->TxWaitDone(stream));

            EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
            EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->TxAsync(txMems, stream));
            EXPECT_EQ(HCCL_SUCCESS, hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->TxWaitDone(stream));
            s32 mem_size = 2 * 1024 * 1024;
            struct Transport::Buffer remoteBuf(&machineParaMem, mem_size);
            struct Transport::Buffer localBuf(&machineParaMem, mem_size);
            EXPECT_EQ(HCCL_SUCCESS,
                hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->WriteAsync(remoteBuf, localBuf, stream));
            EXPECT_EQ(HCCL_SUCCESS,
                hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->WriteAsync(remoteBuf, localBuf, stream));
            EXPECT_EQ(
                dynamic_cast<TransportDeviceIbverbs *>(hcclCommAicpu->linkRdmaRes_[1]["Dev_Aicpu_multiQp"][0]->pimpl_)
                    ->multiQpThreshold_,
                1024);
            EXPECT_EQ(
                dynamic_cast<TransportDeviceIbverbs *>(hcclCommAicpu->linkRdmaRes_[2]["Dev_Aicpu_multiQp"][0]->pimpl_)
                    ->multiQpThreshold_,
                1024);
       }
       AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
       AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
       for (auto ptr : bufferVec) {
            free(ptr);
       }
       inputMem.free();
       outputMem.free();
       hostStateInfo.free();
       aicpuStateInfo.free();
       delete paramTask;
    }

    if (dispatcherPtr != nullptr) {
       ret = HcclDispatcherDestroy(dispatcherPtr);
       EXPECT_EQ(ret, HCCL_SUCCESS);
       dispatcherPtr = nullptr;
    }
    EXPECT_EQ(0, rtStreamDestroy(aicpuStream));
    notifyPool->UnregisterOp(commTag);
    notifyPool->Destroy();
}
#endif
// 临时补充覆盖率
TEST_F(AicpuUnfold_UT, st_OrchestrateAicpu_tmp)
{
    HcclCommunicator impl;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_GATHER;
    std::string algName;
    OpParam param;
    AlgResourceResponse algResource;
    std::string newTag;
    AlgType algType;
    param.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    MOCKER_CPP(&HcclCommunicator::AicpuResourceInit).stubs().with(any()).will(returnValue(HCCL_E_PARA));
    HcclResult ret = impl.OrchestrateAicpu(opType, algName, param, algResource, newTag, algType);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}
#if 0 // 执行失败
TEST_F(AicpuUnfold_UT, st_OrchestrateAicpu_agv)
{
    HcclCommunicator impl;
    impl.userRankSize_ = 4;
    impl.userRank_ = 0;

    HcclCommParams params;
    RankTable_t rankTable;
    params.deviceType = DevType::DEV_TYPE_910;
    TestConstructParam(params, rankTable);
    impl.Init(params, rankTable);

    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLGATHER_V;
    std::string algName;
    OpParam param;
    void* tmpMem = malloc(1024);
    AlgResourceResponse algResource;
    std::string newTag;
    AlgType algType;
    param.opType = opType;
    param.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    param.inputPtr = tmpMem;
    param.outputPtr = tmpMem;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_BFP16;
    param.VDataDes.dataType = HcclDataType::HCCL_DATA_TYPE_BFP16;
    param.VDataDes.counts = tmpMem;
    param.VDataDes.displs = tmpMem;
    MOCKER_CPP(&HcclCommunicator::BuildOpResParam).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunchIn).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcclResult ret = impl.OrchestrateAicpu(opType, algName, param, algResource, newTag, algType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    free(tmpMem);
}
#endif
TEST_F(AicpuUnfold_UT, test_RunAicpuRpcSrvLaunchV2_agv)
{
    HcclResult ret = HCCL_SUCCESS;
    hccl::HcclCommAicpu* hcclCommAicpu = new hccl::HcclCommAicpu();

    MOCKER(GetExternalInputHcclEnableEntryLog).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::CalcResRequest).stubs().will(invoke(CalcResRequestStub));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().will(returnValue(0));
    MOCKER(AicpuHcclProcess::AicpuGetCommbyGroup).stubs().will(returnValue(hcclCommAicpu));
    MOCKER_CPP(&HcclCommAicpu::AllocAlgResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateNotifyWaitTimeOut).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuShareDataManager::RecordOpInfo).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoMatcher::SetAHCAlgOption).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    const u32 rankSize = 8;
    u64 vDateLen = 2 * rankSize * sizeof(u64);
    u32 dynamicDataSize = sizeof(struct OpTilingVDataDes) + vDateLen;
    void* tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    OpTilingData* opTilingData = static_cast<OpTilingData*>(tempPtr);

    u8* dynamicDataPtr = static_cast<u8*>(tempPtr) + sizeof(struct OpTilingData);
    struct OpTilingVDataDes* vDataPtr = reinterpret_cast<struct OpTilingVDataDes*>(dynamicDataPtr);
    vDataPtr->vDataLen = vDateLen;
    vDataPtr->dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    for (u32 i = 0; i < rankSize; i++) {
        vDataPtr->vData[i] = i; // counts
        vDataPtr->vData[i + rankSize] = i;  // displs
    }
    std::string hcomId = "test";
    HcclOpResParam paramTask;
    memset_s(&paramTask, sizeof(HcclOpResParam), 0, sizeof(HcclOpResParam));
    strcpy_s(paramTask.hcomId, 128, hcomId.c_str());
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_ALLGATHER_V);
    opTilingData->srcRank = 0;
    opTilingData->isZeroCopy = 0;
    //默认清空内存
    for (u32 i = 0 ; i < TOP_HIERARCHICAL_CONF_SIZE; i++) {
        opTilingData->ahcConfInfo[i] = 0;
    }
    std::string tag = "hcom_agv_aicpu_unfold";
    memcpy_s(opTilingData->tag, sizeof(opTilingData->tag), tag.c_str(), tag.size() + 1);
    std::string algName = "AllGatherV";
    tag = reinterpret_cast<char *>(opTilingData->tag);
    std::string newTag = tag + "_" + algName;
    memcpy_s(opTilingData->newTag, sizeof(opTilingData->newTag), newTag.c_str(), newTag.length() + 1);
    memcpy_s(opTilingData->algName, sizeof(opTilingData->algName), algName.c_str(),
        algName.length() + 1);
    KFCTaskComm kfcTask;
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(opTilingData);
    paramTask.rankSize = rankSize;
    paramTask.utraceStatusFlag = 1;
    ret = hcclCommAicpu->InitUtraceInfo(&paramTask);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclCommAicpu->FlushUtraceInfo();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&kfcTask));
    delete(hcclCommAicpu);
    free(tempPtr);
}
#if 0 //执行失败
TEST_F(AicpuUnfold_UT, test_OrchestrateAicpu_rsv)
{
    const u32 rankSize = 4;
    HcclCommunicator impl;
    impl.userRankSize_ = rankSize;
    impl.userRank_ = 0;

    HcclCommParams params;
    RankTable_t rankTable;
    params.deviceType = DevType::DEV_TYPE_910;
    TestConstructParam(params, rankTable);
    impl.Init(params, rankTable);

    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V;
    std::string algName;
    OpParam param;
    u32 vDateLen = 2 * rankSize * sizeof(u64);  // counts/displs
    void* tmpMem = malloc(1024 + vDateLen);
    AlgResourceResponse algResource;
    std::string newTag;
    AlgType algType;
    param.opType = opType;
    param.stream = Stream(StreamType::STREAM_TYPE_ONLINE);
    param.inputPtr = tmpMem;
    param.outputPtr = tmpMem;
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    param.VDataDes.dataType = HcclDataType::HCCL_DATA_TYPE_UINT64;
    param.VDataDes.counts = tmpMem;
    param.VDataDes.displs = tmpMem;
    MOCKER_CPP(&HcclCommunicator::BuildOpResParam).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::AicpuKfcTilingDataLaunchIn).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcclResult ret = impl.OrchestrateAicpu(opType, algName, param, algResource, newTag, algType);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
    free(tmpMem);
}
#endif

TEST_F(AicpuUnfold_UT, test_RunAicpuRpcSrvLaunchV2_rsv)
{
    HcclResult ret = HCCL_SUCCESS;
    hccl::HcclCommAicpu* hcclCommAicpu = new hccl::HcclCommAicpu();

    MOCKER(GetExternalInputHcclEnableEntryLog).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::CalcResRequest).stubs().will(invoke(CalcResRequestStub));
    MOCKER_CPP(&HcclCommAicpu::Orchestrate).stubs().will(returnValue(0));
    MOCKER(AicpuHcclProcess::AicpuGetCommbyGroup).stubs().will(returnValue(hcclCommAicpu));
    MOCKER_CPP(&HcclCommAicpu::AllocAlgResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::UpdateNotifyWaitTimeOut).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AicpuShareDataManager::RecordOpInfo).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TopoMatcher::SetAHCAlgOption).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RecordHostOrder).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    const u32 rankSize = 8;
    u64 vDateLen = 2 * rankSize * sizeof(u64);
    u32 dynamicDataSize = sizeof(struct OpTilingVDataDes) + vDateLen;
    void* tempPtr = malloc(sizeof(OpTilingData) + dynamicDataSize);
    OpTilingData* opTilingData = static_cast<OpTilingData*>(tempPtr);

    u8* dynamicDataPtr = static_cast<u8*>(tempPtr) + sizeof(struct OpTilingData);
    struct OpTilingVDataDes* vDataPtr = reinterpret_cast<struct OpTilingVDataDes*>(dynamicDataPtr);
    vDataPtr->vDataLen = vDateLen;
    vDataPtr->dataType = HcclDataType::HCCL_DATA_TYPE_INT32;
    for (u32 i = 0; i < rankSize; i++) {
        vDataPtr->vData[i] = i; // counts
        vDataPtr->vData[i + rankSize] = i;  // displs
    }
    std::string hcomId = "test";
    HcclOpResParam paramTask;
    memset_s(&paramTask, sizeof(HcclOpResParam), 0, sizeof(HcclOpResParam));
    strcpy_s(paramTask.hcomId, 128, hcomId.c_str());
    opTilingData->opType = static_cast<u8>(HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V);
    opTilingData->srcRank = 1;
    opTilingData->isZeroCopy = 0;
    //默认清空内存
    for (u32 i = 0 ; i < TOP_HIERARCHICAL_CONF_SIZE; i++) {
        opTilingData->ahcConfInfo[i] = 0;
    }
    std::string tag = "hcom_rsv_aicpu_unfold";
    memcpy_s(opTilingData->tag, sizeof(opTilingData->tag), tag.c_str(), tag.size() + 1);
    std::string algName = "ReduceScatterV";
    tag = reinterpret_cast<char *>(opTilingData->tag);
    std::string newTag = tag + "_" + algName;
    memcpy_s(opTilingData->newTag, sizeof(opTilingData->newTag), newTag.c_str(), newTag.length() + 1);
    memcpy_s(opTilingData->algName, sizeof(opTilingData->algName), algName.c_str(),
        algName.length() + 1);
    KFCTaskComm kfcTask;
    kfcTask.context = uint64_t(&paramTask);
    kfcTask.tilingData = uint64_t(opTilingData);
    paramTask.rankSize = rankSize;
    paramTask.utraceStatusFlag = 1;
    ret = hcclCommAicpu->InitUtraceInfo(&paramTask);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = hcclCommAicpu->FlushUtraceInfo();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(0, RunAicpuRpcSrvLaunchV2(&kfcTask));
    delete(hcclCommAicpu);
    free(tempPtr);
}
 
TEST_F(AicpuUnfold_UT, ut_get_orderstream_fail)
{
    s32 deviceLogicId = 0;
    std::string group = "group";
    HcclResult ret = HCCL_SUCCESS;
    Stream hostOrderStream;
 
    ret = OrderLaunch::GetInstance(deviceLogicId).GetOrderStream(group, StreamType::STREAM_TYPE_ONLINE,
        hostOrderStream);
    EXPECT_NE(ret, HCCL_SUCCESS);
}

TEST_F(AicpuUnfold_UT, HostCheckSetAclGraphZeroCopyModeZero_FALSE)
{
    ZeroCopyAclGraph aclgraphcpy;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    OpParam opParam;
    opParam.isZeroCopy = true;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclAlg *algo;
    std::string algName = "test";

    EXPECT_EQ(aclgraphcpy.SetAclGraphZeroCopyMode(DevType::DEV_TYPE_910_93, opType, opParam, algo, 0), false);
}

TEST_F(AicpuUnfold_UT, HostCheckScratch_FALSE)
{
    ZeroCopyAclGraph aclgraphcpy;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_REDUCE_SCATTER;
    OpParam opParam;
    opParam.isZeroCopy = true;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    EXPECT_EQ(aclgraphcpy.IsScratchMemorySupportAclGraphZeroCopyMode(opParam, 2000, 1000), true);
    EXPECT_EQ(aclgraphcpy.IsScratchMemorySupportAclGraphZeroCopyMode(opParam, 200, 1000), false);
}

TEST_F(AicpuUnfold_UT, HostCheckSetGraphMode_TRUE)
{
    ZeroCopyAclGraph aclgraphcpy;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    opParam.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.DataDes.count = 64;
    AlgResourceResponse algResResponse;
    u32 usrRankSize = 1;
    HcclAlg *alg = NULL;
    MOCKER_CPP(&ZeroCopyAclGraph::IsAlgoSupportAclGraphZeroCopyMode).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(aclgraphcpy.SetGraphMode(opType, opParam, alg, 1024), true);
}

class HcclAlgMockNull : public HcclAlg {
public:
    HcclAlgMockNull(
        CCLBufferManager &cclBufferManager, const HcclDispatcher dispatcher, const HcclDispatcher vDispatcher)
        : HcclAlg(cclBufferManager, dispatcher, vDispatcher)
    {}
    std::unique_ptr<CollAlgOperator> GetAlgOperator(const HcclCMDType &opType, HcclWorkflowMode workflowMode)
    {
       return nullptr;
    }
};

class HcclAlgMock : public HcclAlg {
public:
    HcclAlgMock(CCLBufferManager &cclBufferManager, const HcclDispatcher dispatcher, const HcclDispatcher vDispatcher)
        : HcclAlg(cclBufferManager, dispatcher, vDispatcher)
    {}
    std::unique_ptr<CollAlgOperator> GetAlgOperator(const HcclCMDType &opType, HcclWorkflowMode workflowMode)
    {
       HcclAlgoAttr algoAttr;
       HcclTopoAttr topoAttr;

       AlgConfigurator *algConfigurator = new AlgConfigurator(algoAttr, topoAttr);

       CCLBufferManager cclBufferManager;
       HcclDispatcher dispatcher;
       HcclCommParams param;
       RankTable_t rankTable;
       param.deviceType = DevType::DEV_TYPE_910;
       std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

       TestConstructParam(param, rankTable);
       implBase->Init(param, rankTable);
       std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
       std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
       MOCKER_CPP(&CollAlgOperator::SetTopoAttr).stubs().with(any());
       MOCKER_CPP(&CollAlgOperator::SetAlgoAttr).stubs().with(any());
       MOCKER_CPP(&AlgConfigurator::GetAlgTypeDirect).stubs().with(any());
       MOCKER_CPP(&AlgConfigurator::GetAlgoLevel1DefaultSwitch).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
       MOCKER_CPP(&AlgConfigurator::GetTopoType).stubs().with(any());
       std::unique_ptr<CollAlgOperator> algOperator(
           new CollAlgOperator(algConfigurator, cclBufferManager, dispatcher, topoMatcher, opType));
       delete algConfigurator;
       return algOperator;
    }
};

TEST_F(AicpuUnfold_UT, HostCheckIsAlgoSupportAclGraphZeroCopyMode_GetAlgoFALSE)
{
    ZeroCopyAclGraph aclgraphcpy;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    opParam.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.DataDes.count = 64;
    AlgResourceResponse algResResponse;
    u32 usrRankSize = 1;

    std::unique_ptr<HcclCommunicator> algo;

    CCLBufferManager cclBufferManager;
    HcclDispatcher dispatcher = nullptr;
    HcclDispatcher vDispatcher = nullptr;

    HcclAlg *alg = new HcclAlgMockNull(cclBufferManager, dispatcher, vDispatcher);
    EXPECT_EQ(aclgraphcpy.IsAlgoSupportAclGraphZeroCopyMode(opType, opParam, alg, 1024), false);
    delete alg;
}

TEST_F(AicpuUnfold_UT, HostCheckIsAlgoSupportAclGraphZeroCopyMode_GetAlgoTRUE)
{
    ZeroCopyAclGraph aclgraphcpy;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    opParam.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.DataDes.count = 64;
    AlgResourceResponse algResResponse;
    u32 usrRankSize = 1;

    std::unique_ptr<HcclCommunicator> algo;

    CCLBufferManager cclBufferManager;
    HcclDispatcher dispatcher = nullptr;
    HcclDispatcher vDispatcher = nullptr;

    HcclAlg *alg = new HcclAlgMock(cclBufferManager, dispatcher, vDispatcher);
    MOCKER_CPP(&ZeroCopyAclGraph::AlgoCheck).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(aclgraphcpy.IsAlgoSupportAclGraphZeroCopyMode(opType, opParam, alg, 1024), false);
    delete alg;
}

HcclResult OpCalcResRequestStub(
    CollAlgOperator *This, const std::string &algName, const OpParam &param, AlgResourceRequest &resourceRequest)
{
    resourceRequest.scratchMemSize = 200;
    return HCCL_SUCCESS;
}

HcclResult SelectAlgStub(
    CollAlgOperator *op, const std::string &tag, const OpParam &param, std::string &algName, std::string &newTag)
{
    return HCCL_SUCCESS;
}

class CollAlgOperatorMock : public CollAlgOperator {
public:
    CollAlgOperatorMock(AlgConfigurator *algConfigurator, CCLBufferManager &cclBufferManager, HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher, HcclCMDType opType)
        : CollAlgOperator(algConfigurator, cclBufferManager, dispatcher, topoMatcher, opType){};
    HcclResult SelectAlgStub(const std::string &tag, const OpParam &param, std::string &algName, std::string &newTag)
    {
       return HCCL_SUCCESS;
    }

    HcclResult CalcResRequest(const std::string &algName, const OpParam &param, AlgResourceRequest &resourceRequest)
    {
       resourceRequest.scratchMemSize = 200;
       return HCCL_SUCCESS;
    }
};

TEST_F(AicpuUnfold_UT, HostCheckIsAlgoSupportAclGraphZeroCopyMode_AlgoChecker)
{
    ZeroCopyAclGraph aclgraphcpy;
    HcclCMDType opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    OpParam opParam;
    opParam.aicpuUnfoldMode = true;
    opParam.opType = HcclCMDType::HCCL_CMD_ALLREDUCE;
    opParam.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    opParam.DataDes.count = 64;
    AlgResourceResponse algResResponse;
    u32 usrRankSize = 1;

    std::unique_ptr<HcclCommunicator> algo;
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;

    AlgConfigurator *algConfigurator = new AlgConfigurator(algoAttr, topoAttr);

    CCLBufferManager cclBufferManager;
    HcclDispatcher dispatcher;
    HcclCommParams param;
    RankTable_t rankTable;
    param.deviceType = DevType::DEV_TYPE_910;
    std::unique_ptr<HcclCommunicator> implBase(new (std::nothrow) HcclCommunicator());

    TestConstructParam(param, rankTable);
    implBase->Init(param, rankTable);
    std::unique_ptr<hcclImpl> &impl = implBase->implAlg_->pimpl_;
    std::unique_ptr<TopoMatcher> &topoMatcher = implBase->implAlg_->topoMatcher_;
    MOCKER_CPP(&CollAlgOperator::SetTopoAttr).stubs().with(any());
    MOCKER_CPP(&CollAlgOperator::SetAlgoAttr).stubs().with(any());
    MOCKER_CPP(&AlgConfigurator::GetAlgTypeDirect).stubs().with(any());
    MOCKER_CPP(&AlgConfigurator::GetAlgoLevel1DefaultSwitch).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AlgConfigurator::GetTopoType).stubs().with(any());

    std::unique_ptr<CollAlgOperator> algOperator(
        new CollAlgOperatorMock(algConfigurator, cclBufferManager, dispatcher, topoMatcher, opType));

    EXPECT_EQ(aclgraphcpy.AlgoCheck(opParam, algOperator, 1024), true);
    delete algConfigurator;
}

TEST_F(AicpuUnfold_UT, AicpuReportRetryErr)
{
    hccl::HcclCommAicpu *hcclCommAicpu = new hccl::HcclCommAicpu;
    hcclCommAicpu->dumpDebug_ = false;
    hcclCommAicpu->dispatcher_ = nullptr;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub3));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    HcclOpExecFSM state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_STOPPED;
    KfcError errorCode = KfcError::kNone;
    uint32_t retryCnt;
    OpParam param;
    uint32_t beginSqePos = INVALID_UINT;
    uint32_t endSqePos = INVALID_UINT;
    std::string algName = "test";
    param.DataDes.dataType = HcclDataType::HCCL_DATA_TYPE_FP32;
    u8 inplaceSupportRetry = 0; // 不支持inplace的通信算子重执行
    u8 retryEnable = 1;
    u8 inPlaceSupportRetryStatus = 0;
    u8 isInplacePreSync = 0;
    u8 isPostSync = 0;
    param.isInplaceError = false;

    // 不支持Inplace算子导致的重执行失败
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    MOCKER_CPP(&HcclCommAicpu::SupportRetryWithInplaceCheck).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), false);
    EXPECT_EQ(param.isInplaceError, true);
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(false));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_E_OPRETRY_FAIL);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    GlobalMockObject::verify();

    state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_WAIT_RETRY;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    KfcCommand lastCmd = KfcCommand::kNone;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub11));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, lastCmd),
            HCCL_E_OPRETRY_FAIL);
    EXPECT_EQ(errorCode, KfcError::kExit);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    GlobalMockObject::verify();

    state = HcclOpExecFSM::HCCL_OP_EXEC_FSM_WAIT_RETRY;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    lastCmd = KfcCommand::kNone;
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub12));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmWaitRetryProcess(param, state, errorCode, lastCmd),
            HCCL_SUCCESS);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_STOP_LAUNCH);
    GlobalMockObject::verify();

    // 算子不一致导致的重执行失败
    inplaceSupportRetry = 1;
    retryEnable = 1;
    param.isInplaceError = false;
    hcclCommAicpu->PrepareOpRetryHandler(inplaceSupportRetry, retryEnable,
        inPlaceSupportRetryStatus, isInplacePreSync, isPostSync);
    MOCKER_CPP(&HcclCommAicpu::HcclOpCheckSupportRetry).stubs().with(any()).will(returnValue(true));
    EXPECT_EQ(hcclCommAicpu->HcclOpSupportRetry(algName, static_cast<bool>(retryEnable), param), true);
    EXPECT_EQ(param.isInplaceError, false);
    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub11));
    MOCKER_CPP(&HcclCommAicpu::HcclOpSupportRetry).stubs().with(any()).will(returnValue(true));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppedProcess(state, errorCode, retryCnt, algName, param, beginSqePos, endSqePos),
              HCCL_E_OPRETRY_FAIL);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExit);

    MOCKER_CPP(&AicpuHdc::GetOpExecCtrlCmd).stubs().with(any()).will(invoke(GetOpExecCtrlCmdStub11));
    MOCKER_CPP(&HcclCommAicpu::SendTaskExceptionByMBox).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(hcclCommAicpu->HcclOpExecFsmStoppingProcess(param, state, errorCode, retryCnt),
              HCCL_E_OPRETRY_FAIL);
    EXPECT_EQ(state, HcclOpExecFSM::HCCL_OP_EXEC_FSM_ERROR);
    EXPECT_EQ(errorCode, KfcError::kExit);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, ReportCqeException)
{
    HcclCommAicpu * hcclCommAicpu = new HcclCommAicpu;
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());
    hcclCommAicpu->opNotifies_.push_back(std::make_shared<LocalNotify>());

    MOCKER(&drvQueryProcessHostPid).stubs().will(invoke(drvQueryProcessHostPidStub));
    MOCKER(&getpid).stubs().will(returnValue(0));
    MOCKER_CPP(&LocalNotify::GetNotifyData).stubs().will(returnValue(HCCL_SUCCESS));

    hcclCommAicpu->SendTaskExceptionByMBox(0);
    hcclCommAicpu->userStreamId_ = -1;
    hcclCommAicpu->SendTaskExceptionByMBox(0);

    MOCKER(&halEschedSubmitEvent).stubs().will(returnValue(1));
    hcclCommAicpu->SendTaskExceptionByMBox(0);
    GlobalMockObject::verify();
    MOCKER(&drvQueryProcessHostPid).stubs().will(returnValue(1));
    MOCKER(&getpid).stubs().will(returnValue(0));
    MOCKER_CPP(&LocalNotify::GetNotifyData).stubs().will(returnValue(HCCL_SUCCESS));
    hcclCommAicpu->SendTaskExceptionByMBox(0);
    hcclCommAicpu->SendTaskExceptionByMBox(TS_ERROR_RETRY_CONSTRAINT);
    GlobalMockObject::verify();

    delete hcclCommAicpu;
}

TEST_F(AicpuUnfold_UT, st_aicpu_getcomm)
{
    std::string group = "hccl_world_group";
    hccl::HcclCommAicpu* hcclCommAicpu;
    HcclResult ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_NE(hcclCommAicpu, nullptr);

    // 获取不存在的group
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup("test");
    EXPECT_EQ(hcclCommAicpu, nullptr);

    // 获取被占用的通信域
    hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(group);
    EXPECT_EQ(hcclCommAicpu, nullptr);

    AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
    AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
}

TEST_F(AicpuUnfold_UT, st_log_aicpuCoreExhausted)
{
    int64_t aicpuCoreNum = 6; // AICPU核总数
    MOCKER(hrtHalGetDeviceInfo).stubs().with(any(), any(), any(), outBoundP(&aicpuCoreNum)).will(returnValue(HCCL_SUCCESS));
    vector<string> groups = {"group1", "group2", "group3", "group4", "group5", "group6", "group7"};

    for (string& group : groups) {
        hccl::HcclCommAicpu* hcclCommAicpu;
        HcclResult ret = AicpuHcclProcess::AicpuCreateCommbyGroup(group, &hcclCommAicpu);
        EXPECT_EQ(ret, HCCL_SUCCESS);

        hcclCommAicpu->identifier_ = group;
        hcclCommAicpu->commOpenStatus = true;

        memset_s(hcclCommAicpu->excuteOpId_.tag, sizeof(hcclCommAicpu->excuteOpId_.tag), 0,
            sizeof(hcclCommAicpu->excuteOpId_.tag));
        memset_s(hcclCommAicpu->excuteOpId_.newTag, sizeof(hcclCommAicpu->excuteOpId_.newTag), 0,
            sizeof(hcclCommAicpu->excuteOpId_.newTag));
        hcclCommAicpu->excuteOpId_.index = 0;
    }

    for (auto i = 0; i < aicpuCoreNum && i < groups.size(); ++i) {
        hccl::HcclCommAicpu* hcclCommAicpu = AicpuHcclProcess::AicpuGetCommbyGroup(groups[i]);
        EXPECT_NE(hcclCommAicpu, nullptr);
        if (i + 1 == aicpuCoreNum) {
            hcclCommAicpu->PrintAicpuCommExecStatus();
        }
    }

    for (string& group : groups) {
        AicpuHcclProcess::AicpuReleaseCommbyGroup(group);
        AicpuHcclProcess::AicpuDestoryCommbyGroup(group);
    }
}

// 覆盖率用例
TEST_F(AicpuUnfold_UT, HcclProcess_WaitAsyncFlagSuccess)
{
    hccl::Transport::Buffer localFlagBufforCheck[3];
    uint32_t checkValue = 1;
    localFlagBufforCheck[0].addr = static_cast<void*>(&checkValue);
    localFlagBufforCheck[1].addr = static_cast<void*>(&checkValue);
    localFlagBufforCheck[2].addr = static_cast<void*>(&checkValue);
    AicpuHcclProcess::WaitAsyncFlag(localFlagBufforCheck,1,2);
}
 
TEST_F(AicpuUnfold_UT, HcclProcess_WaitAsyncFlagFalse)
{
    hccl::Transport::Buffer localFlagBufforCheck[3];
    uint32_t checkValue = 2;
    localFlagBufforCheck[0].addr = static_cast<void*>(&checkValue);
    localFlagBufforCheck[1].addr = static_cast<void*>(&checkValue);
    localFlagBufforCheck[2].addr = static_cast<void*>(&checkValue);
    AicpuHcclProcess::WaitAsyncFlag(localFlagBufforCheck,1,2);
}
 
TEST_F(AicpuUnfold_UT, HcclProcess_InitAsyncFlag)
{
    uint32_t localFlagAddr[6];
    uint32_t remoteFlagAddr[6];
    uint32_t* lFlagAddr = reinterpret_cast<uint32_t *>(localFlagAddr);
    uint32_t* rFlagAddr = reinterpret_cast<uint32_t *>(remoteFlagAddr);
    hccl::Transport::Buffer localFlagBufforCheck[3];
    hccl::Transport::Buffer localFlagBufforWrite[3];
    hccl::Transport::Buffer remoteFlagBuf[3];
    AicpuHcclProcess::InitAsyncFlag(lFlagAddr, rFlagAddr, localFlagBufforCheck, localFlagBufforWrite, remoteFlagBuf);
    HcclQpInfoV2 qpInfo;
    qpInfo.qpPtr = 0;
    const u32 lKey = 512;
    const u32 rKey = 512;
    HcclAicpuUtils::PostSend(lKey,rKey,qpInfo,remoteFlagBuf[0],localFlagBufforWrite[0], true);
}
 
TEST_F(AicpuUnfold_UT, AiCpu_RunTransportRoceTxRx_Success)
{
    MOCKER(AicpuHcclProcess::InitAsyncFlag).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAicpuUtils::PostSend, HcclResult(const u32, const u32, const struct HcclQpInfoV2&,
        const struct hccl::Transport::Buffer&, const struct hccl::Transport::Buffer&, const bool))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(AicpuHcclProcess::WaitAsyncFlag).stubs().will(returnValue(HCCL_SUCCESS));
    struct PostSendTaskParam {
        // For DataCopy
        u32 lKey;
        u32 rKey;
        HcclQpInfoV2 qpInfo;
        u64 remoteAddr;
        u64 localAddr;
        u64 dataSize;
        u64 timeOut;
        // For Flag
        u64 localFlagAddr;
        u64 remoteFlagAddr;
        u32 lfKey;
        u32 rfKey;
    };
    struct PostSendTaskParam apiParam;
    apiParam.lKey = 512;
    apiParam.rKey = 512;
    HcclQpInfoV2 qpInfo;
    qpInfo.qpPtr = 0;
    apiParam.qpInfo = qpInfo;
    u64 remoteData = 0;
    u64 localData = 0;
    u32 localFlagAddr[6];
    u32 remoteFlagAddr[6];
    apiParam.remoteAddr = reinterpret_cast<u64>(&remoteData);
    apiParam.localAddr = reinterpret_cast<u64>(&localData);
    apiParam.dataSize = 1;
    apiParam.timeOut = 2;
    apiParam.localFlagAddr = reinterpret_cast<u64>(localFlagAddr);
    apiParam.remoteFlagAddr = reinterpret_cast<u64>(remoteFlagAddr);
    apiParam.lfKey = 512;
    apiParam.rfKey = 512;
    EXPECT_EQ(RunTransportRoceTx(&apiParam), HCCL_SUCCESS);
    EXPECT_EQ(RunTransportRoceRx(&apiParam), HCCL_SUCCESS);
}
 
TEST_F(AicpuUnfold_UT, AiCpu_RunTransportRoceTxRx_False)
{
    MOCKER(AicpuHcclProcess::InitAsyncFlag).stubs().will(returnValue(HCCL_E_PTR));
    MOCKER(HcclAicpuUtils::PostSend, HcclResult(const u32, const u32, const struct HcclQpInfoV2&,
        const struct hccl::Transport::Buffer&, const struct hccl::Transport::Buffer&, const bool))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(AicpuHcclProcess::WaitAsyncFlag).stubs().will(returnValue(HCCL_SUCCESS));
    struct PostSendTaskParam {
        // For DataCopy
        u32 lKey;
        u32 rKey;
        HcclQpInfoV2 qpInfo;
        u64 remoteAddr;
        u64 localAddr;
        u64 dataSize;
        u64 timeOut;
        // For Flag
        u64 localFlagAddr;
        u64 remoteFlagAddr;
        u32 lfKey;
        u32 rfKey;
    };
    struct PostSendTaskParam apiParam;
    apiParam.lKey = 512;
    apiParam.rKey = 512;
    HcclQpInfoV2 qpInfo;
    qpInfo.qpPtr = 0;
    apiParam.qpInfo = qpInfo;
    u64 remoteData = 0;
    u64 localData = 0;
    u32 localFlagAddr[6];
    u32 remoteFlagAddr[6];
    apiParam.remoteAddr = reinterpret_cast<u64>(&remoteData);
    apiParam.localAddr = reinterpret_cast<u64>(&localData);
    apiParam.dataSize = 1;
    apiParam.timeOut = 2;
    apiParam.localFlagAddr = reinterpret_cast<u64>(localFlagAddr);
    apiParam.remoteFlagAddr = reinterpret_cast<u64>(remoteFlagAddr);
    apiParam.lfKey = 512;
    apiParam.rfKey = 512;
    RunTransportRoceTx(&apiParam);
    RunTransportRoceRx(&apiParam);
}
 
TEST_F(AicpuUnfold_UT, AiCpu_RunTransportRoceTxRx_False2)
{
    MOCKER(AicpuHcclProcess::InitAsyncFlag).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAicpuUtils::PostSend, HcclResult(const u32, const u32, const struct HcclQpInfoV2&,
        const struct hccl::Transport::Buffer&, const struct hccl::Transport::Buffer&, const bool))
        .stubs()
        .will(returnValue(HCCL_E_PTR));
    MOCKER(AicpuHcclProcess::WaitAsyncFlag).stubs().will(returnValue(HCCL_SUCCESS));
    struct PostSendTaskParam {
        // For DataCopy
        u32 lKey;
        u32 rKey;
        HcclQpInfoV2 qpInfo;
        u64 remoteAddr;
        u64 localAddr;
        u64 dataSize;
        u64 timeOut;
        // For Flag
        u64 localFlagAddr;
        u64 remoteFlagAddr;
        u32 lfKey;
        u32 rfKey;
    };
    struct PostSendTaskParam apiParam;
    apiParam.lKey = 512;
    apiParam.rKey = 512;
    HcclQpInfoV2 qpInfo;
    qpInfo.qpPtr = 0;
    apiParam.qpInfo = qpInfo;
    u64 remoteData = 0;
    u64 localData = 0;
    u32 localFlagAddr[6];
    u32 remoteFlagAddr[6];
    apiParam.remoteAddr = reinterpret_cast<u64>(&remoteData);
    apiParam.localAddr = reinterpret_cast<u64>(&localData);
    apiParam.dataSize = 1;
    apiParam.timeOut = 2;
    apiParam.localFlagAddr = reinterpret_cast<u64>(localFlagAddr);
    apiParam.remoteFlagAddr = reinterpret_cast<u64>(remoteFlagAddr);
    apiParam.lfKey = 512;
    apiParam.rfKey = 512;
    RunTransportRoceTx(&apiParam);
    RunTransportRoceRx(&apiParam);
}
 
TEST_F(AicpuUnfold_UT, AiCpu_RunTransportRoceTxRx_False3)
{
    MOCKER(AicpuHcclProcess::InitAsyncFlag).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclAicpuUtils::PostSend, HcclResult(const u32, const u32, const struct HcclQpInfoV2&,
        const struct hccl::Transport::Buffer&, const struct hccl::Transport::Buffer&, const bool))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(AicpuHcclProcess::WaitAsyncFlag).stubs().will(returnValue(HCCL_E_PTR));
    struct PostSendTaskParam {
        // For DataCopy
        u32 lKey;
        u32 rKey;
        HcclQpInfoV2 qpInfo;
        u64 remoteAddr;
        u64 localAddr;
        u64 dataSize;
        u64 timeOut;
        // For Flag
        u64 localFlagAddr;
        u64 remoteFlagAddr;
        u32 lfKey;
        u32 rfKey;
    };
    struct PostSendTaskParam apiParam;
    apiParam.lKey = 512;
    apiParam.rKey = 512;
    HcclQpInfoV2 qpInfo;
    qpInfo.qpPtr = 0;
    apiParam.qpInfo = qpInfo;
    u64 remoteData = 0;
    u64 localData = 0;
    u32 localFlagAddr[6];
    u32 remoteFlagAddr[6];
    apiParam.remoteAddr = reinterpret_cast<u64>(&remoteData);
    apiParam.localAddr = reinterpret_cast<u64>(&localData);
    apiParam.dataSize = 1;
    apiParam.timeOut = 2;
    apiParam.localFlagAddr = reinterpret_cast<u64>(localFlagAddr);
    apiParam.remoteFlagAddr = reinterpret_cast<u64>(remoteFlagAddr);
    apiParam.lfKey = 512;
    apiParam.rfKey = 512;
    RunTransportRoceTx(&apiParam);
    RunTransportRoceRx(&apiParam);
}