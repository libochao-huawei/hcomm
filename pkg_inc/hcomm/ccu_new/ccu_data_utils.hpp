/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_DATA_UTILS_HPP
#define CCU_DATA_UTILS_HPP

namespace ccu {
// 跨资源类共享的占位 tag：表示构造时不调用底层 Alloc。
// 仅供工厂函数（如 ccu::CreateByChannel）以及 ccu::Array<T> 通过 friend 关系使用。
struct NoAllocTag {};
} // namespace ccu

template <typename lhsT, typename rhsT> class CcuOperator {
public:
    CcuOperator(lhsT lhs, rhsT rhs) : lhs(lhs), rhs(rhs)
    {
    }

    lhsT lhs;
    rhsT rhs;
};

enum class CcuArithmeticOperatorType { ADDITION, INVALID };

template <typename lhsT, typename rhsT> class CcuArithmeticOperator : public CcuOperator<lhsT, rhsT> {
public:
    CcuArithmeticOperator(lhsT lhs, rhsT rhs, CcuArithmeticOperatorType type)
        : CcuOperator<lhsT, rhsT>(lhs, rhs), type(type)
    {
        Check();
    }

    void Check() const
    {
        // todo: 需要处理错误处理
        // Hccl::THROW<Hccl::CcuApiException>("Invalid Arithmetic Operator");
        throw "Invalid Arithmetic Operator";
    }

    CcuArithmeticOperatorType type{CcuArithmeticOperatorType::INVALID};
};

#endif // CCU_DATA_UTILS_HPP