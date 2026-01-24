/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAlgBase类的头文件
 * Author: hulinkang
 * Create: 2025-08-21
 */
 
// 算法层CcuContext基类，包含V2版本方法的实现
#ifndef HCCLV2_CCU_CONTEXT_ALG_BASE_H_
#define HCCLV2_CCU_CONTEXT_ALG_BASE_H_
 
#include <vector>
#include <ios>
#include "log.h"
#include "ccu_ctx.h"
#include "ccu_datatype.h"
 
namespace Hccl {
 
class CcuContextAlgBase : public CcuContext {
public:
    CcuContextAlgBase(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                      const CcuTransportGroup &group) : CcuContext(arg, transports, group) {}
    ~CcuContextAlgBase() override {}
 
    // 子类实现
    void                  Algorithm() override = 0 ;
    std::vector<uint64_t> GeneArgs(const CcuTaskArg &arg) override = 0;
 
protected:
    struct GroupOpSizeV2 {
        CcuRep::Variable baseIterNum;
        CcuRep::Variable tailLoopId;
        CcuRep::Variable tail;
    };
 
    GroupOpSizeV2 CreateGroupOpSizeV2()
    {
        return GroupOpSizeV2{CreateVariable(), CreateVariable(), CreateVariable()};
    }
 
    void GroupBroadcastV2(const std::vector<CcuTransport*> &transports, const std::vector<CcuRep::Memory> &dst,
        const CcuRep::Memory &src, const GroupOpSizeV2 &goSize) const;
    void GroupReduceV2(const std::vector<CcuTransport*> &transports, const CcuRep::Memory& dst,
        const std::vector<CcuRep::Memory>& src, const GroupOpSizeV2 &goSize,
        DataType dataType, DataType outputDataType, ReduceOp reduceType) const;
};
} // namespace Hccl
 
#endif // HCCLV2_CCU_CONTEXT_ALG_BASE_H_