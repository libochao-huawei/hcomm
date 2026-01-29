/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: collective operator check header file
 * Create: 2025-06-07
 */
#ifndef HCCLV2_COLL_OPERATOR_CHECK_H
#define HCCLV2_COLL_OPERATOR_CHECK_H

#include <string>
#include "coll_operator.h"
namespace Hccl {

void ReportOpCheckFailed(const std::string &paraName, const std::string &localPara, const std::string &remotePara);

void ReportOpCheckFailed(const std::string &paraName, uint32_t localPara, uint32_t remotePara);

void CompareDataDesOp(const CollOperator &localOpData, const CollOperator &remoteOpData);

void CompareVDataDesOp(const CollOperator &localOpData, const CollOperator &remoteOpData);

void CompareAlltoAllOp(const CollOperator &localOpData, const CollOperator &remoteOpData);

void CompareAlltoAllVOp(const CollOperator &localOpData, const CollOperator &remoteOpData);

void CompareAlltoAllVCOp(const CollOperator &localOpData, const CollOperator &remoteOpData);

void CheckCollOperator(const CollOperator &localOpData, const CollOperator &remoteOpData);

} // namespace Hccl
#endif