/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_operator_check.h"
#include "exception_util.h"
#include "not_support_exception.h"
#include "adapter_error_manager_pub.h"

namespace Hccl {

void ReportOpCheckFailed(const std::string &paraName, const std::string &localPara, const std::string &remotePara)
{
    // 上报故障码EI0005
    RPT_INPUT_ERR(true, "EI0005", std::vector<std::string>({"para_name", "local_para", "remote_para"}),
                            std::vector<std::string>({paraName, localPara, remotePara}));
    THROW<InvalidParamsException>(StringFormat(
        "[RankConsistentImpl][CompareFrame][%s]op information %s check fail. "
        "local[%s], remote[%s]", __func__, paraName.c_str(), localPara.c_str(), remotePara.c_str()));
}

void ReportOpCheckFailed(const std::string &paraName, uint32_t localPara, uint32_t remotePara)
{
    // 上报故障码EI0005
    RPT_INPUT_ERR(true, "EI0005", std::vector<std::string>({"para_name", "local_para", "remote_para"}),
                            std::vector<std::string>({paraName, std::to_string(localPara), std::to_string(remotePara)}));
    THROW<InvalidParamsException>(StringFormat(
        "[RankConsistentImpl][CompareFrame][%s]op information %s check fail. "
        "local[%u], remote[%u]", __func__, paraName.c_str(), localPara, remotePara));
}

void CompareDataDesOp(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    if (localOpData.dataDes.dataCount != remoteOpData.dataDes.dataCount) {
        ReportOpCheckFailed("dataDes.dataCount", localOpData.dataDes.dataCount, remoteOpData.dataDes.dataCount);
    }

    if (localOpData.dataDes.dataType != remoteOpData.dataDes.dataType) {
        ReportOpCheckFailed("dataDes.dataType", localOpData.dataDes.dataType.Describe(),
                            remoteOpData.dataDes.dataType.Describe());
    }

    if (localOpData.dataDes.strideCount != remoteOpData.dataDes.strideCount) {
        ReportOpCheckFailed("dataDes.strideCount", localOpData.dataDes.strideCount, remoteOpData.dataDes.strideCount);
    }
}

void CompareVDataDesOp(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    if (localOpData.vDataDes.dataType != remoteOpData.vDataDes.dataType) {
        ReportOpCheckFailed("vDataDes.dataType", localOpData.vDataDes.dataType.Describe(),
                            remoteOpData.vDataDes.dataType.Describe());
    }
}

void CompareAlltoAllOp(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    if (localOpData.all2AllDataDes.sendType != remoteOpData.all2AllDataDes.recvType) {
        ReportOpCheckFailed("all2AllDataDes.sendType", localOpData.all2AllDataDes.sendType.Describe(),
                            remoteOpData.all2AllDataDes.recvType.Describe());
    }

    if (localOpData.all2AllDataDes.recvType != remoteOpData.all2AllDataDes.sendType) {
        ReportOpCheckFailed("all2AllDataDes.recvType", localOpData.all2AllDataDes.recvType.Describe(),
                            remoteOpData.all2AllDataDes.sendType.Describe());
    }

    if (localOpData.all2AllDataDes.sendCount != remoteOpData.all2AllDataDes.recvCount) {
        ReportOpCheckFailed("all2AllDataDes.sendCount", localOpData.all2AllDataDes.sendCount,
                            remoteOpData.all2AllDataDes.recvCount);
    }

    if (localOpData.all2AllDataDes.recvCount != remoteOpData.all2AllDataDes.sendCount) {
        ReportOpCheckFailed("all2AllDataDes.recvCount", localOpData.all2AllDataDes.recvCount,
                            remoteOpData.all2AllDataDes.sendCount);
    }
}

void CompareAlltoAllVOp(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    if (localOpData.all2AllVDataDes.sendType != remoteOpData.all2AllVDataDes.recvType) {
        ReportOpCheckFailed("all2AllVDataDes.sendType", localOpData.all2AllVDataDes.sendType.Describe(),
                            remoteOpData.all2AllVDataDes.recvType.Describe());
    }

    if (localOpData.all2AllVDataDes.recvType != remoteOpData.all2AllVDataDes.sendType) {
        ReportOpCheckFailed("all2AllVDataDes.recvType", localOpData.all2AllVDataDes.recvType.Describe(),
                            remoteOpData.all2AllVDataDes.sendType.Describe());
    }
}

void CompareAlltoAllVCOp(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    if (localOpData.all2AllVCDataDes.sendType != remoteOpData.all2AllVCDataDes.recvType) {
        ReportOpCheckFailed("all2AllVCDataDes.sendType", localOpData.all2AllVCDataDes.sendType.Describe(),
                            remoteOpData.all2AllVCDataDes.recvType.Describe());
    }
 
    if (localOpData.all2AllVCDataDes.recvType != remoteOpData.all2AllVCDataDes.sendType) {
        ReportOpCheckFailed("all2AllVCDataDes.recvType", localOpData.all2AllVCDataDes.recvType.Describe(),
                            remoteOpData.all2AllVCDataDes.sendType.Describe());
    }
}

void CompareNormalOp(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    if (localOpData.opMode != remoteOpData.opMode) {
        ReportOpCheckFailed("opMode", localOpData.opMode.Describe(), remoteOpData.opMode.Describe());
    }

    if (localOpData.opType == OpType::SEND) {
        if (remoteOpData.opType != OpType::RECV) {
            ReportOpCheckFailed("opType", localOpData.opType.Describe(), remoteOpData.opType.Describe());
        }
    } else if (localOpData.opType == OpType::RECV) {
        if (remoteOpData.opType != OpType::SEND) {
            ReportOpCheckFailed("opType", localOpData.opType.Describe(), remoteOpData.opType.Describe());
        }
    } else if (localOpData.opType != remoteOpData.opType) {
        ReportOpCheckFailed("opType", localOpData.opType.Describe(), remoteOpData.opType.Describe());
    }

    if (localOpData.reduceOp != remoteOpData.reduceOp) {
        ReportOpCheckFailed("reduceOp", localOpData.reduceOp.Describe(), remoteOpData.reduceOp.Describe());
    }

    if (localOpData.dataType != remoteOpData.dataType) {
        ReportOpCheckFailed("dataType", localOpData.dataType.Describe(), remoteOpData.dataType.Describe());
    }

    if (localOpData.opType != OpType::ALLGATHERV && localOpData.opType != OpType::REDUCESCATTERV) {
        if (localOpData.dataCount != remoteOpData.dataCount) {
            ReportOpCheckFailed("dataCount", localOpData.dataCount, remoteOpData.dataCount);
        }
    }

    if (localOpData.root != remoteOpData.root) {
        ReportOpCheckFailed("root", localOpData.root, remoteOpData.root);
    }

    if (localOpData.opType == OpType::SEND || localOpData.opType == OpType::RECV) {
        if (localOpData.myRank != remoteOpData.sendRecvRemoteRank) {
            ReportOpCheckFailed("sendRecvRemoteRank", localOpData.myRank, remoteOpData.sendRecvRemoteRank);
        }
    }

    if (localOpData.opTag !=  remoteOpData.opTag) {
        ReportOpCheckFailed("opTag", localOpData.opTag, remoteOpData.opTag);
    }

    if (localOpData.staticAddr != remoteOpData.staticAddr) {
        ReportOpCheckFailed("staticAddr",
                    static_cast<uint32_t>(localOpData.staticAddr), static_cast<uint32_t>(remoteOpData.staticAddr));
    }

    if (localOpData.staticShape != remoteOpData.staticShape) {
        ReportOpCheckFailed("staticShape",
                    static_cast<uint32_t>(localOpData.staticShape), static_cast<uint32_t>(remoteOpData.staticShape));
    }

    if (localOpData.outputDataType != remoteOpData.outputDataType) {
        ReportOpCheckFailed("outputDataType", localOpData.outputDataType.Describe(),
                            remoteOpData.outputDataType.Describe());
    }
}

/*
当前校验类型不支持vDataDes和all2AllVCDataDes相关内容；batchSendRecvDataDes中的itemNum字段没有校验的必要，双端该值可能不相等
*/
void CheckCollOperator(const CollOperator &localOpData, const CollOperator &remoteOpData)
{
    CompareNormalOp(localOpData, remoteOpData);

    if (localOpData.opType == OpType::BATCHSENDRECV) {
        return;
    }

    if (localOpData.opType == OpType::ALLTOALL) {
        CompareAlltoAllOp(localOpData, remoteOpData);
        return;
    }

    if (localOpData.opType == OpType::ALLTOALLV) {
        CompareAlltoAllVOp(localOpData, remoteOpData);
        return;
    }

    if (localOpData.opType == OpType::ALLTOALLVC) {
        CompareAlltoAllVCOp(localOpData, remoteOpData);
        return;
    }

    if (localOpData.opType == OpType::ALLGATHERV || localOpData.opType == OpType::REDUCESCATTERV) {
        CompareVDataDesOp(localOpData, remoteOpData);
        return;
    }

    CompareDataDesOp(localOpData, remoteOpData);
    return;
}
} // namesapce Hccl
