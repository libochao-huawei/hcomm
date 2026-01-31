/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAlgBase类的实现
 * Author: hulinkang
 * Create: 2025-08-21
 */
 
#include <vector>
 
#include "ccu_assist.h"
#include "ccu_loopcall.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
#include "ccu_context_alg_base.h"
 
namespace Hccl {

void CcuContextAlgBase::GroupBroadcastV2(const std::vector<CcuTransport*> &transports, const std::vector<CcuRep::Memory> &dst,
                                         const CcuRep::Memory &src, const GroupOpSizeV2 &goSize) const
{
    (void)transports;
    (void)dst;
    (void)src;
    (void)goSize;
    THROW<NotSupportException>("Unimplemented Interface: %s", __func__);
}

void CcuContextAlgBase::GroupReduceV2(const std::vector<CcuTransport*> &transports, const CcuRep::Memory& dst,
                                      const std::vector<CcuRep::Memory>& src, const GroupOpSizeV2 &goSize,
                                      DataType dataType, DataType outputDataType, ReduceOp reduceType) const
{
    (void)transports;
    (void)dst;
    (void)src;
    (void)goSize;
    (void)dataType;
    (void)outputDataType;
    (void)reduceType;
    THROW<NotSupportException>("Unimplemented Interface: %s", __func__);
}
}