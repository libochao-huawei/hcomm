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

#include <exception>
#include <string>

#include "ccu_types.h"

namespace ccu {
// 跨资源类共享的占位 tag：表示构造时不调用底层 Alloc。
// 仅供工厂函数（如 ccu::GetResByChannel）以及 ccu::Array<T> 通过 friend 关系使用。
struct NoAllocTag {};
// 统一异常类型。所有 wrapper 类（Variable / Address / Event / ...）发生底层 C API
// 失败时，必须抛 CcuException 而不是字符串字面量，便于用户用
// `catch (const std::exception&)` 或 `catch (const ccu::CcuException&)` 捕获，
// 并能拿到原始 CcuResult 错误码。
class CcuException : public std::exception {
public:
    CcuException(CcuResult code, const char *what)
        : code_(code), what_(BuildMessage(code, what)) {}

    const char *what() const noexcept override { return what_.c_str(); }
    CcuResult   code() const noexcept { return code_; }

private:
    static std::string BuildMessage(CcuResult code, const char *what)
    {
        std::string msg = "[ccu] ";
        msg += (what != nullptr) ? what : "(unknown)";
        msg += " (CcuResult=";
        msg += std::to_string(static_cast<int>(code));
        msg += ")";
        return msg;
    }

    CcuResult   code_;
    std::string what_;
};

#define CCU_THROW_IF_FAILED(ret, msg)                                       \
    do {                                                                    \
        auto _ccu_ret = (ret);                                              \
        if (_ccu_ret != CcuResult::CCU_SUCCESS) {                           \
            throw ::ccu::CcuException(_ccu_ret, (msg));                     \
        }                                                                   \
    } while (0)

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
        // 默认通用模板：未特化的 (lhsT, rhsT) 组合属于编译期允许、运行期非法。
        // 抛 CcuException 而非字符串字面量，可被 `catch (const std::exception&)` 捕获。
        throw ::ccu::CcuException(CcuResult::CCU_E_PARA,
            "CcuArithmeticOperator: invalid operand types");
    }

    CcuArithmeticOperatorType type{CcuArithmeticOperatorType::INVALID};
};

#endif // CCU_DATA_UTILS_HPP